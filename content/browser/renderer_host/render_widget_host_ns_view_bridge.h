// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_NS_VIEW_BRIDGE_H_
#define CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_NS_VIEW_BRIDGE_H_

@class RenderWidgetHostViewCocoa;

#include <memory>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "third_party/WebKit/public/web/WebPopupType.h"
#include "third_party/skia/include/core/SkColor.h"

namespace gfx {
class Rect;
}

namespace content {

class RenderWidgetHostNSViewClient;

// The interface through which RenderWidgetHostViewMac is to manipulate its
// corresponding NSView (potentially in another process).
class RenderWidgetHostNSViewBridge {
 public:
  RenderWidgetHostNSViewBridge() {}
  virtual ~RenderWidgetHostNSViewBridge() {}

  static std::unique_ptr<RenderWidgetHostNSViewBridge> Create(
      std::unique_ptr<RenderWidgetHostNSViewClient> client);

  // TODO(ccameron): RenderWidgetHostViewMac and other functions currently use
  // this method to communicate directly with RenderWidgetHostViewCocoa. The
  // goal of this class is to eliminate this direct communication (so this
  // method is expected to go away).
  virtual RenderWidgetHostViewCocoa* GetRenderWidgetHostViewCocoa() = 0;

  // Initialize the window as a popup (e.g, date/time picker).
  virtual void InitAsPopup(const gfx::Rect& content_rect,
                           blink::WebPopupType popup_type) = 0;

  // Remove the NSView from the view heirarchy and destroy it. After this is
  // called, no calls back into the RenderWidgetHostNSViewClient may be made.
  virtual void Destroy() = 0;

  // Make the NSView be the first responder of its NSWindow.
  virtual void MakeFirstResponder() = 0;

  // Set the bounds of the NSView or its enclosing NSWindow (depending on the
  // window type).
  virtual void SetBounds(const gfx::Rect& rect) = 0;

  // Set the background color of the hosted CALayer.
  virtual void SetBackgroundColor(SkColor color) = 0;

  // Call the -[NSView setHidden:] method.
  virtual void SetVisible(bool visible) = 0;

  // Call the -[NSView setToolTipAtMousePoint] method.
  virtual void SetTooltipText(const base::string16& display_text) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(RenderWidgetHostNSViewBridge);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_NS_VIEW_BRIDGE_H_
