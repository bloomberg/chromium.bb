// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_WEB_CONTENTS_VIEW_MAC_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_WEB_CONTENTS_VIEW_MAC_DELEGATE_H_
#pragma once

#import <Cocoa/Cocoa.h>

@protocol RenderWidgetHostViewMacDelegate;

namespace content {

class RenderWidgetHost;
class WebDragDestDelegate;
struct ContextMenuParams;

// This interface allows a client to extend the functionality of
// WebContentsViewMac.
class WebContentsViewMacDelegate {
 public:
  virtual ~WebContentsViewMacDelegate() {}

  // Returns a newly-created delegate for the RenderWidgetHostViewMac, to handle
  // events on the responder chain.
  virtual NSObject<RenderWidgetHostViewMacDelegate>*
      CreateRenderWidgetHostViewDelegate(
          RenderWidgetHost* render_widget_host) = 0;

  // Returns a delegate to process drags not handled by content.
  virtual WebDragDestDelegate* DragDelegate() = 0;

  // Shows a context menu.
  virtual void ShowContextMenu(const content::ContextMenuParams& params) = 0;

  // Notifications that the native view was created/destroyed.
  virtual void NativeViewCreated(NSView* view) = 0;
  virtual void NativeViewDestroyed(NSView* view) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_WEB_CONTENTS_VIEW_MAC_DELEGATE_H_
