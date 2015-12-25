// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_BASE_H_
#define CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_BASE_H_

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/process/kill.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "cc/output/compositor_frame.h"
#include "content/browser/renderer_host/event_with_latency_info.h"
#include "content/common/content_export.h"
#include "content/common/input/input_event_ack_state.h"
#include "content/public/browser/readback_types.h"
#include "content/public/browser/render_widget_host_view.h"
#include "ipc/ipc_listener.h"
#include "third_party/WebKit/public/platform/modules/screen_orientation/WebScreenOrientationType.h"
#include "third_party/WebKit/public/web/WebPopupType.h"
#include "third_party/WebKit/public/web/WebTextDirection.h"
#include "ui/base/ime/text_input_mode.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/gfx/display.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/range/range.h"
#include "ui/surface/transport_dib.h"

class SkBitmap;

struct AccessibilityHostMsg_EventParams;
struct ViewHostMsg_SelectionBounds_Params;
struct ViewHostMsg_TextInputState_Params;

namespace media {
class VideoFrame;
}

namespace blink {
struct WebScreenInfo;
class WebMouseEvent;
class WebMouseWheelEvent;
}

namespace ui {
class LatencyInfo;
}

namespace content {
class BrowserAccessibilityDelegate;
class BrowserAccessibilityManager;
class SyntheticGesture;
class SyntheticGestureTarget;
class WebCursor;
struct DidOverscrollParams;
struct NativeWebKeyboardEvent;
struct WebPluginGeometry;

// Basic implementation shared by concrete RenderWidgetHostView subclasses.
class CONTENT_EXPORT RenderWidgetHostViewBase : public RenderWidgetHostView,
                                                public IPC::Listener {
 public:
  ~RenderWidgetHostViewBase() override;

  // RenderWidgetHostView implementation.
  void SetBackgroundColor(SkColor color) override;
  void SetBackgroundColorToDefault() final;
  bool GetBackgroundOpaque() override;
  ui::TextInputClient* GetTextInputClient() override;
  void WasUnOccluded() override {}
  void WasOccluded() override {}
  bool IsShowingContextMenu() const override;
  void SetShowingContextMenu(bool showing_menu) override;
  base::string16 GetSelectedText() const override;
  bool IsMouseLocked() override;
  gfx::Size GetVisibleViewportSize() const override;
  void SetInsets(const gfx::Insets& insets) override;
  void BeginFrameSubscription(
      scoped_ptr<RenderWidgetHostViewFrameSubscriber> subscriber) override;
  void EndFrameSubscription() override;

  // IPC::Listener implementation:
  bool OnMessageReceived(const IPC::Message& msg) override;

  void SetPopupType(blink::WebPopupType popup_type);

  blink::WebPopupType GetPopupType();

  // Return a value that is incremented each time the renderer swaps a new frame
  // to the view.
  uint32_t RendererFrameNumber();

  // Called each time the RenderWidgetHost receives a new frame for display from
  // the renderer.
  void DidReceiveRendererFrame();

  // Notification that a resize or move session ended on the native widget.
  void UpdateScreenInfo(gfx::NativeView view);

  // Tells if the display property (work area/scale factor) has
  // changed since the last time.
  bool HasDisplayPropertyChanged(gfx::NativeView view);

  base::WeakPtr<RenderWidgetHostViewBase> GetWeakPtr();

  //----------------------------------------------------------------------------
  // The following methods can be overridden by derived classes.

  // Notifies the View that the renderer text selection has changed.
  virtual void SelectionChanged(const base::string16& text,
                                size_t offset,
                                const gfx::Range& range);

  // The requested size of the renderer. May differ from GetViewBounds().size()
  // when the view requires additional throttling.
  virtual gfx::Size GetRequestedRendererSize() const;

  // The size of the view's backing surface in non-DPI-adjusted pixels.
  virtual gfx::Size GetPhysicalBackingSize() const;

  // Whether or not Blink's viewport size should be shrunk by the height of the
  // URL-bar.
  virtual bool DoTopControlsShrinkBlinkSize() const;

  // The height of the URL-bar top controls.
  virtual float GetTopControlsHeight() const;

  // Called prior to forwarding input event messages to the renderer, giving
  // the view a chance to perform in-process event filtering or processing.
  // Return values of |NOT_CONSUMED| or |UNKNOWN| will result in |input_event|
  // being forwarded.
  virtual InputEventAckState FilterInputEvent(
      const blink::WebInputEvent& input_event);

  // Called by the host when it requires an input flush; the flush call should
  // by synchronized with BeginFrame.
  virtual void OnSetNeedsFlushInput();

  virtual void WheelEventAck(const blink::WebMouseWheelEvent& event,
                             InputEventAckState ack_result);

  virtual void GestureEventAck(const blink::WebGestureEvent& event,
                               InputEventAckState ack_result);

  // Create a platform specific SyntheticGestureTarget implementation that will
  // be used to inject synthetic input events.
  virtual scoped_ptr<SyntheticGestureTarget> CreateSyntheticGestureTarget();

  // Create a BrowserAccessibilityManager for this view.
  virtual BrowserAccessibilityManager* CreateBrowserAccessibilityManager(
      BrowserAccessibilityDelegate* delegate);

  virtual void AccessibilityShowMenu(const gfx::Point& point);
  virtual gfx::Point AccessibilityOriginInScreen(const gfx::Rect& bounds);
  virtual gfx::AcceleratedWidget AccessibilityGetAcceleratedWidget();
  virtual gfx::NativeViewAccessible AccessibilityGetNativeViewAccessible();

  // Informs that the focused DOM node has changed.
  virtual void FocusedNodeChanged(bool is_editable_node) {}

  virtual void OnSwapCompositorFrame(uint32_t output_surface_id,
                                     scoped_ptr<cc::CompositorFrame> frame) {}

  // This method exists to allow removing of displayed graphics, after a new
  // page has been loaded, to prevent the displayed URL from being out of sync
  // with what is visible on screen.
  virtual void ClearCompositorFrame() = 0;

  // Because the associated remote WebKit instance can asynchronously
  // prevent-default on a dispatched touch event, the touch events are queued in
  // the GestureRecognizer until invocation of ProcessAckedTouchEvent releases
  // it to be consumed (when |ack_result| is NOT_CONSUMED OR NO_CONSUMER_EXISTS)
  // or ignored (when |ack_result| is CONSUMED).
  virtual void ProcessAckedTouchEvent(const TouchEventWithLatencyInfo& touch,
                                      InputEventAckState ack_result) {}

  virtual void DidOverscroll(const DidOverscrollParams& params) {}

  virtual void DidStopFlinging() {}

  // Returns the compositing surface ID namespace, or 0 if Surfaces are not
  // enabled.
  virtual uint32_t GetSurfaceIdNamespace();

  // When there are multiple RenderWidgetHostViews for a single page, input
  // events need to be targeted to the correct one for handling. The following
  // methods are invoked on the RenderWidgetHostView that should be able to
  // properly handle the event (i.e. it has focus for keyboard events, or has
  // been identified by hit testing mouse, touch or gesture events).
  virtual uint32_t SurfaceIdNamespaceAtPoint(const gfx::Point& point,
                                             gfx::Point* transformed_point);
  virtual void ProcessKeyboardEvent(const NativeWebKeyboardEvent& event) {}
  virtual void ProcessMouseEvent(const blink::WebMouseEvent& event) {}
  virtual void ProcessMouseWheelEvent(const blink::WebMouseWheelEvent& event) {}
  virtual void ProcessTouchEvent(const blink::WebTouchEvent& event,
                         const ui::LatencyInfo& latency) {}

  // If a RenderWidgetHost is dealing with points that are transformed from the
  // root frame for a page (i.e. because its content is contained within
  // that of another RenderWidgetHost), this provides a facility to convert
  // a point from its own coordinate space to that of the root frame.
  // This only needs to be overriden by RenderWidgetHostView subclasses
  // that handle content embedded within other RenderWidgetHostViews.
  virtual void TransformPointToRootCoordSpace(const gfx::Point& point,
                                              gfx::Point* transformed_point);

  // Transform a point that is in the coordinate space of a Surface that is
  // embedded within the RenderWidgetHostViewBase's Surface to the
  // coordinate space of the embedding Surface. Typically this means that a
  // point was received from an out-of-process iframe's RenderWidget and needs
  // to be translated to viewport coordinates for the root RWHV, in which case
  // this method is called on the root RWHV with the out-of-process iframe's
  // SurfaceId.
  virtual void TransformPointToLocalCoordSpace(const gfx::Point& point,
                                               cc::SurfaceId original_surface,
                                               gfx::Point* transformed_point);

  //----------------------------------------------------------------------------
  // The following static methods are implemented by each platform.

  static void GetDefaultScreenInfo(blink::WebScreenInfo* results);

  //----------------------------------------------------------------------------
  // The following pure virtual methods are implemented by derived classes.

  // Perform all the initialization steps necessary for this object to represent
  // a popup (such as a <select> dropdown), then shows the popup at |pos|.
  virtual void InitAsPopup(RenderWidgetHostView* parent_host_view,
                           const gfx::Rect& bounds) = 0;

  // Perform all the initialization steps necessary for this object to represent
  // a full screen window.
  // |reference_host_view| is the view associated with the creating page that
  // helps to position the full screen widget on the correct monitor.
  virtual void InitAsFullscreen(RenderWidgetHostView* reference_host_view) = 0;

  // Moves all plugin windows as described in the given list.
  // |scroll_offset| is the scroll offset of the render view.
  virtual void MovePluginWindows(
      const std::vector<WebPluginGeometry>& moves) = 0;

  // Sets the cursor to the one associated with the specified cursor_type
  virtual void UpdateCursor(const WebCursor& cursor) = 0;

  // Indicates whether the page has finished loading.
  virtual void SetIsLoading(bool is_loading) = 0;

  // Updates the state of the input method attached to the view.
  virtual void TextInputStateChanged(
      const ViewHostMsg_TextInputState_Params& params) = 0;

  // Cancel the ongoing composition of the input method attached to the view.
  virtual void ImeCancelComposition() = 0;

  // Notifies the View that the renderer has ceased to exist.
  virtual void RenderProcessGone(base::TerminationStatus status,
                                 int error_code) = 0;

  // Tells the View to destroy itself.
  virtual void Destroy() = 0;

  // Tells the View that the tooltip text for the current mouse position over
  // the page has changed.
  virtual void SetTooltipText(const base::string16& tooltip_text) = 0;

  // Notifies the View that the renderer selection bounds has changed.
  // |start_rect| and |end_rect| are the bounds end of the selection in the
  // coordinate system of the render view. |start_direction| and |end_direction|
  // indicates the direction at which the selection was made on touch devices.
  virtual void SelectionBoundsChanged(
      const ViewHostMsg_SelectionBounds_Params& params) = 0;

  // Copies the contents of the compositing surface, providing a new SkBitmap
  // result via an asynchronously-run |callback|. |src_subrect| is specified in
  // layer space coordinates for the current platform (e.g., DIP for Aura/Mac,
  // physical for Android), and is the region to be copied from this view. When
  // |src_subrect| is empty then the whole surface will be copied. The copy is
  // then scaled to a SkBitmap of size |dst_size|. If |dst_size| is empty then
  // output will be unscaled. |callback| is run with true on success,
  // false otherwise. A smaller region than |src_subrect| may be copied
  // if the underlying surface is smaller than |src_subrect|.
  virtual void CopyFromCompositingSurface(
      const gfx::Rect& src_subrect,
      const gfx::Size& dst_size,
      const ReadbackRequestCallback& callback,
      const SkColorType preferred_color_type) = 0;

  // Copies the contents of the compositing surface, populating the given
  // |target| with YV12 image data. |src_subrect| is specified in layer space
  // coordinates for the current platform (e.g., DIP for Aura/Mac, physical for
  // Android), and is the region to be copied from this view. The copy is then
  // scaled and letterboxed with black borders to fit |target|. Finally,
  // |callback| is asynchronously run with true/false for
  // success/failure. |target| must point to an allocated, YV12 video frame of
  // the intended size. This operation will fail if there is no available
  // compositing surface.
  virtual void CopyFromCompositingSurfaceToVideoFrame(
      const gfx::Rect& src_subrect,
      const scoped_refptr<media::VideoFrame>& target,
      const base::Callback<void(const gfx::Rect&, bool)>& callback) = 0;

  // Returns true if CopyFromCompositingSurfaceToVideoFrame() is likely to
  // succeed.
  //
  // TODO(nick): When VideoFrame copies are broadly implemented, this method
  // should be renamed to HasCompositingSurface(), or unified with
  // IsSurfaceAvailableForCopy() and HasAcceleratedSurface().
  virtual bool CanCopyToVideoFrame() const = 0;

  // Return true if the view has an accelerated surface that contains the last
  // presented frame for the view. If |desired_size| is non-empty, true is
  // returned only if the accelerated surface size matches.
  virtual bool HasAcceleratedSurface(const gfx::Size& desired_size) = 0;

  // Compute the orientation type of the display assuming it is a mobile device.
  static blink::WebScreenOrientationType GetOrientationTypeForMobile(
      const gfx::Display& display);

  // Compute the orientation type of the display assuming it is a desktop.
  static blink::WebScreenOrientationType GetOrientationTypeForDesktop(
      const gfx::Display& display);

  virtual void GetScreenInfo(blink::WebScreenInfo* results) = 0;
  virtual bool GetScreenColorProfile(std::vector<char>* color_profile) = 0;

  // Gets the bounds of the window, in screen coordinates.
  virtual gfx::Rect GetBoundsInRootWindow() = 0;

  // Called by the RenderFrameHost when it receives an IPC response to a
  // TextSurroundingSelectionRequest.
  virtual void OnTextSurroundingSelectionResponse(const base::string16& content,
                                                  size_t start_offset,
                                                  size_t end_offset);

  // Called by the RenderWidgetHost when an ambiguous gesture is detected to
  // show the disambiguation popup bubble.
  virtual void ShowDisambiguationPopup(const gfx::Rect& rect_pixels,
                                       const SkBitmap& zoomed_bitmap);

  // Called by the WebContentsImpl when a user tries to navigate a new page on
  // main frame.
  virtual void OnDidNavigateMainFrameToNewPage();

  // Instructs the view to not drop the surface even when the view is hidden.
  virtual void LockCompositingSurface() = 0;
  virtual void UnlockCompositingSurface() = 0;

#if defined(OS_MACOSX)
  // Does any event handling necessary for plugin IME; should be called after
  // the plugin has already had a chance to process the event. If plugin IME is
  // not enabled, this is a no-op, so it is always safe to call.
  // Returns true if the event was handled by IME.
  virtual bool PostProcessEventForPluginIme(
      const NativeWebKeyboardEvent& event) = 0;
#endif

  // Updates the range of the marked text in an IME composition.
  virtual void ImeCompositionRangeChanged(
      const gfx::Range& range,
      const std::vector<gfx::Rect>& character_bounds) = 0;

#if defined(OS_WIN)
  virtual void SetParentNativeViewAccessible(
      gfx::NativeViewAccessible accessible_parent) = 0;

  // Returns an HWND that's given as the parent window for windowless Flash to
  // workaround crbug.com/301548.
  virtual gfx::NativeViewId GetParentForWindowlessPlugin() const = 0;

  // The callback that DetachPluginsHelper calls for each child window. Call
  // this directly if you want to do custom filtering on plugin windows first.
  static void DetachPluginWindowsCallback(HWND window);
#endif

 protected:
  // Interface class only, do not construct.
  RenderWidgetHostViewBase();

#if defined(OS_WIN)
  // Shared implementation of MovePluginWindows for use by win and aura/wina.
  static void MovePluginWindowsHelper(
      HWND parent,
      const std::vector<WebPluginGeometry>& moves);

  static void PaintPluginWindowsHelper(
      HWND parent,
      const gfx::Rect& damaged_screen_rect);

  // Needs to be called before the HWND backing the view goes away to avoid
  // crashes in Windowed plugins.
  static void DetachPluginsHelper(HWND parent);
#endif

  // Whether this view is a popup and what kind of popup it is (select,
  // autofill...).
  blink::WebPopupType popup_type_;

  // The background color of the web content.
  SkColor background_color_;

  // While the mouse is locked, the cursor is hidden from the user. Mouse events
  // are still generated. However, the position they report is the last known
  // mouse position just as mouse lock was entered; the movement they report
  // indicates what the change in position of the mouse would be had it not been
  // locked.
  bool mouse_locked_;

  // Whether we are showing a context menu.
  bool showing_context_menu_;

  // A buffer containing the text inside and around the current selection range.
  base::string16 selection_text_;

  // The offset of the text stored in |selection_text_| relative to the start of
  // the web page.
  size_t selection_text_offset_;

  // The current selection range relative to the start of the web page.
  gfx::Range selection_range_;

 protected:
  // The scale factor of the display the renderer is currently on.
  float current_device_scale_factor_;

  // The orientation of the display the renderer is currently on.
  gfx::Display::Rotation current_display_rotation_;

  // Whether pinch-to-zoom should be enabled and pinch events forwarded to the
  // renderer.
  bool pinch_zoom_enabled_;

 private:
  void FlushInput();

  gfx::Rect current_display_area_;

  uint32_t renderer_frame_number_;

  base::OneShotTimer flush_input_timer_;

  base::WeakPtrFactory<RenderWidgetHostViewBase> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(RenderWidgetHostViewBase);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_BASE_H_
