// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PORT_BROWSER_RENDER_VIEW_HOST_DELEGATE_VIEW_H_
#define CONTENT_PORT_BROWSER_RENDER_VIEW_HOST_DELEGATE_VIEW_H_
#pragma once

#include "base/basictypes.h"
#include "content/common/content_export.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDragOperation.h"

class SkBitmap;
struct WebDropData;

namespace gfx {
class Point;
}

namespace content {

struct ContextMenuParams;

// This class provides a way for the RenderViewHost to reach out to its
// delegate's view. It only needs to be implemented by embedders if they don't
// use the default WebContentsView implementations.
class CONTENT_EXPORT RenderViewHostDelegateView {
 public:
  // A context menu should be shown, to be built using the context information
  // provided in the supplied params.
  virtual void ShowContextMenu(const ContextMenuParams& params) {}

  // The user started dragging content of the specified type within the
  // RenderView. Contextual information about the dragged content is supplied
  // by WebDropData.
  virtual void StartDragging(const WebDropData& drop_data,
                             WebKit::WebDragOperationsMask allowed_ops,
                             const SkBitmap& image,
                             const gfx::Point& image_offset) {}

  // The page wants to update the mouse cursor during a drag & drop operation.
  // |operation| describes the current operation (none, move, copy, link.)
  virtual void UpdateDragCursor(WebKit::WebDragOperation operation) {}

  // Notification that view for this delegate got the focus.
  virtual void GotFocus() {}

  // Callback to inform the browser that the page is returning the focus to
  // the browser's chrome. If reverse is true, it means the focus was
  // retrieved by doing a Shift-Tab.
  virtual void TakeFocus(bool reverse) {}

 protected:
  virtual ~RenderViewHostDelegateView() {}
};

}  // namespace content

#endif  // CONTENT_PORT_BROWSER_RENDER_VIEW_HOST_DELEGATE_VIEW_H_
