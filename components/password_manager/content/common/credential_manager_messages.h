// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Message definition file, included multiple times, hence no include guard.

#include <string>
#include <vector>

#include "base/strings/string16.h"
#include "components/password_manager/content/common/credential_manager_types.h"
#include "content/public/common/common_param_traits.h"
#include "content/public/common/common_param_traits_macros.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_message_utils.h"
#include "third_party/WebKit/public/platform/WebCredentialManagerError.h"
#include "url/gurl.h"

#define IPC_MESSAGE_START CredentialManagerMsgStart

IPC_ENUM_TRAITS_MAX_VALUE(blink::WebCredentialManagerError::ErrorType,
                          blink::WebCredentialManagerError::ErrorTypeLast);

IPC_ENUM_TRAITS_MAX_VALUE(password_manager::CredentialType,
                          password_manager::CREDENTIAL_TYPE_LAST);

IPC_STRUCT_TRAITS_BEGIN(password_manager::CredentialInfo)
  IPC_STRUCT_TRAITS_MEMBER(type)
  IPC_STRUCT_TRAITS_MEMBER(id)
  IPC_STRUCT_TRAITS_MEMBER(name)
  IPC_STRUCT_TRAITS_MEMBER(avatar)
  IPC_STRUCT_TRAITS_MEMBER(password)
  IPC_STRUCT_TRAITS_MEMBER(federation)
IPC_STRUCT_TRAITS_END()

// ----------------------------------------------------------------------------
// Messages sent from the renderer to the browser

// Passes the notification from 'navigator.credentials.notifyFailedSignIn()' up
// to the browser process in order to suppress the automatic bubble which would
// pop up in order to prompt the user to save the credential she used for
// signin. The browser process will respond with a
// CredentialManagerMsg_AcknowledgeFailedSignedIn message.
IPC_MESSAGE_ROUTED2(CredentialManagerHostMsg_NotifyFailedSignIn,
                    int /* request_id */,
                    password_manager::CredentialInfo /* credential */)

// Passes the notification from 'navigator.credentials.notifySignedIn()' up to
// the browser process in order to (among other things) prompt the user to save
// the credential she used for signin. The browser process will respond with a
// CredentialManagerMsg_AcknowledgeSignedIn message.
IPC_MESSAGE_ROUTED2(CredentialManagerHostMsg_NotifySignedIn,
                    int /* request_id */,
                    password_manager::CredentialInfo /* credential */)

// Passes the notification from 'navigator.credentials.notifySignedOut()' up to
// the browser process in order to clear the "zeroclick" bit on that origin's
// stored credentials. The browser process will respond with a
// CredentialManagerMsg_AcknowledgeSignedOut message.
IPC_MESSAGE_ROUTED1(CredentialManagerHostMsg_NotifySignedOut,
                    int /* request_id */)

// Requests a credential from the browser process in response to a page calling
// 'navigator.credentials.request()'. The browser process will respond with a
// CredentialManagerMsg_SendCredential message.
IPC_MESSAGE_ROUTED3(CredentialManagerHostMsg_RequestCredential,
                    int /* request_id */,
                    bool /* zero_click_only */,
                    std::vector<GURL> /* federations */)

// ----------------------------------------------------------------------------
// Messages sent from the browser to the renderer

// Notify the renderer that the browser process has finished processing a
// CredentialManagerHostMsg_NotifyFailedSignedIn message.
IPC_MESSAGE_ROUTED1(CredentialManagerMsg_AcknowledgeFailedSignIn,
                    int /* request_id */);

// Notify the renderer that the browser process has finished processing a
// CredentialManagerHostMsg_NotifySignedIn message.
IPC_MESSAGE_ROUTED1(CredentialManagerMsg_AcknowledgeSignedIn,
                    int /* request_id */);

// Notify the renderer that the browser process has finished processing a
// CredentialManagerHostMsg_NotifySignedOut message.
IPC_MESSAGE_ROUTED1(CredentialManagerMsg_AcknowledgeSignedOut,
                    int /* request_id */);

// Send a credential to the renderer in response to a
// CredentialManagerHostMsg_RequestCredential message.
IPC_MESSAGE_ROUTED2(CredentialManagerMsg_SendCredential,
                    int /* request_id */,
                    password_manager::CredentialInfo /* credential */)
