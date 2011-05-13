// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Multiply-included message file, no traditional include guard.
#include <string>

#include "chrome/common/cloud_print/cloud_print_proxy_info.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/ipc_message_macros.h"

#define IPC_MESSAGE_START ServiceMsgStart

IPC_STRUCT_TRAITS_BEGIN(cloud_print::CloudPrintProxyInfo)
  IPC_STRUCT_TRAITS_MEMBER(enabled)
  IPC_STRUCT_TRAITS_MEMBER(email)
  IPC_STRUCT_TRAITS_MEMBER(proxy_id)
IPC_STRUCT_TRAITS_END()

//-----------------------------------------------------------------------------
// Service process messages:
// These are messages from the browser to the service process.
// Tell the service process to enable the cloud proxy passing in the lsid
// of the account to be used.
IPC_MESSAGE_CONTROL1(ServiceMsg_EnableCloudPrintProxy,
                     std::string /* lsid */)

// Tell the service process to enable the cloud proxy passing in the OAuth2
// auth code of a robot account.
IPC_MESSAGE_CONTROL3(ServiceMsg_EnableCloudPrintProxyWithRobot,
                     std::string /* robot_auth_code */,
                     std::string /* robot_email*/,
                     std::string /* user_email*/)

// Tell the service process to disable the cloud proxy.
IPC_MESSAGE_CONTROL0(ServiceMsg_DisableCloudPrintProxy)

// Requests a message back on the current status of the cloud print proxy
// (whether it is enabled, the email address and the proxy id).
IPC_MESSAGE_CONTROL0(ServiceMsg_GetCloudPrintProxyInfo)

// Tell the service process to shutdown.
IPC_MESSAGE_CONTROL0(ServiceMsg_Shutdown)

// Tell the service process that an update is available.
IPC_MESSAGE_CONTROL0(ServiceMsg_UpdateAvailable)

//-----------------------------------------------------------------------------
// Service process host messages:
// These are messages from the service process to the browser.
// Sent when the cloud print proxy has an authentication error.
IPC_MESSAGE_CONTROL0(ServiceHostMsg_CloudPrintProxy_AuthError)

// Sent as a response to a request for cloud print proxy info
IPC_MESSAGE_CONTROL1(ServiceHostMsg_CloudPrintProxy_Info,
                     cloud_print::CloudPrintProxyInfo /* proxy info */)
