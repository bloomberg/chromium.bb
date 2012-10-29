// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_CONTENTS_WEB_DRAG_UTILS_WIN_H_
#define CONTENT_BROWSER_WEB_CONTENTS_WEB_DRAG_UTILS_WIN_H_

#include "third_party/WebKit/Source/WebKit/chromium/public/WebDragOperation.h"

#include <windows.h>

namespace content {

WebKit::WebDragOperation WinDragOpToWebDragOp(DWORD effect);
WebKit::WebDragOperationsMask WinDragOpMaskToWebDragOpMask(DWORD effects);

DWORD WebDragOpToWinDragOp(WebKit::WebDragOperation op);
DWORD WebDragOpMaskToWinDragOpMask(WebKit::WebDragOperationsMask ops);

}  // namespace content

#endif  // CONTENT_BROWSER_WEB_CONTENTS_WEB_DRAG_UTILS_WIN_H_
