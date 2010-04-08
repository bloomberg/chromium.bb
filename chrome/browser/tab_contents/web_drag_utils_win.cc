// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_contents/web_drag_utils_win.h"

#include <oleidl.h>

using WebKit::WebDragOperationsMask;
using WebKit::WebDragOperationNone;
using WebKit::WebDragOperationCopy;
using WebKit::WebDragOperationLink;
using WebKit::WebDragOperationMove;

namespace web_drag_utils_win {

WebDragOperationsMask WinDragOpToWebDragOp(DWORD effect) {
  WebDragOperationsMask op = WebDragOperationNone;
  if (effect & DROPEFFECT_COPY)
    op = static_cast<WebDragOperationsMask>(op | WebDragOperationCopy);
  if (effect & DROPEFFECT_LINK)
    op = static_cast<WebDragOperationsMask>(op | WebDragOperationLink);
  if (effect & DROPEFFECT_MOVE)
    op = static_cast<WebDragOperationsMask>(op | WebDragOperationMove);
  return op;
}

DWORD WebDragOpToWinDragOp(WebDragOperationsMask op) {
  DWORD win_op = DROPEFFECT_NONE;
  if (op & WebDragOperationCopy)
    win_op |= DROPEFFECT_COPY;
  if (op & WebDragOperationLink)
    win_op |= DROPEFFECT_LINK;
  if (op & WebDragOperationMove)
    win_op |= DROPEFFECT_MOVE;
  return win_op;
}

}  // namespace web_drag_utils_win

