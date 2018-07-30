// Copyright (c) 2009-2014 The Bitcoin developers
// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2017 The LUX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "rpcserver.h"

#include "addrman.h"
#include "clientversion.h"
#include "main.h"
#include "net.h"
#include "netbase.h"
#include "protocol.h"
#include "sync.h"
#include "timedata.h"
#include "util.h"
#include "version.h"

#include <boost/foreach.hpp>

#include "univalue/univalue.h"

using namespace std;

UniValue getconnectioncount(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "getconnectioncount\n"
            "\nReturns the number of connections to other nodes.\n"
            "\nbResult:\n"
            "n          (numeric) The connection count\n"
            "\nExamples:\n" +
            HelpExampleCli("getconnectioncount", "") + HelpExampleRpc("getconnectioncount", ""));

    LOCK2(cs_main, cs_vNodes);
    return (int)vNodes.size();
}

UniValue ping(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "ping\n"
            "\nRequests that a ping be sent to all other nodes, to measure ping time.\n"
            "Results provided in getpeerinfo, pingtime and pingwait fields are decimal seconds.\n"
            "Ping command is handled in queue with all other commands, so it measures processing backlog, not just network ping.\n"
            "\nExamples:\n" +
            HelpExampleCli("ping", "") + HelpExampleRpc("ping", ""));

    // Request that each node send a ping during next message processing pass
    LOCK2(cs_main, cs_vNodes);
    BOOST_FOREACH (CNode* pNode, vNodes) {
        pNode->fPingQueued = true;
    }

    return NullUniValue;
}

UniValue destination(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() > 2)
        throw runtime_error(
                "destination [\"match|good|attempt|connect\"] [\"b32.i2p|base64|ip:port\"]\n"
                "\n Returns I2P destination details stored in your b32.i2p address manager lookup system.\n"
                "\nArguments:\n"
                "  If no arguments are provided, the command returns all the b32.i2p addresses. NOTE: Results will not include base64\n"
                "  1st argument = \"match\" then a 2nd argument is also required.\n"
                "  2nd argument = Any string. If a match is found in any of the address, source or base64 fields, that result will be returned.\n"
                "  1st argument = \"good\" destinations that has been tried, connected and found to be good will be returned.\n"
                "  1st argument = \"attempt\" destinations that have been attempted, will be returned.\n"
                "  1st argument = \"connect\" destinations that have been connected to in the past, will be returned.\n"
                "\nResults are returned as a json array of object(s).\n"
                "  The 1st result pair is the total size of the address hash map.\n"
                "  The 2nd result pair is the number of objects which follow, as matching this query.  It can be zero, if no match was found.\n"
                "\nResult:\n"
                "[\n"
                "  {\n"
                "    \"tablesize\": nnn,             (numeric) The total number of destinations in the i2p address book\n"
                "    \"matchsize\": nnn,             (numeric) The number of results returned, which matched your query\n"
                "  }\n"
                "  {\n"
                "    \"address\":\"b32.i2p\",          (string)  Base32 hash of a i2p destination, a possible peer\n"
                "    \"good\": true|false,           (boolean) Has this address been tried & found to be good\n"
                "    \"attempt\": nnn,               (numeric) The number of times it has been attempted\n"
                "    \"lasttry\": ttt,               (numeric) The time of a last attempted connection (memory only)\n"
                "    \"connect\": ttt,               (numeric) The time of a last successful connection\n"
                "    \"source\":\"b32.i2p|ip:port\",   (string)  The source of information about this address\n"
                "    \"base64\":\"destination\",       (string)  The full Base64 Public Key of this peers b32.i2p address\n"
                "  }\n"
                "  ,...\n"
                "]\n"
                "\nNOTE: The results obtained are only a snapshot, while you are connected to the network.\n"
                "      Peers are updating addresses & destinations all the time.\n"
                "\nExamples: Return all I2P destinations currently known about on the system.\n"
                + HelpExampleCli("destination", "")
                + HelpExampleRpc("destination", "") +
                "\nExamples: Return the I2P destinations marked as 'good', happens if they have been tried and a successful version handshake made.\n"
                + HelpExampleCli("destination", "good") +
                "\nExample: Return I2P destinations marked as having made an attempt to connect\n"
                + HelpExampleRpc("destination", "attempt") +
                "\nExample: Return I2P destinations which are marked as having been connected to.\n"
                + HelpExampleCli("destination", "connect") +
                "\nExamples: Return I2P destination entries which came from the 'source' IP address 215.49.103.xxx\n"
                + HelpExampleRpc("destination", "match 215.49.103") +
                "\nExamples: Return all I2P b32.i2p destinations which match the patter, these could be found in the 'source' or the 'address' fields.\n"
                + HelpExampleCli("destination", "match vatzduwjheyou3ybknfgm7cl43efbhovtrpfduz55uilxahxwt7a.b32.i2p")
                + HelpExampleRpc("destination", "match vatzduwjheyou3ybknfgm7cl43efbhovtrpfduz55uilxahxwt7a.b32.i2p")
        );
    //! We must not have node or main processing as Addrman needs to
    //! be considered static for the time required to process this.
    LOCK2(cs_main, cs_vNodes);

    bool fSelectedMatch = false;
    bool fMatchStr = false;
    bool fMatchTried = false;
    bool fMatchAttempt = false;
    bool fMatchConnect = false;
    bool fUnknownCmd = false;
    string sMatchStr;

    if( params.size() > 0 ) {                                   // Lookup the address and return the one object if found
        string sCmdStr = params[0].get_str();
        if( sCmdStr == "match" ) {
            if( params.size() > 1 ) {
                sMatchStr = params[1].get_str();
                fMatchStr = true;
            } else
                fUnknownCmd = true;
        } else if( sCmdStr == "good" )
            fMatchTried = true;
        else if( sCmdStr == "attempt" )
            fMatchAttempt = true;
        else if( sCmdStr == "connect")
            fMatchConnect = true;
        else
            fUnknownCmd = true;
        fSelectedMatch = true;
    }

    UniValue ret(UniValue::VARR);;
    // Load the vector with all the objects we have and return with
    // the total number of addresses we have on file
    vector<CDestinationStats> vecStats;
    int nTableSize = addrman.CopyDestinationStats(vecStats);
    if( !fUnknownCmd ) {       // If set, throw runtime error
        for( int i = 0; i < 2; i++ ) {      // Loop through the data twice
            bool fMatchFound = false;       // Assume no match
            int nMatchSize = 0;             // the match counter
            BOOST_FOREACH(const CDestinationStats& stats, vecStats) {
                if( fSelectedMatch ) {
                    if( fMatchStr ) {
                        if( stats.sAddress.find(sMatchStr) != string::npos ||
                            stats.sSource.find(sMatchStr) != string::npos ||
                            stats.sBase64.find(sMatchStr) != string::npos )
                            fMatchFound = true;
                    } else if( fMatchTried ) {
                        if( stats.fInTried ) fMatchFound = true;
                    }
                    else if( fMatchAttempt ) {
                        if( stats.nAttempts > 0 ) fMatchFound = true;
                    }
                    else if( fMatchConnect ) {
                        if( stats.nSuccessTime > 0 ) fMatchFound = true;
                    }
                } else          // Match everything
                    fMatchFound = true;

                if( i == 1 && fMatchFound ) {
                    UniValue obj(UniValue::VOBJ);;
                    obj.push_back(Pair("address", stats.sAddress));
                    obj.push_back(Pair("good", stats.fInTried));
                    obj.push_back(Pair("attempt", stats.nAttempts));
                    obj.push_back(Pair("lasttry", stats.nLastTry));
                    obj.push_back(Pair("connect", stats.nSuccessTime));
                    obj.push_back(Pair("source", stats.sSource));
                    //! Do to an RPC buffer limit of 65535 with stream output, we can not send these and ever get a result
                    //! This should be considered a short term fix ToDo:  Allocate bigger iostream buffer for the output
                    if( fSelectedMatch )
                        obj.push_back(Pair("base64", stats.sBase64));
                    ret.push_back(obj);
                }
                if( fMatchFound ) {
                    nMatchSize++;
                    fMatchFound = false;
                }
            }
            // The 1st time we get a count of the matches, so we can list that first in the results,
            // then we finally build the output objects, on the 2nd pass...and don't put this in there twice
            if( i == 0 ) {
                UniValue objSizes(UniValue::VOBJ);;
                objSizes.push_back(Pair("tablesize", nTableSize));
                objSizes.push_back(Pair("matchsize", nMatchSize));
                ret.push_back(objSizes);                            // This is the 1st object put on the Array
            }
        }
    } else
        throw JSONRPCError(RPC_INVALID_PARAMS, "Unknown subcommand or argument missing");

    return ret;
}

static void CopyNodeStats(std::vector<CNodeStats>& vstats)
{
    vstats.clear();

    LOCK(cs_vNodes);
    vstats.reserve(vNodes.size());
    BOOST_FOREACH (CNode* pnode, vNodes) {
        CNodeStats stats;
        pnode->copyStats(stats);
        vstats.push_back(stats);
    }
}

UniValue getpeerinfo(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "getpeerinfo\n"
            "\nReturns data about each connected network node as a json array of objects.\n"
            "\nbResult:\n"
            "[\n"
            "  {\n"
            "    \"id\": n,                   (numeric) Peer index\n"
            "    \"addr\":\"host:port\",      (string) The ip address and port of the peer\n"
            "    \"addrlocal\":\"ip:port\",   (string) local address\n"
            "    \"services\":\"xxxxxxxxxxxxxxxx\",   (string) The services offered\n"
            "    \"lastsend\": ttt,           (numeric) The time in seconds since epoch (Jan 1 1970 GMT) of the last send\n"
            "    \"lastrecv\": ttt,           (numeric) The time in seconds since epoch (Jan 1 1970 GMT) of the last receive\n"
            "    \"bytessent\": n,            (numeric) The total bytes sent\n"
            "    \"bytesrecv\": n,            (numeric) The total bytes received\n"
            "    \"conntime\": ttt,           (numeric) The connection time in seconds since epoch (Jan 1 1970 GMT)\n"
            "    \"pingtime\": n,             (numeric) ping time\n"
            "    \"pingwait\": n,             (numeric) ping wait\n"
            "    \"version\": v,              (numeric) The peer version, such as 7001\n"
            "    \"subver\": \"/Luxcore:x.x.x.x/\",  (string) The string version\n"
            "    \"inbound\": true|false,     (boolean) Inbound (true) or Outbound (false)\n"
            "    \"startingheight\": n,       (numeric) The starting height (block) of the peer\n"
            "    \"banscore\": n,             (numeric) The ban score\n"
            "    \"synced_headers\": n,       (numeric) The last header we have in common with this peer\n"
            "    \"synced_blocks\": n,        (numeric) The last block we have in common with this peer\n"
            "    \"inflight\": [\n"
            "       n,                        (numeric) The heights of blocks we're currently asking from this peer\n"
            "       ...\n"
            "    ]\n"
            "  }\n"
            "  ,...\n"
            "]\n"
            "\nExamples:\n" +
            HelpExampleCli("getpeerinfo", "") + HelpExampleRpc("getpeerinfo", ""));

    LOCK(cs_main);

    vector<CNodeStats> vstats;
    CopyNodeStats(vstats);

    UniValue ret(UniValue::VARR);

    BOOST_FOREACH (const CNodeStats& stats, vstats) {
        UniValue obj(UniValue::VOBJ);
        CNodeStateStats statestats;
        bool fStateStats = GetNodeStateStats(stats.nodeid, statestats);
        obj.push_back(Pair("id", stats.nodeid));
        obj.push_back(Pair("addr", stats.addrName));
        if (!(stats.addrLocal.empty()))
            obj.push_back(Pair("addrlocal", stats.addrLocal));
        obj.push_back(Pair("services", strprintf("%016x", stats.nServices)));
        obj.push_back(Pair("lastsend", stats.nLastSend));
        obj.push_back(Pair("lastrecv", stats.nLastRecv));
        obj.push_back(Pair("bytessent", stats.nSendBytes));
        obj.push_back(Pair("bytesrecv", stats.nRecvBytes));
        obj.push_back(Pair("conntime", stats.nTimeConnected));
        obj.push_back(Pair("pingtime", stats.dPingTime));
        if (stats.dPingWait > 0.0)
            obj.push_back(Pair("pingwait", stats.dPingWait));
        obj.push_back(Pair("version", stats.nVersion));
        // Use the sanitized form of subver here, to avoid tricksy remote peers from
        // corrupting or modifiying the JSON output by putting special characters in
        // their ver message.
        obj.push_back(Pair("subver", stats.cleanSubVer));
        obj.push_back(Pair("inbound", stats.fInbound));
        obj.push_back(Pair("startingheight", stats.nStartingHeight));
        if (fStateStats) {
            obj.push_back(Pair("banscore", statestats.nMisbehavior));
            obj.push_back(Pair("synced_headers", statestats.nSyncHeight));
            obj.push_back(Pair("synced_blocks", statestats.nCommonHeight));
            UniValue heights(UniValue::VARR);
            BOOST_FOREACH (int height, statestats.vHeightInFlight) {
                heights.push_back(height);
            }
            obj.push_back(Pair("inflight", heights));
        }
        obj.push_back(Pair("whitelisted", stats.fWhitelisted));

        ret.push_back(obj);
    }

    return ret;
}

UniValue addnode(const UniValue& params, bool fHelp)
{
    string strCommand;
    if (params.size() == 2)
        strCommand = params[1].get_str();
    if (fHelp || params.size() != 2 ||
        (strCommand != "onetry" && strCommand != "add" && strCommand != "remove"))
        throw runtime_error(
            "addnode \"b32.i2p|base64|ip:port|ipv6\" \"add|remove|onetry\"\n"
            "\nAttempts add or remove a node from the addnode list.\n"
            "Or try a connection to a node once.\n"
            "\nArguments:\n"
            "1. \"node\"     (string, required) The node (see getpeerinfo for nodes)\n"
            "2. \"command\"  (string, required) 'add' to add a node to the list, 'remove' to remove a node from the list, 'onetry' to try a connection to the node once\n"
            "\nExamples:\n"
             + HelpExampleCli("addnode", "\"192.168.0.6:26969\" \"onetry\"")
             + HelpExampleRpc("addnode", "\"192.168.0.6:26969\", \"add\"")
             + HelpExampleCli("addnode", "ibtfn3cnherivbkfbytay5tx35saajauxlg2aohna2rwyci2pecq.b32.i2p remove")
             + HelpExampleRpc("addnode", "\"ibtfn3cnherivbkfbytay5tx35saajauxlg2aohna2rwyci2pecq.b32.i2p\", \"onetry\""));
    string strNode = params[0].get_str();

    if (strCommand == "onetry") {
        CAddress addr;
        OpenNetworkConnection(addr, false, NULL, strNode.c_str());
        return NullUniValue;
    }

    LOCK(cs_vAddedNodes);
    vector<string>::iterator it = vAddedNodes.begin();
    for (; it != vAddedNodes.end(); it++)
        if (strNode == *it)
            break;

    if (strCommand == "add") {
        if (it != vAddedNodes.end())
            throw JSONRPCError(RPC_CLIENT_NODE_ALREADY_ADDED, "Error: Node already added");
        vAddedNodes.push_back(strNode);
    } else if (strCommand == "remove") {
        if (it == vAddedNodes.end())
            throw JSONRPCError(RPC_CLIENT_NODE_NOT_ADDED, "Error: Node has not been added.");
        vAddedNodes.erase(it);
    }

    return NullUniValue;
}

UniValue disconnectnode(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
                "disconnectnode \"node\" \n"
                "\nImmediately disconnects from the specified node.\n"
                "\nArguments:\n"
                "1. \"node\"     (string, required) The node (see getpeerinfo for nodes)\n"
                "\nExamples:\n"
                + HelpExampleCli("disconnectnode", "\"192.168.0.6:8333\"")
                + HelpExampleRpc("disconnectnode", "\"192.168.0.6:8333\"")
        );

    CNode* pNode = FindNode(params[0].get_str());
    if (pNode == NULL)
        throw JSONRPCError(RPC_CLIENT_NODE_NOT_CONNECTED, "Node not found in connected nodes");

    pNode->CloseSocketDisconnect();

    return NullUniValue;
}

UniValue getaddednodeinfo(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 2)
        throw runtime_error(
            "getaddednodeinfo dns ( \"node\" )\n"
            "\nReturns information about the given added node, or all added nodes\n"
            "(note that onetry addnodes are not listed here)\n"
            "If dns is false, only a list of added nodes will be provided,\n"
            "otherwise connected information will also be available.\n"
            "\nArguments:\n"
            "1. dns        (boolean, required) If false, only a list of added nodes will be provided, otherwise connected information will also be available.\n"
            "2. \"node\"   (string, optional) If provided, return information about this specific node, otherwise all nodes are returned.\n"
            "\nResult:\n"
            "[\n"
            "  {\n"
            "    \"addednode\" : \"192.168.0.201\",   (string) The node ip address\n"
            "    \"connected\" : true|false,          (boolean) If connected\n"
            "    \"addresses\" : [\n"
            "       {\n"
            "         \"address\" : \"192.168.0.201:26969\",  (string) The lux server host and port\n"
            "         \"connected\" : \"outbound\"           (string) connection, inbound or outbound\n"
            "       }\n"
            "     ]\n"
            "  }\n"
            "  ,...\n"
            "]\n"
            "\nExamples:\n"
            + HelpExampleCli("getaddednodeinfo", "true")
            + HelpExampleCli("getaddednodeinfo", "true \"192.168.0.201\"")
            + HelpExampleRpc("getaddednodeinfo", "true, \"192.168.0.201\""));

    std::vector<AddedNodeInfo> vInfo = GetAddedNodeInfo();

    if (params.size() == 2) {
        bool found = false;
        for (const AddedNodeInfo& info : vInfo) {
            if (info.strAddedNode == params[1].get_str()) {
                vInfo.assign(1, info);
                found = true;
                break;
            }
        }
        if (!found) {
            throw JSONRPCError(RPC_CLIENT_NODE_NOT_ADDED, "Error: Node has not been added.");
        }
    }

    UniValue ret(UniValue::VARR);

    for (const AddedNodeInfo& info : vInfo) {
        UniValue obj(UniValue::VOBJ);
        obj.push_back(Pair("addednode", info.strAddedNode));
        obj.push_back(Pair("connected", info.fConnected));
        UniValue addresses(UniValue::VARR);
        if (info.fConnected) {
            UniValue address(UniValue::VOBJ);
            address.push_back(Pair("address", info.resolvedAddress.ToString()));
            address.push_back(Pair("connected", info.fInbound ? "inbound" : "outbound"));
            addresses.push_back(address);
        }
        obj.push_back(Pair("addresses", addresses));
        ret.push_back(obj);
    }

    return ret;
}

UniValue getnettotals(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() > 0)
        throw runtime_error(
            "getnettotals\n"
            "\nReturns information about network traffic, including bytes in, bytes out,\n"
            "and current time.\n"
            "\nResult:\n"
            "{\n"
            "  \"totalbytesrecv\": n,   (numeric) Total bytes received\n"
            "  \"totalbytessent\": n,   (numeric) Total bytes sent\n"
            "  \"timemillis\": t        (numeric) Total cpu time\n"
            "}\n"
            "\nExamples:\n" +
            HelpExampleCli("getnettotals", "") + HelpExampleRpc("getnettotals", ""));

    UniValue obj(UniValue::VOBJ);
    obj.push_back(Pair("totalbytesrecv", CNode::GetTotalBytesRecv()));
    obj.push_back(Pair("totalbytessent", CNode::GetTotalBytesSent()));
    obj.push_back(Pair("timemillis", GetTimeMillis()));
    return obj;
}

UniValue switchnetwork(const UniValue& params, bool fHelp) {
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "switchnetwork\n"
            "Toggle all network activity temporarily.");

    SetNetworkActive(!fNetworkActive);

    return fNetworkActive;
}

static UniValue GetNetworksInfo()
{
    UniValue networks;
    for (int n = 0; n < NET_MAX; ++n) {
        enum Network network = static_cast<enum Network>(n);
        if (network == NET_UNROUTABLE)
            continue;
        proxyType proxy;
        UniValue obj(UniValue::VOBJ);
        GetProxy(network, proxy);
        obj.push_back(Pair("name", GetNetworkName(network)));
        obj.push_back(Pair("limited", IsLimited(network)));
        obj.push_back(Pair("reachable", IsReachable(network)));
        obj.push_back(Pair("proxy", proxy.IsValid() ? proxy.ToStringIPPort() : string()));
        networks.push_back(obj);
    }
    return networks;
}

UniValue getnetworkinfo(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "getnetworkinfo\n"
            "Returns an object containing various state info regarding P2P networking.\n"
            "\nResult:\n"
            "{\n"
            "  \"version\": xxxxx,                      (numeric) the server version\n"
            "  \"subversion\": \"/Luxcore:x.x.x.x/\",     (string) the server subversion string\n"
            "  \"protocolversion\": xxxxx,              (numeric) the protocol version\n"
            "  \"localservices\": \"xxxxxxxxxxxxxxxx\", (string) the services we offer to the network\n"
            "  \"timeoffset\": xxxxx,                   (numeric) the time offset\n"
            "  \"connections\": xxxxx,                  (numeric) the number of connections\n"
            "  \"networks\": [                          (array) information per network\n"
            "  {\n"
            "    \"name\": \"xxx\",                     (string) network (ipv4, ipv6 or onion)\n"
            "    \"limited\": true|false,               (boolean) is the network limited using -onlynet?\n"
            "    \"reachable\": true|false,             (boolean) is the network reachable?\n"
            "    \"proxy\": \"host:port\"               (string) the proxy that is used for this network, or empty if none\n"
            "  }\n"
            "  ,...\n"
            "  ],\n"
            "  \"relayfee\": x.xxxxxxxx,                (numeric) minimum relay fee for non-free transactions in lux/kb\n"
            "  \"localaddresses\": [                    (array) list of local addresses\n"
            "  {\n"
            "    \"address\": \"xxxx\",                 (string) network address\n"
            "    \"port\": xxx,                         (numeric) network port\n"
            "    \"score\": xxx                         (numeric) relative score\n"
            "  }\n"
            "  ,...\n"
            "  ]\n"
            "}\n"
            "\nExamples:\n" +
            HelpExampleCli("getnetworkinfo", "") + HelpExampleRpc("getnetworkinfo", ""));

    LOCK(cs_main);

    UniValue obj(UniValue::VOBJ);
    obj.push_back(Pair("version", CLIENT_VERSION));
    obj.push_back(Pair("subversion",
        FormatSubVersion(CLIENT_NAME, CLIENT_VERSION, std::vector<string>())));
    obj.push_back(Pair("protocolversion", PROTOCOL_VERSION));
    obj.push_back(Pair("localservices", strprintf("%016x", nLocalServices)));
    obj.push_back(Pair("timeoffset", GetTimeOffset()));
    obj.push_back(Pair("connections", (int)vNodes.size()));
    obj.push_back(Pair("networks", GetNetworksInfo()));
    obj.push_back(Pair("relayfee", ValueFromAmount(::minRelayTxFee.GetFeePerK())));
    UniValue localAddresses(UniValue::VARR);
    {
        LOCK(cs_mapLocalHost);
        BOOST_FOREACH (const PAIRTYPE(CNetAddr, LocalServiceInfo) & item, mapLocalHost) {
            UniValue rec(UniValue::VOBJ);
            rec.push_back(Pair("address", item.first.ToString()));
            rec.push_back(Pair("port", item.second.nPort));
            rec.push_back(Pair("score", item.second.nScore));
            localAddresses.push_back(rec);
        }
    }
    obj.push_back(Pair("localaddresses", localAddresses));
    return obj;
}

UniValue setban(const UniValue& params, bool fHelp)
{
    string strCommand;
    if (params.size() >= 2)
        strCommand = params[1].get_str();
    if (fHelp || params.size() < 2 ||
        (strCommand != "add" && strCommand != "remove"))
        throw runtime_error(
            "setban \"ip(/netmask)\" \"add|remove\" (bantime) (absolute)\n"
            "\nAttempts add or remove a IP/Subnet from the banned list.\n"

            "\nArguments:\n"
            "1. \"ip(/netmask)\" (string, required) The IP/Subnet (see getpeerinfo for nodes ip) with a optional netmask (default is /32 = single ip)\n"
            "2. \"command\"      (string, required) 'add' to add a IP/Subnet to the list, 'remove' to remove a IP/Subnet from the list\n"
            "3. \"bantime\"      (numeric, optional) time in seconds how long (or until when if [absolute] is set) the ip is banned (0 or empty means using the default time of 24h which can also be overwritten by the -bantime startup argument)\n"
            "4. \"absolute\"     (boolean, optional) If set, the bantime must be a absolute timestamp in seconds since epoch (Jan 1 1970 GMT)\n"

            "\nExamples:\n"
            + HelpExampleCli("setban", "\"192.168.0.6\" \"add\" 86400")
            + HelpExampleCli("setban", "\"192.168.0.0/24\" \"add\"")
            + HelpExampleRpc("setban", "\"192.168.0.6\", \"add\" 86400"));

    CSubNet subNet;
    CNetAddr netAddr;
    bool isSubnet = false;

    if (params[0].get_str().find("/") != string::npos)
        isSubnet = true;

    if (!isSubnet)
        netAddr = CNetAddr(params[0].get_str());
    else
        subNet = CSubNet(params[0].get_str());

    if (! (isSubnet ? subNet.IsValid() : netAddr.IsValid()) )
        throw JSONRPCError(RPC_CLIENT_NODE_ALREADY_ADDED, "Error: Invalid IP/Subnet");

    if (strCommand == "add")
    {
        if (isSubnet ? CNode::IsBanned(subNet) : CNode::IsBanned(netAddr))
            throw JSONRPCError(RPC_CLIENT_NODE_ALREADY_ADDED, "Error: IP/Subnet already banned");

        int64_t banTime = 0; //use standard bantime if not specified
        if (params.size() >= 3 && !params[2].isNull())
            banTime = params[2].get_int64();

        bool absolute = false;
        if (params.size() == 4)
            absolute = params[3].get_bool();

        isSubnet ? CNode::Ban(subNet, BanReasonManually, banTime, absolute) : CNode::Ban(netAddr, BanReasonManually, banTime, absolute);

        //disconnect possible nodes
        while(CNode *bannedNode = (isSubnet ? FindNode(subNet) : FindNode(netAddr)))
            bannedNode->CloseSocketDisconnect();
    }
    else if(strCommand == "remove")
    {
        if (!( isSubnet ? CNode::Unban(subNet) : CNode::Unban(netAddr) ))
            throw JSONRPCError(RPC_MISC_ERROR, "Error: Unban failed");
    }

    DumpBanlist(); //store banlist to disk
    return NullUniValue;
}

UniValue listbanned(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
                "listbanned\n"
                "\nList all banned IPs/Subnets.\n"
                "\nExamples:\n"
                + HelpExampleCli("listbanned", "")
                + HelpExampleRpc("listbanned", "")
        );

    banmap_t banMap;
    CNode::GetBanned(banMap);

    UniValue bannedAddresses(UniValue::VARR);
    for (banmap_t::iterator it = banMap.begin(); it != banMap.end(); it++)
    {
        CBanEntry banEntry = (*it).second;
        UniValue rec(UniValue::VOBJ);
        rec.push_back(Pair("address", (*it).first.ToString()));
        rec.push_back(Pair("banned_until", banEntry.nBanUntil));
        rec.push_back(Pair("ban_created", banEntry.nCreateTime));
        rec.push_back(Pair("ban_reason", banEntry.banReasonToString()));

        bannedAddresses.push_back(rec);
    }

    return bannedAddresses;
}

UniValue clearbanned(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "clearbanned\n"
            "\nClear all banned IPs.\n"

            "\nExamples:\n"
            + HelpExampleCli("clearbanned", "")
            + HelpExampleRpc("clearbanned", ""));

    CNode::ClearBanned();
    DumpBanlist(); //store banlist to disk

    return NullUniValue;
}
