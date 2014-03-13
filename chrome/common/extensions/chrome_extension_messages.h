// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Chrome-specific IPC messages for extensions.
// Extension-related messages that aren't specific to Chrome live in
// extensions/common/extension_messages.h.
//
// Multiply-included message file, hence no include guard.

#include "chrome/common/web_application_info.h"
#include "ipc/ipc_message_macros.h"

#define IPC_MESSAGE_START ChromeExtensionMsgStart

IPC_STRUCT_TRAITS_BEGIN(WebApplicationInfo::IconInfo)
  IPC_STRUCT_TRAITS_MEMBER(url)
  IPC_STRUCT_TRAITS_MEMBER(width)
  IPC_STRUCT_TRAITS_MEMBER(height)
  IPC_STRUCT_TRAITS_MEMBER(data)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(WebApplicationInfo)
  IPC_STRUCT_TRAITS_MEMBER(title)
  IPC_STRUCT_TRAITS_MEMBER(description)
  IPC_STRUCT_TRAITS_MEMBER(app_url)
  IPC_STRUCT_TRAITS_MEMBER(icons)
IPC_STRUCT_TRAITS_END()

// Messages sent from the browser to the renderer.

// Requests application info for the page. The renderer responds back with
// ExtensionHostMsg_DidGetApplicationInfo.
IPC_MESSAGE_ROUTED1(ChromeExtensionMsg_GetApplicationInfo,
                    int32 /* page_id */)

// Messages sent from the renderer to the browser.

IPC_MESSAGE_ROUTED2(ChromeExtensionHostMsg_DidGetApplicationInfo,
                    int32 /* page_id */,
                    WebApplicationInfo)
