// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PORT_BROWSER_RENDER_WIDGET_HOST_VIEW_PORT_H_
#define CONTENT_PORT_BROWSER_RENDER_WIDGET_HOST_VIEW_PORT_H_

#include "base/callback.h"
#include "base/process_util.h"
#include "base/string16.h"
#include "content/common/content_export.h"
#include "content/port/common/input_event_ack_state.h"
#include "content/public/browser/render_widget_host_view.h"
#include "ipc/ipc_listener.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPopupType.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebTextDirection.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/base/range/range.h"
#include "ui/surface/transport_dib.h"

class SkBitmap;
class WebCursor;

struct AccessibilityHostMsg_NotificationParams;
struct GpuHostMsg_AcceleratedSurfaceBuffersSwapped_Params;
struct GpuHostMsg_AcceleratedSurfacePostSubBuffer_Params;
struct ViewHostMsg_TextInputState_Params;
struct ViewHostMsg_SelectionBounds_Params;

namespace cc {
class CompositorFrame;
}

namespace media {
class VideoFrame;
}

namespace webkit {
namespace npapi {
struct WebPluginGeometry;
}
}

namespace WebKit {
struct WebScreenInfo;
}

namespace content {
class BackingStore;
class RenderWidgetHostViewFrameSubscriber;
class SmoothScrollGesture;
struct NativeWebKeyboardEvent;

// This is the larger RenderWidgetHostView interface exposed only
// within content/ and to embedders looking to port to new platforms.
// RenderWidgetHostView class hierarchy described in render_widget_host_view.h.
class CONTENT_EXPORT RenderWidgetHostViewPort : public RenderWidgetHostView,
                                                public IPC::Listener {
 public:
  virtual ~RenderWidgetHostViewPort() {}

  // Does the cast for you.
  static RenderWidgetHostViewPort* FromRWHV(RenderWidgetHostView* rwhv);

  // Like RenderWidgetHostView::CreateViewForWidget, with cast.
  static RenderWidgetHostViewPort* CreateViewForWidget(
      RenderWidgetHost* widget);

  static void GetDefaultScreenInfo(WebKit::WebScreenInfo* results);

  // Perform all the initialization steps necessary for this object to represent
  // a popup (such as a <select> dropdown), then shows the popup at |pos|.
  virtual void InitAsPopup(RenderWidgetHostView* parent_host_view,
                           const gfx::Rect& pos) = 0;

  // Perform all the initialization steps necessary for this object to represent
  // a full screen window.
  // |reference_host_view| is the view associated with the creating page that
  // helps to position the full screen widget on the correct monitor.
  virtual void InitAsFullscreen(
      RenderWidgetHostView* reference_host_view) = 0;

  // Notifies the View that it has become visible.
  virtual void WasShown() = 0;

  // Notifies the View that it has been hidden.
  virtual void WasHidden() = 0;

  // Moves all plugin windows as described in the given list.
  // |scroll_offset| is the scroll offset of the render view.
  virtual void MovePluginWindows(
      const gfx::Vector2d& scroll_offset,
      const std::vector<webkit::npapi::WebPluginGeometry>& moves) = 0;

  // Take focus from the associated View component.
  virtual void Blur() = 0;

  // Sets the cursor to the one associated with the specified cursor_type
  virtual void UpdateCursor(const WebCursor& cursor) = 0;

  // Indicates whether the page has finished loading.
  virtual void SetIsLoading(bool is_loading) = 0;

  // Updates the state of the input method attached to the view.
  virtual void TextInputStateChanged(
      const ViewHostMsg_TextInputState_Params& params) = 0;

  // Cancel the ongoing composition of the input method attached to the view.
  virtual void ImeCancelComposition() = 0;

  // Updates the range of the marked text in an IME composition.
  virtual void ImeCompositionRangeChanged(
      const ui::Range& range,
      const std::vector<gfx::Rect>& character_bounds) = 0;

  // Informs the view that a portion of the widget's backing store was scrolled
  // and/or painted.  The view should ensure this gets copied to the screen.
  //
  // If the scroll_rect is non-empty, then a portion of the widget's backing
  // store was scrolled by dx pixels horizontally and dy pixels vertically.
  // The exposed rect from the scroll operation is included in copy_rects.
  //
  // There are subtle performance implications here.  The RenderWidget gets sent
  // a paint ack after this returns, so if the view only ever invalidates in
  // response to this, then on Windows, where WM_PAINT has lower priority than
  // events which can cause renderer resizes/paint rect updates, e.g.
  // drag-resizing can starve painting; this function thus provides the view its
  // main chance to ensure it stays painted and not just invalidated.  On the
  // other hand, if this always blindly paints, then if we're already in the
  // midst of a paint on the callstack, we can double-paint unnecessarily.
  // (Worse, we might recursively call RenderWidgetHost::GetBackingStore().)
  // Thus implementers should generally paint as much of |rect| as possible
  // synchronously with as little overpainting as possible.
  virtual void DidUpdateBackingStore(
      const gfx::Rect& scroll_rect,
      const gfx::Vector2d& scroll_delta,
      const std::vector<gfx::Rect>& copy_rects) = 0;

  // Notifies the View that the renderer has ceased to exist.
  virtual void RenderViewGone(base::TerminationStatus status,
                              int error_code) = 0;

  // Tells the View to destroy itself.
  virtual void Destroy() = 0;

  // Tells the View that the tooltip text for the current mouse position over
  // the page has changed.
  virtual void SetTooltipText(const string16& tooltip_text) = 0;

  // Notifies the View that the renderer text selection has changed.
  virtual void SelectionChanged(const string16& text,
                                size_t offset,
                                const ui::Range& range) = 0;

  // Notifies the View that the renderer selection bounds has changed.
  // |start_rect| and |end_rect| are the bounds end of the selection in the
  // coordinate system of the render view. |start_direction| and |end_direction|
  // indicates the direction at which the selection was made on touch devices.
  virtual void SelectionBoundsChanged(
      const ViewHostMsg_SelectionBounds_Params& params) = 0;

  // Notifies the view that the scroll offset has changed.
  virtual void ScrollOffsetChanged() = 0;

  // Allocate a backing store for this view.
  virtual BackingStore* AllocBackingStore(const gfx::Size& size) = 0;

  // Copies the contents of the compositing surface into the given
  // (uninitialized) PlatformCanvas if any.
  // The rectangle region specified with |src_subrect| is copied from the
  // contents, scaled to |dst_size|, and written to |output|.
  // |callback| is invoked with true on success, false otherwise. |output| can
  // be initialized even on failure.
  // A smaller region than |src_subrect| may be copied if the underlying surface
  // is smaller than |src_subrect|.
  // NOTE: |callback| is called asynchronously.
  virtual void CopyFromCompositingSurface(
      const gfx::Rect& src_subrect,
      const gfx::Size& dst_size,
      const base::Callback<void(bool, const SkBitmap&)>& callback) = 0;

  // Copies a given subset of the compositing surface's content into a YV12
  // VideoFrame, and invokes a callback with a success/fail parameter. |target|
  // must contain an allocated, YV12 video frame of the intended size. If the
  // copy rectangle does not match |target|'s size, the copied content will be
  // scaled and letterboxed with black borders. The copy will happen
  // asynchronously. This operation will fail if there is no available
  // compositing surface.
  virtual void CopyFromCompositingSurfaceToVideoFrame(
      const gfx::Rect& src_subrect,
      const scoped_refptr<media::VideoFrame>& target,
      const base::Callback<void(bool)>& callback) = 0;

  // Returns true if CopyFromCompositingSurfaceToVideoFrame() is likely to
  // succeed.
  //
  // TODO(nick): When VideoFrame copies are broadly implemented, this method
  // should be renamed to HasCompositingSurface(), or unified with
  // IsSurfaceAvailableForCopy() and HasAcceleratedSurface().
  virtual bool CanCopyToVideoFrame() const = 0;

  // Return true if frame subscription is supported on this platform.
  virtual bool CanSubscribeFrame() const = 0;

  // Begin subscribing for presentation events and captured frames.
  // |subscriber| is now owned by this object, it will be called only on the
  // UI thread.
  virtual void BeginFrameSubscription(
      RenderWidgetHostViewFrameSubscriber* subscriber) = 0;

  // End subscribing for frame presentation events. FrameSubscriber will be
  // deleted after this call.
  virtual void EndFrameSubscription() = 0;

  // Called when accelerated compositing state changes.
  virtual void OnAcceleratedCompositingStateChange() = 0;
  // |params.window| and |params.surface_id| indicate which accelerated
  // surface's buffers swapped. |params.renderer_id| and |params.route_id|
  // are used to formulate a reply to the GPU process to prevent it from getting
  // too far ahead. They may all be zero, in which case no flow control is
  // enforced; this case is currently used for accelerated plugins.
  virtual void AcceleratedSurfaceBuffersSwapped(
      const GpuHostMsg_AcceleratedSurfaceBuffersSwapped_Params& params_in_pixel,
      int gpu_host_id) = 0;
  // Similar to above, except |params.(x|y|width|height)| define the region
  // of the surface that changed.
  virtual void AcceleratedSurfacePostSubBuffer(
      const GpuHostMsg_AcceleratedSurfacePostSubBuffer_Params& params_in_pixel,
      int gpu_host_id) = 0;

  // Release the accelerated surface temporarily. It will be recreated on the
  // next swap buffers or post sub buffer.
  virtual void AcceleratedSurfaceSuspend() = 0;

  virtual void AcceleratedSurfaceRelease() = 0;

  // Return true if the view has an accelerated surface that contains the last
  // presented frame for the view. If |desired_size| is non-empty, true is
  // returned only if the accelerated surface size matches.
  virtual bool HasAcceleratedSurface(const gfx::Size& desired_size) = 0;

  virtual void OnSwapCompositorFrame(const cc::CompositorFrame& frame) = 0;

  virtual void GetScreenInfo(WebKit::WebScreenInfo* results) = 0;

  // The size of the view's backing surface in non-DPI-adjusted pixels.
  virtual gfx::Size GetPhysicalBackingSize() const = 0;

  // Gets the bounds of the window, in screen coordinates.
  virtual gfx::Rect GetBoundsInRootWindow() = 0;

  virtual gfx::GLSurfaceHandle GetCompositingSurface() = 0;

  // Because the associated remote WebKit instance can asynchronously
  // prevent-default on a dispatched touch event, the touch events are queued in
  // the GestureRecognizer until invocation of ProcessAckedTouchEvent releases
  // it to be consumed (when |ack_result| is NOT_CONSUMED OR NO_CONSUMER_EXISTS)
  // or ignored (when |ack_result| is CONSUMED).
  virtual void ProcessAckedTouchEvent(const WebKit::WebTouchEvent& touch,
                                      InputEventAckState ack_result) = 0;

  // Asks the view to create a smooth scroll gesture that will be used to
  // simulate a user-initiated scroll.
  virtual SmoothScrollGesture* CreateSmoothScrollGesture(
      bool scroll_down, int pixels_to_scroll, int mouse_event_x,
      int mouse_event_y) = 0;

  virtual void SetHasHorizontalScrollbar(bool has_horizontal_scrollbar) = 0;
  virtual void SetScrollOffsetPinning(
      bool is_pinned_to_left, bool is_pinned_to_right) = 0;

  // Called when a mousewheel event was not processed by the renderer.
  virtual void UnhandledWheelEvent(const WebKit::WebMouseWheelEvent& event) = 0;

  virtual void SetPopupType(WebKit::WebPopupType popup_type) = 0;
  virtual WebKit::WebPopupType GetPopupType() = 0;

  virtual BrowserAccessibilityManager*
      GetBrowserAccessibilityManager() const = 0;
  virtual void OnAccessibilityNotifications(
      const std::vector<AccessibilityHostMsg_NotificationParams>& params) = 0;

#if defined(OS_MACOSX)
  // Called just before GetBackingStore blocks for an updated frame.
  virtual void AboutToWaitForBackingStoreMsg() = 0;

  // Does any event handling necessary for plugin IME; should be called after
  // the plugin has already had a chance to process the event. If plugin IME is
  // not enabled, this is a no-op, so it is always safe to call.
  // Returns true if the event was handled by IME.
  virtual bool PostProcessEventForPluginIme(
      const NativeWebKeyboardEvent& event) = 0;
#endif

#if defined(OS_ANDROID)
  virtual void ShowDisambiguationPopup(const gfx::Rect& target_rect,
                                       const SkBitmap& zoomed_bitmap) = 0;
  virtual void UpdateFrameInfo(const gfx::Vector2dF& scroll_offset,
                               float page_scale_factor,
                               const gfx::Vector2dF& page_scale_factor_limits,
                               const gfx::SizeF& content_size,
                               const gfx::SizeF& viewport_size,
                               const gfx::Vector2dF& controls_offset,
                               const gfx::Vector2dF& content_offset) = 0;
  virtual void HasTouchEventHandlers(bool need_touch_events) = 0;
#endif

#if defined(OS_WIN) && !defined(USE_AURA)
  virtual void WillWmDestroy() = 0;
#endif
};

}  // namespace content

#endif  // CONTENT_PORT_BROWSER_RENDER_WIDGET_HOST_VIEW_PORT_H_
