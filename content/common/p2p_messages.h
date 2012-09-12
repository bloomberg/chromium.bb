// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// IPC messages for the P2P Transport API.
// Multiply-included message file, hence no include guard.

#include "content/common/content_export.h"
#include "content/common/p2p_sockets.h"
#include "ipc/ipc_message_macros.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_util.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT
#define IPC_MESSAGE_START P2PMsgStart

IPC_ENUM_TRAITS(content::P2PSocketType)

IPC_STRUCT_TRAITS_BEGIN(net::NetworkInterface)
  IPC_STRUCT_TRAITS_MEMBER(name)
  IPC_STRUCT_TRAITS_MEMBER(address)
IPC_STRUCT_TRAITS_END()

// P2P Socket messages sent from the browser to the renderer.

IPC_MESSAGE_CONTROL1(P2PMsg_NetworkListChanged,
                     net::NetworkInterfaceList /* networks */)

IPC_MESSAGE_CONTROL2(P2PMsg_GetHostAddressResult,
                     int32 /* request_id */,
                     net::IPAddressNumber /* address */)

IPC_MESSAGE_CONTROL2(P2PMsg_OnSocketCreated,
                     int /* socket_id */,
                     net::IPEndPoint /* socket_address */)

IPC_MESSAGE_CONTROL1(P2PMsg_OnError,
                     int /* socket_id */)

IPC_MESSAGE_CONTROL2(P2PMsg_OnIncomingTcpConnection,
                     int /* socket_id */,
                     net::IPEndPoint /* socket_address */)

IPC_MESSAGE_CONTROL3(P2PMsg_OnDataReceived,
                     int /* socket_id */,
                     net::IPEndPoint /* socket_address */,
                     std::vector<char> /* data */)

// P2P Socket messages sent from the renderer to the browser.

// Start/stop sending P2PMsg_NetworkListChanged messages when network
// configuration changes.
IPC_MESSAGE_CONTROL0(P2PHostMsg_StartNetworkNotifications)
IPC_MESSAGE_CONTROL0(P2PHostMsg_StopNetworkNotifications)

IPC_MESSAGE_CONTROL2(P2PHostMsg_GetHostAddress,
                    std::string /* host_name */,
                    int32 /* request_id */)

IPC_MESSAGE_CONTROL4(P2PHostMsg_CreateSocket,
                     content::P2PSocketType /* type */,
                     int /* socket_id */,
                     net::IPEndPoint /* local_address */,
                     net::IPEndPoint /* remote_address */)

IPC_MESSAGE_CONTROL3(P2PHostMsg_AcceptIncomingTcpConnection,
                    int /* listen_socket_id */,
                    net::IPEndPoint /* remote_address */,
                    int /* connected_socket_id */)

// TODO(sergeyu): Use shared memory to pass the data.
IPC_MESSAGE_CONTROL3(P2PHostMsg_Send,
                     int /* socket_id */,
                     net::IPEndPoint /* socket_address */,
                     std::vector<char> /* data */)

IPC_MESSAGE_CONTROL1(P2PHostMsg_DestroySocket,
                     int /* socket_id */)
