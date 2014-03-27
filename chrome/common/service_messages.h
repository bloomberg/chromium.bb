// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Multiply-included message file, no traditional include guard.
#include <string>
#include <vector>

#include "base/values.h"
#include "chrome/common/cloud_print/cloud_print_proxy_info.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/ipc_message_macros.h"

#define IPC_MESSAGE_START ServiceMsgStart

IPC_STRUCT_TRAITS_BEGIN(cloud_print::CloudPrintProxyInfo)
  IPC_STRUCT_TRAITS_MEMBER(enabled)
  IPC_STRUCT_TRAITS_MEMBER(email)
  IPC_STRUCT_TRAITS_MEMBER(proxy_id)
IPC_STRUCT_TRAITS_END()

// Tell the service process to enable the cloud proxy passing in the OAuth2
// auth code of a robot account.
IPC_MESSAGE_CONTROL4(ServiceMsg_EnableCloudPrintProxyWithRobot,
                     std::string /* robot_auth_code */,
                     std::string /* robot_email */,
                     std::string /* user_email */,
                     base::DictionaryValue /* user_settings */)

// Tell the service process to disable the cloud proxy.
IPC_MESSAGE_CONTROL0(ServiceMsg_DisableCloudPrintProxy)

// Requests a message back on the current status of the cloud print proxy
// (whether it is enabled, the email address and the proxy id).
IPC_MESSAGE_CONTROL0(ServiceMsg_GetCloudPrintProxyInfo)

// Requests a message back with serialized UMA histograms.
IPC_MESSAGE_CONTROL0(ServiceMsg_GetHistograms)

// Requests a message back with all available printers.
IPC_MESSAGE_CONTROL0(ServiceMsg_GetPrinters)

// Tell the service process to shutdown.
IPC_MESSAGE_CONTROL0(ServiceMsg_Shutdown)

// Tell the service process that an update is available.
IPC_MESSAGE_CONTROL0(ServiceMsg_UpdateAvailable)

//-----------------------------------------------------------------------------
// Service process host messages:
// These are messages from the service process to the browser.
// Sent as a response to a request for cloud print proxy info
IPC_MESSAGE_CONTROL1(ServiceHostMsg_CloudPrintProxy_Info,
                     cloud_print::CloudPrintProxyInfo /* proxy info */)

// Sent as a response to ServiceMsg_GetHistograms.
IPC_MESSAGE_CONTROL1(ServiceHostMsg_Histograms,
                     std::vector<std::string> /* pickled_histograms */)

// Sent as a response to ServiceMsg_GetPrinters.
IPC_MESSAGE_CONTROL1(ServiceHostMsg_Printers,
                     std::vector<std::string> /* printers */)
