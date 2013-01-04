// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Multiply-included file, no traditional include guard.
#include "android_webview/common/aw_hit_test_data.h"
#include "content/public/common/common_param_traits.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_platform_file.h"

// Singly-included section for enums and custom IPC traits.
#ifndef ANDROID_WEBVIEW_COMMON_RENDER_VIEW_MESSAGES_H_
#define ANDROID_WEBVIEW_COMMON_RENDER_VIEW_MESSAGES_H_

namespace IPC {

// TODO - add enums and custom IPC traits here when needed.

}  // namespace IPC

#endif  // ANDROID_WEBVIEW_COMMON_RENDER_VIEW_MESSAGES_H_

IPC_STRUCT_TRAITS_BEGIN(android_webview::AwHitTestData)
  IPC_STRUCT_TRAITS_MEMBER(type)
  IPC_STRUCT_TRAITS_MEMBER(extra_data_for_type)
  IPC_STRUCT_TRAITS_MEMBER(href)
  IPC_STRUCT_TRAITS_MEMBER(anchor_text)
  IPC_STRUCT_TRAITS_MEMBER(img_src)
IPC_STRUCT_TRAITS_END()

#define IPC_MESSAGE_START AndroidWebViewMsgStart

//-----------------------------------------------------------------------------
// RenderView messages
// These are messages sent from the browser to the renderer process.

// Tells the renderer to drop all WebCore memory cache.
IPC_MESSAGE_CONTROL0(AwViewMsg_ClearCache)

// Request for the renderer to determine if the document contains any image
// elements.  The id should be passed in the response message so the response
// can be associated with the request.
IPC_MESSAGE_ROUTED1(AwViewMsg_DocumentHasImages,
                    int /* id */)

// Do hit test at the given webview coordinate. "Webview" coordinates are
// physical pixel values with the 0,0 at the top left of the current displayed
// view (ie 0,0 is not the top left of the page if the page is scrolled).
IPC_MESSAGE_ROUTED2(AwViewMsg_DoHitTest,
                    int /* view_x */,
                    int /* view_y */)

// Enables receiving pictures from the renderer on every new frame.
IPC_MESSAGE_ROUTED1(AwViewMsg_EnableCapturePictureCallback,
                    bool /* enable */)

//-----------------------------------------------------------------------------
// RenderView messages
// These are messages sent from the renderer to the browser process.

// Response to AwViewMsg_DocumentHasImages request.
IPC_MESSAGE_ROUTED2(AwViewHostMsg_DocumentHasImagesResponse,
                    int, /* id */
                    bool /* has_images */)

// Response to AwViewMsg_DoHitTest.
IPC_MESSAGE_ROUTED1(AwViewHostMsg_UpdateHitTestData,
                    android_webview::AwHitTestData)

// Notification that a new picture becomes available. It is only sent if
// AwViewMsg_EnableCapturePictureCallback was previously enabled.
IPC_MESSAGE_ROUTED0(AwViewHostMsg_PictureUpdated)
