// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Multiply-included message file, no traditional include guard.
#include <vector>

#include "ipc/ipc_message.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_param_traits.h"
#include "third_party/skia/include/core/SkBitmap.h"

#define IPC_MESSAGE_START ImageMsgStart

// Messages sent from the browser to the renderer.

// Requests the renderer to download the specified image, decode it,
// and send the image data back via ImageHostMsg_DidDownloadImage.
IPC_MESSAGE_ROUTED4(ImageMsg_DownloadImage,
                    int /* identifier for the request */,
                    GURL /* URL of the image */,
                    bool /* is favicon (turn off cookies) */,
                    int /* Preferred image size. Passed on to
                           ImageHostMsg_DidDownloadFavicon, unused otherwise */)

// Messages sent from the renderer to the browser.

IPC_MESSAGE_ROUTED4(ImageHostMsg_DidDownloadImage,
                    int /* Identifier of the request */,
                    GURL /* URL of the image */,
                    int /* Preferred image size passed to
                           ImageMsg_DownloadImage */,
                    std::vector<SkBitmap> /* image_data */)
