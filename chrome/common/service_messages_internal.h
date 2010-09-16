// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

// This header is meant to be included in multiple passes, hence no traditional
// header guard.
// See ipc_message_macros.h for explanation of the macros and passes.

// This file needs to be included again, even though we're actually included
// from it via utility_messages.h.
#include "ipc/ipc_message_macros.h"

//------------------------------------------------------------------------------
// Service process messages:
// These are messages from the browser to the service process.
IPC_BEGIN_MESSAGES(Service)

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

  // Queries whether the cloud print proxy is enabled.
  IPC_SYNC_MESSAGE_ROUTED0_2(ServiceMsg_IsCloudPrintProxyEnabled,
                             bool,       /* out: is_enabled*/
                             std::string /* out: Email address of account */
                                         /* used for Cloud Print, if enabled*/)

  // This message is for testing purpose.
  IPC_MESSAGE_CONTROL0(ServiceMsg_Hello)

  // This message is for enabling the remoting process.
  IPC_MESSAGE_CONTROL3(ServiceMsg_EnableRemotingWithTokens,
                       std::string, /* username */
                       std::string, /* Token for remoting */
                       std::string  /* Token for Google Talk */)

  // Tell the service process to shutdown.
  IPC_MESSAGE_CONTROL0(ServiceMsg_Shutdown)

IPC_END_MESSAGES(Service)

//------------------------------------------------------------------------------
// Service process host messages:
// These are messages from the service process to the browser.
IPC_BEGIN_MESSAGES(ServiceHost)

  // Sent when the cloud print proxy has an authentication error.
  IPC_MESSAGE_CONTROL0(ServiceHostMsg_CloudPrintProxy_AuthError)

  // Sent from the service process in response to a Hello message.
  IPC_MESSAGE_CONTROL0(ServiceHostMsg_GoodDay)

IPC_END_MESSAGES(ServiceHost)
