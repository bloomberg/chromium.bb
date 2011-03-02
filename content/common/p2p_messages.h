// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// IPC messages for the P2P Transport API.

#ifndef CONTENT_COMMON_P2P_MESSAGES_H_
#define CONTENT_COMMON_P2P_MESSAGES_H_

#include "content/common/p2p_sockets.h"
#include "ipc/ipc_message_macros.h"

#define IPC_MESSAGE_START P2PMsgStart

// P2P Socket messages sent from the browser to the renderer.

IPC_MESSAGE_ROUTED2(P2PMsg_OnSocketCreated,
                    int /* socket_id */,
                    P2PSocketAddress /* socket_address */)

IPC_MESSAGE_ROUTED1(P2PMsg_OnError,
                    int /* socket_id */)

IPC_MESSAGE_ROUTED3(P2PMsg_OnDataReceived,
                    int /* socket_id */,
                    P2PSocketAddress /* socket_address */,
                    std::vector<char> /* data */)

// P2P Socket messages sent from the renderer to the browser.

IPC_MESSAGE_ROUTED3(P2PHostMsg_CreateSocket,
                    P2PSocketType /* type */,
                    int /* socket_id */,
                    P2PSocketAddress /* remote_address */)

// TODO(sergeyu): Use shared memory to pass the data.
IPC_MESSAGE_ROUTED3(P2PHostMsg_Send,
                    int /* socket_id */,
                    P2PSocketAddress /* socket_address */,
                    std::vector<char> /* data */)

IPC_MESSAGE_ROUTED1(P2PHostMsg_DestroySocket,
                     int /* socket_id */)

namespace IPC {

template <>
struct ParamTraits<P2PSocketAddress> {
  typedef P2PSocketAddress param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* p);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct SimilarTypeTraits<P2PSocketType> {
  typedef int Type;
};

}  // namespace IPC

#endif  // CONTENT_COMMON_P2P_MESSAGES_H_
