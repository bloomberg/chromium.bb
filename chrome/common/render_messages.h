// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_RENDER_MESSAGES_H_
#define CHROME_COMMON_RENDER_MESSAGES_H_

#include <stdint.h>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/common/features.h"
#include "chrome/common/web_application_info.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/offline_pages/features/features.h"
#include "content/public/common/browser_controls_state.h"
#include "content/public/common/webplugininfo.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_platform_file.h"
#include "media/media_features.h"
#include "ppapi/features/features.h"
#include "third_party/WebKit/public/web/WebConsoleMessage.h"
#include "url/gurl.h"
#include "url/ipc/url_param_traits.h"
#include "url/origin.h"

// Singly-included section for enums and custom IPC traits.
#ifndef INTERNAL_CHROME_COMMON_RENDER_MESSAGES_H_
#define INTERNAL_CHROME_COMMON_RENDER_MESSAGES_H_


#endif  // INTERNAL_CHROME_COMMON_RENDER_MESSAGES_H_

#define IPC_MESSAGE_START ChromeMsgStart

IPC_ENUM_TRAITS_MAX_VALUE(blink::WebConsoleMessage::Level,
                          blink::WebConsoleMessage::kLevelLast)
IPC_ENUM_TRAITS_MAX_VALUE(content::BrowserControlsState,
                          content::BROWSER_CONTROLS_STATE_LAST)


IPC_ENUM_TRAITS_MAX_VALUE(WebApplicationInfo::MobileCapable,
                          WebApplicationInfo::MOBILE_CAPABLE_APPLE)

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
  IPC_STRUCT_TRAITS_MEMBER(mobile_capable)
IPC_STRUCT_TRAITS_END()

//-----------------------------------------------------------------------------
// RenderView messages
// These are messages sent from the browser to the renderer process.

// Tells the render frame to load all blocked plugins with the given identifier.
IPC_MESSAGE_ROUTED1(ChromeViewMsg_LoadBlockedPlugins,
                    std::string /* identifier */)

// Tells the renderer whether or not a file system access has been allowed.
IPC_MESSAGE_ROUTED2(ChromeViewMsg_RequestFileSystemAccessAsyncResponse,
                    int  /* request_id */,
                    bool /* allowed */)

// Notifies the renderer whether hiding/showing the browser controls is enabled,
// what the current state should be, and whether or not to animate to the
// proper state.
IPC_MESSAGE_ROUTED3(ChromeViewMsg_UpdateBrowserControlsState,
                    content::BrowserControlsState /* constraints */,
                    content::BrowserControlsState /* current */,
                    bool /* animate */)

// Requests application info for the frame. The renderer responds back with
// ChromeFrameHostMsg_DidGetWebApplicationInfo.
IPC_MESSAGE_ROUTED0(ChromeFrameMsg_GetWebApplicationInfo)

// JavaScript related messages -----------------------------------------------

// Tells the frame it is displaying an interstitial page.
IPC_MESSAGE_ROUTED0(ChromeViewMsg_SetAsInterstitial)

#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
// Message sent from the renderer to the browser to schedule to download the
// page at a later time.
IPC_MESSAGE_ROUTED0(ChromeViewHostMsg_DownloadPageLater)

// Message sent from the renderer to the browser to indicate if download button
// is being shown in error page.
IPC_MESSAGE_ROUTED1(ChromeViewHostMsg_SetIsShowingDownloadButtonInErrorPage,
                    bool /* showing download button */)
#endif

#if defined(OS_ANDROID)
// Sent when navigating to chrome://sandbox to install bindings onto the WebUI.
IPC_MESSAGE_ROUTED0(ChromeViewMsg_AddSandboxStatusExtension)
#endif  // defined(OS_ANDROID)

//-----------------------------------------------------------------------------
// Misc messages
// These are messages sent from the renderer to the browser process.

// Tells the browser that content in the current page was blocked due to the
// user's content settings.
IPC_MESSAGE_ROUTED2(ChromeViewHostMsg_ContentBlocked,
                    ContentSettingsType /* type of blocked content */,
                    base::string16 /* details on blocked content */)

// Sent by the renderer process to check whether access to web databases is
// granted by content settings.
IPC_SYNC_MESSAGE_CONTROL5_1(ChromeViewHostMsg_AllowDatabase,
                            int /* render_frame_id */,
                            GURL /* origin_url */,
                            GURL /* top origin url */,
                            base::string16 /* database name */,
                            base::string16 /* database display name */,
                            bool /* allowed */)

// Sent by the renderer process to check whether access to DOM Storage is
// granted by content settings.
IPC_SYNC_MESSAGE_CONTROL4_1(ChromeViewHostMsg_AllowDOMStorage,
                            int /* render_frame_id */,
                            GURL /* origin_url */,
                            GURL /* top origin url */,
                            bool /* if true local storage, otherwise session */,
                            bool /* allowed */)

// Sent by the renderer process to check whether access to FileSystem is
// granted by content settings.
IPC_SYNC_MESSAGE_CONTROL3_1(ChromeViewHostMsg_RequestFileSystemAccessSync,
                            int /* render_frame_id */,
                            GURL /* origin_url */,
                            GURL /* top origin url */,
                            bool /* allowed */)

// Sent by the renderer process to check whether access to FileSystem is
// granted by content settings.
IPC_MESSAGE_CONTROL4(ChromeViewHostMsg_RequestFileSystemAccessAsync,
                    int /* render_frame_id */,
                    int /* request_id */,
                    GURL /* origin_url */,
                    GURL /* top origin url */)

// Sent by the renderer process to check whether access to Indexed DBis
// granted by content settings.
IPC_SYNC_MESSAGE_CONTROL4_1(ChromeViewHostMsg_AllowIndexedDB,
                            int /* render_frame_id */,
                            GURL /* origin_url */,
                            GURL /* top origin url */,
                            base::string16 /* database name */,
                            bool /* allowed */)


#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
// Returns whether any internal plugin supporting |mime_type| is registered and
// enabled. Does not determine whether the plugin can actually be instantiated
// (e.g. whether it has all its dependencies).
// When the returned *|is_available| is true, |additional_param_names| and
// |additional_param_values| contain the name-value pairs, if any, specified
// for the *first* non-disabled plugin found that is registered for |mime_type|.
IPC_SYNC_MESSAGE_CONTROL1_3(
    ChromeViewHostMsg_IsInternalPluginAvailableForMimeType,
    std::string /* mime_type */,
    bool /* is_available */,
    std::vector<base::string16> /* additional_param_names */,
    std::vector<base::string16> /* additional_param_values */)
#endif

IPC_MESSAGE_ROUTED1(ChromeFrameHostMsg_DidGetWebApplicationInfo,
                    WebApplicationInfo)

#if BUILDFLAG(ENABLE_PLUGINS)
// Sent by the renderer to check if crash reporting is enabled.
IPC_SYNC_MESSAGE_CONTROL0_1(ChromeViewHostMsg_IsCrashReportingEnabled,
                            bool /* enabled */)
#endif

// Tells the browser to open a PDF file in a new tab. Used when no PDF Viewer is
// available, and user clicks to view PDF.
IPC_MESSAGE_ROUTED1(ChromeViewHostMsg_OpenPDF, GURL /* url */)

#endif  // CHROME_COMMON_RENDER_MESSAGES_H_
