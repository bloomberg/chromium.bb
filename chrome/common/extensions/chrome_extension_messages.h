// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Chrome-specific IPC messages for extensions.
// Extension-related messages that aren't specific to Chrome live in
// extensions/common/extension_messages.h.
//
// Multiply-included message file, hence no include guard.

#include <string>

#include "base/strings/string16.h"
#include "base/values.h"
#include "chrome/common/extensions/api/webstore/webstore_api_constants.h"
#include "chrome/common/extensions/webstore_install_result.h"
#include "chrome/common/web_application_info.h"
#include "extensions/common/stack_frame.h"
#include "ipc/ipc_message_macros.h"
#include "url/gurl.h"

#define IPC_MESSAGE_START ChromeExtensionMsgStart

IPC_ENUM_TRAITS_MAX_VALUE(extensions::api::webstore::InstallStage,
                          extensions::api::webstore::INSTALL_STAGE_INSTALLING)
IPC_ENUM_TRAITS_MAX_VALUE(extensions::webstore_install::Result,
                          extensions::webstore_install::RESULT_LAST)

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
IPC_MESSAGE_ROUTED0(ChromeExtensionMsg_GetApplicationInfo)

// Toggles visual muting of the render view area. This is on when a constrained
// window is showing.
IPC_MESSAGE_ROUTED1(ChromeViewMsg_SetVisuallyDeemphasized,
                    bool /* deemphazied */)

// Sent to the renderer if install stage updates were requested for an inline
// install.
IPC_MESSAGE_ROUTED1(ExtensionMsg_InlineInstallStageChanged,
                    extensions::api::webstore::InstallStage /* stage */)

// Sent to the renderer if download progress updates were requested for an
// inline install.
IPC_MESSAGE_ROUTED1(ExtensionMsg_InlineInstallDownloadProgress,
                    int /* percent_downloaded */)

// Send to renderer once the installation mentioned on
// ExtensionHostMsg_InlineWebstoreInstall is complete.
IPC_MESSAGE_ROUTED4(ExtensionMsg_InlineWebstoreInstallResponse,
                    int32 /* install id */,
                    bool /* whether the install was successful */,
                    std::string /* error */,
                    extensions::webstore_install::Result /* result */)

// Messages sent from the renderer to the browser.

// Sent by the renderer to check if a URL has permission to trigger a clipboard
// read/write operation from the DOM.
IPC_SYNC_MESSAGE_CONTROL1_1(ChromeViewHostMsg_CanTriggerClipboardRead,
                            GURL /* origin */,
                            bool /* allowed */)
IPC_SYNC_MESSAGE_CONTROL1_1(ChromeViewHostMsg_CanTriggerClipboardWrite,
                            GURL /* origin */,
                            bool /* allowed */)

// Sent by the renderer to implement chrome.webstore.install().
IPC_MESSAGE_ROUTED5(ExtensionHostMsg_InlineWebstoreInstall,
                    int32 /* install id */,
                    int32 /* return route id */,
                    std::string /* Web Store item ID */,
                    GURL /* requestor URL */,
                    int /* listeners_mask */)

IPC_MESSAGE_ROUTED1(ChromeExtensionHostMsg_DidGetApplicationInfo,
                    WebApplicationInfo)
