// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_MAC_H_
#define CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_MAC_H_

#import <Cocoa/Cocoa.h>

#include <string>
#include <vector>

#include "base/mac/scoped_nsobject.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "content/browser/renderer_host/browser_compositor_view_mac.h"
#include "content/browser/renderer_host/input/mouse_wheel_phase_handler.h"
#include "content/browser/renderer_host/render_widget_host_ns_view_client.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/browser/renderer_host/text_input_manager.h"
#include "content/common/content_export.h"
#include "content/common/cursors/webcursor.h"
#include "ipc/ipc_sender.h"
#include "ui/accelerated_widget_mac/accelerated_widget_mac.h"
#include "ui/accelerated_widget_mac/display_link_mac.h"
#include "ui/base/cocoa/remote_layer_api.h"

namespace content {
class CursorManager;
class RenderWidgetHost;
class RenderWidgetHostNSViewBridge;
class RenderWidgetHostViewMac;
class WebContents;
}

namespace ui {
class ScopedPasswordInputEnabler;
}

@class FullscreenWindowManager;
@protocol RenderWidgetHostViewMacDelegate;

@class RenderWidgetHostViewCocoa;

namespace content {

///////////////////////////////////////////////////////////////////////////////
// RenderWidgetHostViewMac
//
//  An object representing the "View" of a rendered web page. This object is
//  responsible for displaying the content of the web page, and integrating with
//  the Cocoa view system. It is the implementation of the RenderWidgetHostView
//  that the cross-platform RenderWidgetHost object uses
//  to display the data.
//
//  Comment excerpted from render_widget_host.h:
//
//    "The lifetime of the RenderWidgetHost* is tied to the render process.
//     If the render process dies, the RenderWidgetHost* goes away and all
//     references to it must become NULL."
//
// RenderWidgetHostView class hierarchy described in render_widget_host_view.h.
class CONTENT_EXPORT RenderWidgetHostViewMac
    : public RenderWidgetHostViewBase,
      public RenderWidgetHostNSViewClient,
      public BrowserCompositorMacClient,
      public TextInputManager::Observer,
      public ui::AcceleratedWidgetMacNSView,
      public IPC::Sender {
 public:
  // The view will associate itself with the given widget. The native view must
  // be hooked up immediately to the view hierarchy, or else when it is
  // deleted it will delete this out from under the caller.
  //
  // When |is_guest_view_hack| is true, this view isn't really the view for
  // the |widget|, a RenderWidgetHostViewGuest is.
  // TODO(lazyboy): Remove |is_guest_view_hack| once BrowserPlugin has migrated
  // to use RWHVChildFrame (http://crbug.com/330264).
  RenderWidgetHostViewMac(RenderWidgetHost* widget, bool is_guest_view_hack);
  ~RenderWidgetHostViewMac() override;

  RenderWidgetHostViewCocoa* cocoa_view() const;

  // |delegate| is used to separate out the logic from the NSResponder delegate.
  // |delegate| is retained by this class.
  // |delegate| should be set at most once.
  CONTENT_EXPORT void SetDelegate(
    NSObject<RenderWidgetHostViewMacDelegate>* delegate);
  void SetAllowPauseForResizeOrRepaint(bool allow);

  // RenderWidgetHostView implementation.
  void InitAsChild(gfx::NativeView parent_view) override;
  void SetSize(const gfx::Size& size) override;
  void SetBounds(const gfx::Rect& rect) override;
  gfx::Vector2dF GetLastScrollOffset() const override;
  gfx::NativeView GetNativeView() const override;
  gfx::NativeViewAccessible GetNativeViewAccessible() override;
  bool HasFocus() const override;
  void Show() override;
  void Hide() override;
  bool IsShowing() override;
  void WasUnOccluded() override;
  void WasOccluded() override;
  gfx::Rect GetViewBounds() const override;
  void SetActive(bool active) override;
  void ShowDefinitionForSelection() override;
  void SpeakSelection() override;
  void SetBackgroundColor(SkColor color) override;
  SkColor background_color() const override;
  void SetNeedsBeginFrames(bool needs_begin_frames) override;
  void GetScreenInfo(ScreenInfo* screen_info) const override;
  void SetWantsAnimateOnlyBeginFrames() override;

  // Implementation of RenderWidgetHostViewBase.
  void InitAsPopup(RenderWidgetHostView* parent_host_view,
                   const gfx::Rect& pos) override;
  void InitAsFullscreen(RenderWidgetHostView* reference_host_view) override;
  void Focus() override;
  void UpdateCursor(const WebCursor& cursor) override;
  void DisplayCursor(const WebCursor& cursor) override;
  CursorManager* GetCursorManager() override;
  void SetIsLoading(bool is_loading) override;
  void RenderProcessGone(base::TerminationStatus status,
                         int error_code) override;
  void Destroy() override;
  void SetTooltipText(const base::string16& tooltip_text) override;
  gfx::Size GetRequestedRendererSize() const override;
  bool IsSurfaceAvailableForCopy() const override;
  void CopyFromSurface(
      const gfx::Rect& src_rect,
      const gfx::Size& output_size,
      base::OnceCallback<void(const SkBitmap&)> callback) override;
  void FocusedNodeChanged(bool is_editable_node,
                          const gfx::Rect& node_bounds_in_screen) override;
  void DidCreateNewRendererCompositorFrameSink(
      viz::mojom::CompositorFrameSinkClient* renderer_compositor_frame_sink)
      override;
  void SubmitCompositorFrame(
      const viz::LocalSurfaceId& local_surface_id,
      viz::CompositorFrame frame,
      viz::mojom::HitTestRegionListPtr hit_test_region_list) override;
  void OnDidNotProduceFrame(const viz::BeginFrameAck& ack) override;
  void ClearCompositorFrame() override;
  BrowserAccessibilityManager* CreateBrowserAccessibilityManager(
      BrowserAccessibilityDelegate* delegate, bool for_root_frame) override;
  gfx::Point AccessibilityOriginInScreen(const gfx::Rect& bounds) override;
  gfx::AcceleratedWidget AccessibilityGetAcceleratedWidget() override;

  bool ShouldContinueToPauseForFrame() override;
  gfx::Vector2d GetOffsetFromRootSurface() override;
  gfx::Rect GetBoundsInRootWindow() override;
  viz::ScopedSurfaceIdAllocator ResizeDueToAutoResize(
      const gfx::Size& new_size,
      uint64_t sequence_number) override;
  void DidNavigate() override;

  bool LockMouse() override;
  void UnlockMouse() override;
  void GestureEventAck(const blink::WebGestureEvent& event,
                       InputEventAckState ack_result) override;

  void DidOverscroll(const ui::DidOverscrollParams& params) override;

  std::unique_ptr<SyntheticGestureTarget> CreateSyntheticGestureTarget()
      override;

  viz::FrameSinkId GetFrameSinkId() override;
  viz::LocalSurfaceId GetLocalSurfaceId() const override;
  // Returns true when we can do SurfaceHitTesting for the event type.
  bool ShouldRouteEvent(const blink::WebInputEvent& event) const;
  // This method checks |event| to see if a GesturePinch event can be routed
  // according to ShouldRouteEvent, and if not, sends it directly to the view's
  // RenderWidgetHost.
  // By not just defaulting to sending the GesturePinch events to the mainframe,
  // we allow the events to be targeted to an oopif subframe, in case some
  // consumer, such as PDF or maps, wants to intercept them and implement a
  // custom behavior.
  void SendGesturePinchEvent(blink::WebGestureEvent* event);
  bool TransformPointToLocalCoordSpace(const gfx::PointF& point,
                                       const viz::SurfaceId& original_surface,
                                       gfx::PointF* transformed_point) override;
  bool TransformPointToCoordSpaceForView(
      const gfx::PointF& point,
      RenderWidgetHostViewBase* target_view,
      gfx::PointF* transformed_point) override;
  viz::FrameSinkId GetRootFrameSinkId() override;
  viz::SurfaceId GetCurrentSurfaceId() const override;

  // TextInputManager::Observer implementation.
  void OnUpdateTextInputStateCalled(TextInputManager* text_input_manager,
                                    RenderWidgetHostViewBase* updated_view,
                                    bool did_update_state) override;
  void OnImeCancelComposition(TextInputManager* text_input_manager,
                              RenderWidgetHostViewBase* updated_view) override;
  void OnImeCompositionRangeChanged(
      TextInputManager* text_input_manager,
      RenderWidgetHostViewBase* updated_view) override;
  void OnSelectionBoundsChanged(
      TextInputManager* text_input_manager,
      RenderWidgetHostViewBase* updated_view) override;
  void OnTextSelectionChanged(TextInputManager* text_input_manager,
                              RenderWidgetHostViewBase* updated_view) override;
  // IPC::Sender implementation.
  bool Send(IPC::Message* message) override;

  // Forwards the mouse event to the renderer.
  void ForwardMouseEvent(const blink::WebMouseEvent& event);

  void KillSelf();

  void SetTextInputActive(bool active);

  // Returns true and stores first rectangle for character range if the
  // requested |range| is already cached, otherwise returns false.
  // Exposed for testing.
  CONTENT_EXPORT bool GetCachedFirstRectForCharacterRange(
      NSRange range, NSRect* rect, NSRange* actual_range);

  // Returns true if there is line break in |range| and stores line breaking
  // point to |line_breaking_point|. The |line_break_point| is valid only if
  // this function returns true.
  bool GetLineBreakIndex(const std::vector<gfx::Rect>& bounds,
                         const gfx::Range& range,
                         size_t* line_break_point);

  // Returns composition character boundary rectangle. The |range| is
  // composition based range. Also stores |actual_range| which is corresponding
  // to actually used range for returned rectangle.
  gfx::Rect GetFirstRectForCompositionRange(const gfx::Range& range,
                                            gfx::Range* actual_range);

  // Converts from given whole character range to composition oriented range. If
  // the conversion failed, return gfx::Range::InvalidRange.
  gfx::Range ConvertCharacterRangeToCompositionRange(
      const gfx::Range& request_range);

  WebContents* GetWebContents();

  // Set the color of the background CALayer shown when no content is ready to
  // see.
  void SetBackgroundLayerColor(SkColor color);

  bool HasPendingWheelEndEventForTesting() {
    return mouse_wheel_phase_handler_.HasPendingWheelEndEvent();
  }

  // These member variables should be private, but the associated ObjC class
  // needs access to them and can't be made a friend.

  // Delegated frame management and compositor interface.
  std::unique_ptr<BrowserCompositorMac> browser_compositor_;
  BrowserCompositorMac* BrowserCompositorForTesting() const {
    return browser_compositor_.get();
  }

  // Set when the currently-displayed frame is the minimum scale. Used to
  // determine if pinch gestures need to be thresholded.
  bool page_at_minimum_scale_;

  MouseWheelPhaseHandler mouse_wheel_phase_handler_;

  // Used to set the mouse_wheel_phase_handler_ timer timeout for testing.
  void set_mouse_wheel_wheel_phase_handler_timeout(base::TimeDelta timeout) {
    mouse_wheel_phase_handler_.set_mouse_wheel_end_dispatch_timeout(timeout);
  }

  NSWindow* pepper_fullscreen_window() const {
    return pepper_fullscreen_window_;
  }

  CONTENT_EXPORT void release_pepper_fullscreen_window_for_testing();

  int window_number() const;

  // Update the size, scale factor, color profile, vsync parameters, and any
  // other properties of the NSView or its NSScreen. Propagate these to the
  // RenderWidgetHostImpl as well.
  void UpdateNSViewAndDisplayProperties();

  void PauseForPendingResizeOrRepaintsAndDraw();

  // RenderWidgetHostNSViewClient implementation.
  RenderWidgetHostViewMac* GetRenderWidgetHostViewMac() override;
  void OnNSViewBoundsInWindowChanged(const gfx::Rect& view_bounds_in_window_dip,
                                     bool attached_to_window) override;
  void OnNSViewWindowFrameInScreenChanged(
      const gfx::Rect& window_frame_in_screen_dip) override;
  void OnNSViewDisplayChanged(const display::Display& display) override;

  // BrowserCompositorMacClient implementation.
  SkColor BrowserCompositorMacGetGutterColor() const override;
  void BrowserCompositorMacOnBeginFrame() override;
  void OnFrameTokenChanged(uint32_t frame_token) override;
  void DidReceiveFirstFrameAfterNavigation() override;
  void DestroyCompositorForShutdown() override;

  // AcceleratedWidgetMacNSView implementation.
  NSView* AcceleratedWidgetGetNSView() const override;
  void AcceleratedWidgetGetVSyncParameters(
      base::TimeTicks* timebase, base::TimeDelta* interval) const override;
  void AcceleratedWidgetSwapCompleted() override;

  void SetShowingContextMenu(bool showing) override;

  // Helper method to obtain ui::TextInputType for the active widget from the
  // TextInputManager.
  ui::TextInputType GetTextInputType();

  // Helper method to obtain the currently active widget from TextInputManager.
  // An active widget is a RenderWidget which is currently focused and has a
  // |TextInputState.type| which is not ui::TEXT_INPUT_TYPE_NONE.
  RenderWidgetHostImpl* GetActiveWidget();

  // Returns the composition range information for the active RenderWidgetHost
  // (accepting IME and keyboard input).
  const TextInputManager::CompositionRangeInfo* GetCompositionRangeInfo();

  // Returns the TextSelection information for the active widget. If
  // |is_guest_view_hack_| is true, then it will return the TextSelection
  // information for this RenderWidgetHostViewMac (which is serving as a
  // platform view for a guest).
  const TextInputManager::TextSelection* GetTextSelection();

  // Get the focused view that should be used for retrieving the text selection.
  RenderWidgetHostViewBase* GetFocusedViewForTextSelection();

  // Returns the RenderWidgetHostDelegate corresponding to the currently focused
  // RenderWidgetHost. It is different from |render_widget_host_->delegate()|
  // when there are focused inner WebContentses on the page. Also, this method
  // can return nullptr; for instance when |render_widget_host_| becomes nullptr
  // in the destruction path of the WebContentsImpl.
  RenderWidgetHostDelegate* GetFocusedRenderWidgetHostDelegate();

 private:
  friend class RenderWidgetHostViewMacTest;
  FRIEND_TEST_ALL_PREFIXES(RenderWidgetHostViewMacTest, GetPageTextForSpeech);

  // Allocate a new FrameSinkId if this object is the platform view of a
  // RenderWidgetHostViewGuest. This FrameSinkId will not be actually used in
  // any useful way. It's only created because BrowserCompositorMac always
  // expects to have a FrameSinkId. FrameSinkIds generated by this method do not
  // collide with FrameSinkIds used by RenderWidgetHostImpls.
  static viz::FrameSinkId AllocateFrameSinkIdForGuestViewHack();

  // Returns whether this render view is a popup (autocomplete window).
  bool IsPopup() const;

  // Shuts down the render_widget_host_.  This is a separate function so we can
  // invoke it from the message loop.
  void ShutdownHost();

  // Send updated vsync parameters to the top level display.
  void UpdateDisplayVSyncParameters();

  // Adds/Removes frame observer based on state.
  void UpdateNeedsBeginFramesInternal();

  void SendSyntheticWheelEventWithPhaseEnded(
      blink::WebMouseWheelEvent wheel_event,
      bool should_route_event);

  void OnResizeDueToAutoResizeComplete(const gfx::Size& new_size,
                                       uint64_t sequence_number);

  // Gets a textual view of the page's contents, and passes it to the callback
  // provided.
  using SpeechCallback = base::OnceCallback<void(const base::string16&)>;
  void GetPageTextForSpeech(SpeechCallback callback);

  // Interface through which the NSView is to be manipulated.
  std::unique_ptr<RenderWidgetHostNSViewBridge> ns_view_bridge_;

  // State tracked by Show/Hide/IsShowing.
  bool is_visible_ = false;

  // The bounds of the view in its NSWindow's coordinate system (with origin
  // in the upper-left).
  gfx::Rect view_bounds_in_window_dip_;

  // The frame of the window in the global display::Screen coordinate system
  // (where the origin is the upper-left corner of Screen::GetPrimaryDisplay).
  gfx::Rect window_frame_in_screen_dip_;

  // Cached copy of the display information pushed to us from the NSView.
  display::Display display_;

  // Indicates if the page is loading.
  bool is_loading_;

  // Whether it's allowed to pause waiting for a new frame.
  bool allow_pause_for_resize_or_repaint_;

  // The last scroll offset of the view.
  gfx::Vector2dF last_scroll_offset_;

  // The text to be shown in the tooltip, supplied by the renderer.
  base::string16 tooltip_text_;

  // True when this view acts as a platform view hack for a
  // RenderWidgetHostViewGuest.
  bool is_guest_view_hack_;

  // The window used for popup widgets.
  base::scoped_nsobject<NSWindow> popup_window_;

  // The fullscreen window used for pepper flash.
  base::scoped_nsobject<NSWindow> pepper_fullscreen_window_;
  base::scoped_nsobject<FullscreenWindowManager> fullscreen_window_manager_;
  // Our parent host view, if this is fullscreen.  NULL otherwise.
  RenderWidgetHostViewMac* fullscreen_parent_host_view_;

  // Display link for getting vsync info.
  scoped_refptr<ui::DisplayLinkMac> display_link_;

  // The current VSync timebase and interval. This is zero until the first call
  // to SendVSyncParametersToRenderer(), and refreshed regularly thereafter.
  base::TimeTicks vsync_timebase_;
  base::TimeDelta vsync_interval_;

  // Whether a request for begin frames has been issued.
  bool needs_begin_frames_;

  // Whether or not the background is opaque as determined by calls to
  // SetBackgroundColor. The default value is opaque.
  bool background_is_opaque_ = true;

  // The color of the background CALayer, stored as a SkColor for efficient
  // comparison. Initially transparent so that the embedding NSView shows
  // through.
  SkColor background_layer_color_ = SK_ColorTRANSPARENT;

  // The background color of the last frame that was swapped. This is not
  // applied until the swap completes (see comments in
  // AcceleratedWidgetSwapCompleted).
  SkColor last_frame_root_background_color_ = SK_ColorTRANSPARENT;

  int tab_show_sequence_ = 0;

  std::unique_ptr<CursorManager> cursor_manager_;

  // Used to track active password input sessions.
  std::unique_ptr<ui::ScopedPasswordInputEnabler> password_input_enabler_;

  // Factory used to safely scope delayed calls to ShutdownHost().
  base::WeakPtrFactory<RenderWidgetHostViewMac> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(RenderWidgetHostViewMac);
};

// RenderWidgetHostViewCocoa is not exported outside of content. This helper
// method provides the tests with the class object so that they can override the
// methods according to the tests' requirements.
CONTENT_EXPORT Class GetRenderWidgetHostViewCocoaClassForTesting();

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_MAC_H_
