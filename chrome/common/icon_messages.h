// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Multiply-included message file, no traditional include guard.
#include <vector>

#include "chrome/common/favicon_url.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_param_traits.h"
#include "third_party/skia/include/core/SkBitmap.h"

#define IPC_MESSAGE_START IconMsgStart

IPC_ENUM_TRAITS(FaviconURL::IconType)

IPC_STRUCT_TRAITS_BEGIN(FaviconURL)
  IPC_STRUCT_TRAITS_MEMBER(icon_url)
  IPC_STRUCT_TRAITS_MEMBER(icon_type)
IPC_STRUCT_TRAITS_END()

// Messages sent from the browser to the renderer.

// Requests the renderer to download the specified favicon image encode it as
// PNG and send the PNG data back ala IconHostMsg_DidDownloadFavicon.
IPC_MESSAGE_ROUTED3(IconMsg_DownloadFavicon,
                    int /* identifier for the request */,
                    GURL /* URL of the image */,
                    int /* Size of the image. Normally 0, but set if you have
                           a preferred image size to request, such as when
                           downloading the favicon */)

// Messages sent from the renderer to the browser.

// Notification that the urls for the favicon of a site has been determined.
IPC_MESSAGE_ROUTED2(IconHostMsg_UpdateFaviconURL,
                    int32 /* page_id */,
                    std::vector<FaviconURL> /* urls of the favicon */)

IPC_MESSAGE_ROUTED4(IconHostMsg_DidDownloadFavicon,
                    int /* Identifier of the request */,
                    GURL /* URL of the image */,
                    bool /* true if there was a network error */,
                    SkBitmap /* image_data */)
