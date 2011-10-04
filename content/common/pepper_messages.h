// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Multiply-included message file, no traditional include guard
#include "content/common/content_export.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_platform_file.h"
#include "ppapi/c/private/ppb_flash_net_connector.h"
#include "ppapi/proxy/ppapi_param_traits.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT
#define IPC_MESSAGE_START PepperMsgStart

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

IPC_SYNC_MESSAGE_CONTROL1_1(PepperMsg_GetLocalTimeZoneOffset,
                            base::Time /* t */,
                            double /* result */)
