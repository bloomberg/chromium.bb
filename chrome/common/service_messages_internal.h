// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "ipc/ipc_message_macros.h"

#include "chrome/common/remoting/chromoting_host_info.h"

#define IPC_MESSAGE_START ServiceMsgStart

//------------------------------------------------------------------------------
// Service process messages:
// These are messages from the browser to the service process.
// Tell the service process to enable the cloud proxy passing in the lsid
// of the account to be used.
IPC_MESSAGE_CONTROL1(ServiceMsg_EnableCloudPrintProxy,
                     std::string /* lsid */)
// Tell the service process to enable the cloud proxy passing in specific
// tokens to be used.
IPC_MESSAGE_CONTROL2(ServiceMsg_EnableCloudPrintProxyWithTokens,
                     std::string, /* token for cloudprint service */
                     std::string  /* token for Google Talk service */)
// Tell the service process to disable the cloud proxy.
IPC_MESSAGE_CONTROL0(ServiceMsg_DisableCloudPrintProxy)

// Requests a message back on whether the cloud print proxy is
// enabled.
IPC_MESSAGE_CONTROL0(ServiceMsg_IsCloudPrintProxyEnabled)

// This message is for testing purpose.
IPC_MESSAGE_CONTROL0(ServiceMsg_Hello)

// Set credentials used by the RemotingHost.
IPC_MESSAGE_CONTROL2(ServiceMsg_SetRemotingHostCredentials,
                     std::string, /* username */
                     std::string  /* token for XMPP */)

// Enabled remoting host.
IPC_MESSAGE_CONTROL0(ServiceMsg_EnableRemotingHost)

// Disable remoting host.
IPC_MESSAGE_CONTROL0(ServiceMsg_DisableRemotingHost)

// Get remoting host status information.
IPC_MESSAGE_CONTROL0(ServiceMsg_GetRemotingHostInfo)

// Tell the service process to shutdown.
IPC_MESSAGE_CONTROL0(ServiceMsg_Shutdown)

// Tell the service process that an update is available.
IPC_MESSAGE_CONTROL0(ServiceMsg_UpdateAvailable)

//------------------------------------------------------------------------------
// Service process host messages:
// These are messages from the service process to the browser.
// Sent when the cloud print proxy has an authentication error.
IPC_MESSAGE_CONTROL0(ServiceHostMsg_CloudPrintProxy_AuthError)

// Sent as a response to a request for enablement status.
IPC_MESSAGE_CONTROL2(ServiceHostMsg_CloudPrintProxy_IsEnabled,
                     bool,       /* Is the proxy enabled? */
                     std::string /* Email address of account */)

IPC_MESSAGE_CONTROL1(ServiceHostMsg_RemotingHost_HostInfo,
                     remoting::ChromotingHostInfo /* host_info */)

// Sent from the service process in response to a Hello message.
IPC_MESSAGE_CONTROL0(ServiceHostMsg_GoodDay)
