// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_NS_VIEW_CLIENT_H_
#define CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_NS_VIEW_CLIENT_H_

#include "base/macros.h"
#include "content/common/mac/attributed_string_coder.h"

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

  // Handling pinch gesture events.
  virtual void OnNSViewGestureBegin(blink::WebGestureEvent begin_event) = 0;
  virtual void OnNSViewGestureUpdate(blink::WebGestureEvent update_event) = 0;
  virtual void OnNSViewGestureEnd(blink::WebGestureEvent end_event) = 0;
  virtual void OnNSViewSmartMagnify(
      const blink::WebGestureEvent& smart_magnify_event) = 0;

  // Request an overlay dictionary be displayed for the text at the specified
  // point.
  virtual void OnNSViewLookUpDictionaryOverlayAtPoint(
      const gfx::PointF& root_point) = 0;

  // Request an overlay dictionary be displayed for the text in the the
  // specified character range.
  virtual void OnNSViewLookUpDictionaryOverlayFromRange(
      const gfx::Range& range) = 0;

  // Synchronously query the text input type from the
  virtual void OnNSViewSyncGetTextInputType(
      ui::TextInputType* text_input_type) = 0;

  // Synchronously query the current selection. If there exists a selection then
  // set |*has_selection| to true and return in |*selected_text| the selected
  // text. Otherwise set |*has_selection| to false.
  virtual void OnNSViewSyncGetSelectedText(bool* has_selection,
                                           base::string16* selected_text) = 0;

  // Synchronously query the character index for |root_point| and return it in
  // |*index|. Sets it to UINT32_MAX if the request fails or is not completed.
  virtual void OnNSViewSyncGetCharacterIndexAtPoint(
      const gfx::PointF& root_point,
      uint32_t* index) = 0;

  // Synchronously query the composition character boundary rectangle and return
  // it in |*rect|. Set |*actual_range| to the range actually used for the
  // returned rectangle.
  virtual void OnNSViewSyncGetFirstRectForRange(
      const gfx::Range& requested_range,
      gfx::Rect* rect,
      gfx::Range* actual_range) = 0;

  // Forward the corresponding edit menu command to the RenderWidgetHost's
  // delegate.
  virtual void OnNSViewExecuteEditCommand(const std::string& command) = 0;
  virtual void OnNSViewUndo() = 0;
  virtual void OnNSViewRedo() = 0;
  virtual void OnNSViewCut() = 0;
  virtual void OnNSViewCopy() = 0;
  virtual void OnNSViewCopyToFindPboard() = 0;
  virtual void OnNSViewPaste() = 0;
  virtual void OnNSViewPasteAndMatchStyle() = 0;
  virtual void OnNSViewSelectAll() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(RenderWidgetHostNSViewClient);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_NS_VIEW_CLIENT_H_
