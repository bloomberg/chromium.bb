// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Multiply-included file, no traditional include guard.
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

#define IPC_MESSAGE_START AndroidWebViewMsgStart

//-----------------------------------------------------------------------------
// RenderView messages
// These are messages sent from the browser to the renderer process.

// Request for the renderer to determine if the document contains any image
// elements.  The id should be passed in the response message so the response
// can be associated with the request.
IPC_MESSAGE_ROUTED1(AwViewMsg_DocumentHasImages,
                    int /* id */)

//-----------------------------------------------------------------------------
// RenderView messages
// These are messages sent from the renderer to the browser process.

// Response to AwViewMsg_DocumentHasImages request.
IPC_MESSAGE_ROUTED2(AwViewHostMsg_DocumentHasImagesResponse,
                    int, /* id */
                    bool /* has_images */)

