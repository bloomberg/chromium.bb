// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_NS_VIEW_CLIENT_H_
#define CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_NS_VIEW_CLIENT_H_

#include "base/macros.h"

namespace content {

class RenderWidgetHostViewMac;

// The interface through which the NSView for a RenderWidgetHostViewMac is to
// communicate with the RenderWidgetHostViewMac (potentially in another
// process).
class RenderWidgetHostNSViewClient {
 public:
  RenderWidgetHostNSViewClient() {}
  virtual ~RenderWidgetHostNSViewClient() {}

  // TODO(ccameron): As with the GetRenderWidgetHostViewCocoa method on
  // RenderWidgetHostNSViewBridge, this method is to be removed.
  virtual RenderWidgetHostViewMac* GetRenderWidgetHostViewMac() = 0;

  // Indicates that the RenderWidgetHost is to shut down.
  virtual void OnNSViewRequestShutdown() = 0;

  // Indicates whether or not the NSView is its NSWindow's first responder.
  virtual void OnNSViewIsFirstResponderChanged(bool is_first_responder) = 0;

  // Indicates whether or not the NSView's NSWindow is key.
  virtual void OnNSViewWindowIsKeyChanged(bool is_key) = 0;

  // Indicates the NSView's bounds in its NSWindow's DIP coordinate system (with
  // the origin at the upper-left corner), and indicate if the the NSView is
  // attached to an NSWindow (if it is not, then |view_bounds_in_window_dip|'s
  // origin is meaningless, but its size is still relevant).
  virtual void OnNSViewBoundsInWindowChanged(
      const gfx::Rect& view_bounds_in_window_dip,
      bool attached_to_window) = 0;

  // Indicates the NSView's NSWindow's frame in the global display::Screen
  // DIP coordinate system (where the origin the upper-left corner of
  // Screen::GetPrimaryDisplay).
  virtual void OnNSViewWindowFrameInScreenChanged(
      const gfx::Rect& window_frame_in_screen_dip) = 0;

  // Indicate the NSView's NSScreen's properties.
  virtual void OnNSViewDisplayChanged(const display::Display& display) = 0;

  // Forward events to the renderer or the input router, as appropriate.
  virtual void OnNSViewRouteOrProcessMouseEvent(
      const blink::WebMouseEvent& web_event) = 0;
  virtual void OnNSViewRouteOrProcessWheelEvent(
      const blink::WebMouseWheelEvent& web_event) = 0;

  // Special case forwarding of synthetic events to the renderer.
  virtual void OnNSViewForwardMouseEvent(
      const blink::WebMouseEvent& web_event) = 0;
  virtual void OnNSViewForwardWheelEvent(
      const blink::WebMouseWheelEvent& web_event) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(RenderWidgetHostNSViewClient);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_NS_VIEW_CLIENT_H_
