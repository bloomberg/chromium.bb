// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_PEPPER_MESSAGES_H_
#define CHROME_COMMON_PEPPER_MESSAGES_H_
#pragma once

#include "chrome/common/common_param_traits.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_param_traits.h"
#include "ipc/ipc_platform_file.h"

#define IPC_MESSAGE_START PepperMsgStart

struct PP_Flash_NetAddress;

namespace IPC {

template <>
struct ParamTraits<PP_Flash_NetAddress> {
  typedef PP_Flash_NetAddress param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* p);
  static void Log(const param_type& p, std::string* l);
};

}  // namespace IPC

// Pepper (non-file-system) messages sent from the browser to the renderer.

// The response to PepperMsg_ConnectTcp(Address).
IPC_MESSAGE_ROUTED4(PepperMsg_ConnectTcpACK,
                    int /* request_id */,
                    IPC::PlatformFileForTransit /* socket */,
                    PP_Flash_NetAddress /* local_addr */,
                    PP_Flash_NetAddress /* remote_addr */)

// Pepper (non-file-system) messages sent from the renderer to the browser.

IPC_MESSAGE_CONTROL4(PepperMsg_ConnectTcp,
                     int /* routing_id */,
                     int /* request_id */,
                     std::string /* host */,
                     uint16 /* port */)

IPC_MESSAGE_CONTROL3(PepperMsg_ConnectTcpAddress,
                     int /* routing_id */,
                     int /* request_id */,
                     PP_Flash_NetAddress /* addr */)

#endif  // CHROME_COMMON_PEPPER_MESSAGES_H_
