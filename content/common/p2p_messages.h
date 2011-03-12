// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// IPC messages for the P2P Transport API.
// Multiply-included message file, hence no include guard.

#include "content/common/p2p_sockets.h"
#include "ipc/ipc_message_macros.h"
#include "net/base/ip_endpoint.h"

#define IPC_MESSAGE_START P2PMsgStart

IPC_ENUM_TRAITS(P2PSocketType)

// P2P Socket messages sent from the browser to the renderer.

IPC_MESSAGE_ROUTED2(P2PMsg_OnSocketCreated,
                    int /* socket_id */,
                    net::IPEndPoint /* socket_address */)

IPC_MESSAGE_ROUTED1(P2PMsg_OnError,
                    int /* socket_id */)

IPC_MESSAGE_ROUTED3(P2PMsg_OnDataReceived,
                    int /* socket_id */,
                    net::IPEndPoint /* socket_address */,
                    std::vector<char> /* data */)

// P2P Socket messages sent from the renderer to the browser.

IPC_MESSAGE_ROUTED3(P2PHostMsg_CreateSocket,
                    P2PSocketType /* type */,
                    int /* socket_id */,
                    net::IPEndPoint /* remote_address */)

// TODO(sergeyu): Use shared memory to pass the data.
IPC_MESSAGE_ROUTED3(P2PHostMsg_Send,
                    int /* socket_id */,
                    net::IPEndPoint /* socket_address */,
                    std::vector<char> /* data */)

IPC_MESSAGE_ROUTED1(P2PHostMsg_DestroySocket,
                    int /* socket_id */)
