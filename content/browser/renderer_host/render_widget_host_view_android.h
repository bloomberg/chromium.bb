// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_ANDROID_H_
#define CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_ANDROID_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <queue>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/i18n/rtl.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/process/process.h"
#include "cc/input/selection.h"
#include "cc/output/begin_frame_args.h"
#include "cc/scheduler/begin_frame_source.h"
#include "components/viz/service/frame_sinks/frame_evictor.h"
#include "content/browser/android/content_view_core_observer.h"
#include "content/browser/renderer_host/input/mouse_wheel_phase_handler.h"
#include "content/browser/renderer_host/input/stylus_text_selector.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/browser/renderer_host/text_input_manager.h"
#include "content/common/content_export.h"
#include "content/public/browser/readback_types.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/android/delegated_frame_host_android.h"
#include "ui/android/view_android.h"
#include "ui/android/view_client.h"
#include "ui/android/window_android_observer.h"
#include "ui/base/ui_base_types.h"
#include "ui/events/gesture_detection/filtered_gesture_provider.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/vector2d_f.h"
#include "ui/gfx/selection_bound.h"
#include "ui/touch_selection/touch_selection_controller.h"

namespace ui {
class MotionEventAndroid;
struct DidOverscrollParams;
}

namespace content {
class ContentViewCore;
class ImeAdapterAndroid;
class OverscrollControllerAndroid;
class RenderWidgetHost;
class RenderWidgetHostImpl;
class SelectionPopupController;
class SynchronousCompositorHost;
class SynchronousCompositorClient;
class TouchSelectionControllerClientManagerAndroid;
class WebContentsAccessibilityAndroid;
struct NativeWebKeyboardEvent;
struct ContextMenuParams;

// -----------------------------------------------------------------------------
// See comments in render_widget_host_view.h about this class and its members.
// -----------------------------------------------------------------------------
class CONTENT_EXPORT RenderWidgetHostViewAndroid
    : public RenderWidgetHostViewBase,
      public ui::GestureProviderClient,
      public ui::ViewClient,
      public ui::WindowAndroidObserver,
      public viz::FrameEvictorClient,
      public StylusTextSelectorClient,
      public ui::TouchSelectionControllerClient,
      public content::ContentViewCoreObserver,
      public content::TextInputManager::Observer,
      public ui::DelegatedFrameHostAndroid::Client,
      public cc::BeginFrameObserver {
 public:
  RenderWidgetHostViewAndroid(RenderWidgetHostImpl* widget,
                              ContentViewCore* content_view_core);
  ~RenderWidgetHostViewAndroid() override;

  // Interface used to observe the destruction of a RenderWidgetHostViewAndroid.
  class DestructionObserver {
   public:
    virtual void RenderWidgetHostViewDestroyed(
        RenderWidgetHostViewAndroid* rwhva) = 0;

   protected:
    virtual ~DestructionObserver() {}
  };

  void AddDestructionObserver(DestructionObserver* connector);
  void RemoveDestructionObserver(DestructionObserver* connector);

  ui::TouchSelectionController* touch_selection_controller() {
    return touch_selection_controller_.get();
  }

  // RenderWidgetHostView implementation.
  bool OnMessageReceived(const IPC::Message& msg) override;
  void InitAsChild(gfx::NativeView parent_view) override;
  void InitAsPopup(RenderWidgetHostView* parent_host_view,
                   const gfx::Rect& pos) override;
  void InitAsFullscreen(RenderWidgetHostView* reference_host_view) override;
  RenderWidgetHost* GetRenderWidgetHost() const override;
  void SetSize(const gfx::Size& size) override;
  void SetBounds(const gfx::Rect& rect) override;
  gfx::Vector2dF GetLastScrollOffset() const override;
  gfx::NativeView GetNativeView() const override;
  gfx::NativeViewAccessible GetNativeViewAccessible() override;
  void Focus() override;
  bool HasFocus() const override;
  void Show() override;
  void Hide() override;
  bool IsShowing() override;
  gfx::Rect GetViewBounds() const override;
  gfx::Size GetVisibleViewportSize() const override;
  gfx::Size GetPhysicalBackingSize() const override;
  bool IsSurfaceAvailableForCopy() const override;
  void CopyFromSurface(const gfx::Rect& src_rect,
                       const gfx::Size& output_size,
                       const ReadbackRequestCallback& callback,
                       const SkColorType color_type) override;
  bool DoBrowserControlsShrinkBlinkSize() const override;
  float GetTopControlsHeight() const override;
  float GetBottomControlsHeight() const override;
  void UpdateCursor(const WebCursor& cursor) override;
  void SetIsLoading(bool is_loading) override;
  void FocusedNodeChanged(bool is_editable_node,
                          const gfx::Rect& node_bounds_in_screen) override;
  void RenderProcessGone(base::TerminationStatus status,
                         int error_code) override;
  void Destroy() override;
  void SetTooltipText(const base::string16& tooltip_text) override;
  bool HasAcceleratedSurface(const gfx::Size& desired_size) override;
  void SetBackgroundColor(SkColor color) override;
  SkColor background_color() const override;
  gfx::Rect GetBoundsInRootWindow() override;
  void ProcessAckedTouchEvent(const TouchEventWithLatencyInfo& touch,
                              InputEventAckState ack_result) override;
  InputEventAckState FilterInputEvent(
      const blink::WebInputEvent& input_event) override;
  void GestureEventAck(const blink::WebGestureEvent& event,
                       InputEventAckState ack_result) override;
  BrowserAccessibilityManager* CreateBrowserAccessibilityManager(
      BrowserAccessibilityDelegate* delegate, bool for_root_frame) override;
  bool LockMouse() override;
  void UnlockMouse() override;
  void DidCreateNewRendererCompositorFrameSink(
      cc::mojom::CompositorFrameSinkClient* renderer_compositor_frame_sink)
      override;
  void SubmitCompositorFrame(const viz::LocalSurfaceId& local_surface_id,
                             cc::CompositorFrame frame) override;
  void OnDidNotProduceFrame(const cc::BeginFrameAck& ack) override;
  void ClearCompositorFrame() override;
  void SetIsInVR(bool is_in_vr) override;
  bool IsInVR() const override;
  void DidOverscroll(const ui::DidOverscrollParams& params) override;
  void DidStopFlinging() override;
  void ShowDisambiguationPopup(const gfx::Rect& rect_pixels,
                               const SkBitmap& zoomed_bitmap) override;
  std::unique_ptr<SyntheticGestureTarget> CreateSyntheticGestureTarget()
      override;
  void OnDidNavigateMainFrameToNewPage() override;
  void SetNeedsBeginFrames(bool needs_begin_frames) override;
  viz::FrameSinkId GetFrameSinkId() override;
  viz::FrameSinkId FrameSinkIdAtPoint(cc::SurfaceHittestDelegate* delegate,
                                      const gfx::Point& point,
                                      gfx::Point* transformed_point) override;
  void ProcessMouseEvent(const blink::WebMouseEvent& event,
                         const ui::LatencyInfo& latency) override;
  void ProcessMouseWheelEvent(const blink::WebMouseWheelEvent& event,
                              const ui::LatencyInfo& latency) override;
  void ProcessTouchEvent(const blink::WebTouchEvent& event,
                         const ui::LatencyInfo& latency) override;
  void ProcessGestureEvent(const blink::WebGestureEvent& event,
                           const ui::LatencyInfo& latency) override;
  bool TransformPointToLocalCoordSpace(const gfx::Point& point,
                                       const viz::SurfaceId& original_surface,
                                       gfx::Point* transformed_point) override;
  bool TransformPointToCoordSpaceForView(
      const gfx::Point& point,
      RenderWidgetHostViewBase* target_view,
      gfx::Point* transformed_point) override;
  TouchSelectionControllerClientManager*
  GetTouchSelectionControllerClientManager() override;

  // ui::ViewClient implementation.
  bool OnTouchEvent(const ui::MotionEventAndroid& m,
                    bool for_touch_handle) override;
  bool OnMouseEvent(const ui::MotionEventAndroid& m) override;
  bool OnMouseWheelEvent(const ui::MotionEventAndroid& event) override;
  void OnPhysicalBackingSizeChanged() override;

  // ui::GestureProviderClient implementation.
  void OnGestureEvent(const ui::GestureEventData& gesture) override;

  // ui::WindowAndroidObserver implementation.
  void OnCompositingDidCommit() override {}
  void OnRootWindowVisibilityChanged(bool visible) override;
  void OnAttachCompositor() override;
  void OnDetachCompositor() override;
  void OnAnimate(base::TimeTicks begin_frame_time) override;
  void OnActivityStopped() override;
  void OnActivityStarted() override;

  // content::ContentViewCoreObserver implementation.
  void OnContentViewCoreDestroyed() override;
  void OnAttachedToWindow() override;
  void OnDetachedFromWindow() override;

  // viz::FrameEvictor implementation
  void EvictDelegatedFrame() override;

  // StylusTextSelectorClient implementation.
  void OnStylusSelectBegin(float x0, float y0, float x1, float y1) override;
  void OnStylusSelectUpdate(float x, float y) override;
  void OnStylusSelectEnd(float x, float y) override;
  void OnStylusSelectTap(base::TimeTicks time, float x, float y) override;

  // ui::TouchSelectionControllerClient implementation.
  bool SupportsAnimation() const override;
  void SetNeedsAnimate() override;
  void MoveCaret(const gfx::PointF& position) override;
  void MoveRangeSelectionExtent(const gfx::PointF& extent) override;
  void SelectBetweenCoordinates(const gfx::PointF& base,
                                const gfx::PointF& extent) override;
  void OnSelectionEvent(ui::SelectionEventType event) override;
  std::unique_ptr<ui::TouchHandleDrawable> CreateDrawable() override;

  // DelegatedFrameHostAndroid::Client implementation.
  void SetBeginFrameSource(cc::BeginFrameSource* begin_frame_source) override;
  void DidReceiveCompositorFrameAck() override;
  void ReclaimResources(
      const std::vector<cc::ReturnedResource>& resources) override;

  // cc::BeginFrameObserver implementation.
  void OnBeginFrame(const cc::BeginFrameArgs& args) override;
  const cc::BeginFrameArgs& LastUsedBeginFrameArgs() const override;
  void OnBeginFrameSourcePausedChanged(bool paused) override;

  // Non-virtual methods
  void SetContentViewCore(ContentViewCore* content_view_core);
  SkColor GetCachedBackgroundColor() const;
  void SendKeyEvent(const NativeWebKeyboardEvent& event);
  void SendMouseEvent(const ui::MotionEventAndroid&, int action_button);
  void SendMouseWheelEvent(const blink::WebMouseWheelEvent& event);
  void SendGestureEvent(const blink::WebGestureEvent& event);
  bool ShowSelectionMenu(const ContextMenuParams& params);
  void ResolveTapDisambiguation(double timestamp_seconds,
                                gfx::Point tap_viewport_offset,
                                bool is_long_press);
  void set_ime_adapter(ImeAdapterAndroid* ime_adapter) {
    ime_adapter_android_ = ime_adapter;
  }
  void set_selection_popup_controller(SelectionPopupController* controller) {
    selection_popup_controller_ = controller;
  }

  base::WeakPtr<RenderWidgetHostViewAndroid> GetWeakPtrAndroid();

  bool OnTouchEvent(const ui::MotionEvent& event);
  bool OnTouchHandleEvent(const ui::MotionEvent& event);
  int GetTouchHandleHeight();
  void ResetGestureDetection();
  void SetDoubleTapSupportEnabled(bool enabled);
  void SetMultiTouchZoomSupportEnabled(bool enabled);

  void WasResized();

  bool HasValidFrame() const;

  void MoveCaret(const gfx::Point& point);
  void ShowContextMenuAtPoint(const gfx::Point& point, ui::MenuSourceType);
  void DismissTextHandles();
  void SetTextHandlesTemporarilyHidden(bool hidden);
  void OnShowUnhandledTapUIIfNeeded(int x_dip, int y_dip);
  void OnSelectWordAroundCaretAck(bool did_select,
                                  int start_adjust,
                                  int end_adjust);

  void SynchronousFrameMetadata(cc::CompositorFrameMetadata frame_metadata);

  void SetSynchronousCompositorClient(SynchronousCompositorClient* client);

  SynchronousCompositorClient* synchronous_compositor_client() const {
    return synchronous_compositor_client_;
  }

  void OnOverscrollRefreshHandlerAvailable();

  static void OnContextLost();

  // TextInputManager::Observer overrides.
  void OnUpdateTextInputStateCalled(TextInputManager* text_input_manager,
                                    RenderWidgetHostViewBase* updated_view,
                                    bool did_change_state) override;
  void OnImeCompositionRangeChanged(
      TextInputManager* text_input_manager,
      RenderWidgetHostViewBase* updated_view) override;
  void OnImeCancelComposition(TextInputManager* text_input_manager,
                              RenderWidgetHostViewBase* updated_view) override;
  void OnTextSelectionChanged(TextInputManager* text_input_manager,
                              RenderWidgetHostViewBase* updated_view) override;

  ImeAdapterAndroid* ime_adapter_for_testing() { return ime_adapter_android_; }

  // Exposed for tests.
  viz::SurfaceId SurfaceIdForTesting() const override;

  ui::TouchSelectionControllerClient*
  GetSelectionControllerClientManagerForTesting();
  void SetSelectionControllerClientForTesting(
      std::unique_ptr<ui::TouchSelectionControllerClient> client);

  void GotFocus();
  void LostFocus();

 private:
  void RunAckCallbacks();

  void SendReclaimCompositorResources(bool is_swap_ack);

  void OnFrameMetadataUpdated(const cc::CompositorFrameMetadata& frame_metadata,
                              bool is_transparent);

  void ShowInternal();
  void HideInternal();
  void AttachLayers();
  void RemoveLayers();

  void UpdateBackgroundColor(SkColor color);

  void EvictFrameIfNecessary();

  // DevTools ScreenCast support for Android WebView.
  void SynchronousCopyContents(const gfx::Rect& src_subrect_in_pixel,
                               const gfx::Size& dst_size_in_pixel,
                               const ReadbackRequestCallback& callback,
                               const SkColorType color_type);

  void DestroyDelegatedContent();
  void OnLostResources();

  enum BeginFrameRequestType {
    BEGIN_FRAME = 1 << 0,
    PERSISTENT_BEGIN_FRAME = 1 << 1
  };
  void AddBeginFrameRequest(BeginFrameRequestType request);
  void ClearBeginFrameRequest(BeginFrameRequestType request);
  void AcknowledgeBeginFrame(const cc::BeginFrameAck& ack);
  void StartObservingRootWindow();
  void StopObservingRootWindow();
  void SendBeginFramePaused();
  void SendBeginFrame(cc::BeginFrameArgs args);
  bool Animate(base::TimeTicks frame_time);
  void RequestDisallowInterceptTouchEvent();

  bool SyncCompositorOnMessageReceived(const IPC::Message& message);

  void ComputeEventLatencyOSTouchHistograms(const ui::MotionEvent& event);

  void CreateOverscrollControllerIfPossible();

  void UpdateLeftClickCount(int action_button,
                            float mousedown_x,
                            float mouse_down_y);

  WebContentsAccessibilityAndroid* GetWebContentsAccessibilityAndroid() const;

  void OnFocusInternal();
  void LostFocusInternal();

  // The model object.
  RenderWidgetHostImpl* host_;

  // The begin frame source being observed.  Null if none.
  cc::BeginFrameSource* begin_frame_source_;
  cc::BeginFrameArgs last_begin_frame_args_;
  bool begin_frame_paused_ = false;

  // Indicates whether and for what reason a request for begin frames has been
  // issued. Used to control action dispatch at the next |OnBeginFrame()| call.
  uint32_t outstanding_begin_frame_requests_;

  bool is_showing_;

  // Window-specific bits that affect widget visibility.
  bool is_window_visible_;
  bool is_window_activity_started_;

  // Used to customize behavior for virtual reality mode, such as the
  // appearance of overscroll glow and the keyboard.
  bool is_in_vr_;

  // ContentViewCore is our interface to the view system.
  ContentViewCore* content_view_core_;

  ImeAdapterAndroid* ime_adapter_android_;
  SelectionPopupController* selection_popup_controller_;

  // The background color of the widget.
  SkColor background_color_;

  // Body background color of the underlying document.
  SkColor cached_background_color_;

  mutable ui::ViewAndroid view_;

  // Manages the Compositor Frames received from the renderer.
  std::unique_ptr<ui::DelegatedFrameHostAndroid> delegated_frame_host_;

  std::vector<cc::ReturnedResource> surface_returned_resources_;

  // The most recent surface size that was pushed to the surface layer.
  gfx::Size current_surface_size_;

  std::queue<base::Closure> ack_callbacks_;

  // Used to control and render overscroll-related effects.
  std::unique_ptr<OverscrollControllerAndroid> overscroll_controller_;

  // Provides gesture synthesis given a stream of touch events (derived from
  // Android MotionEvent's) and touch event acks.
  ui::FilteredGestureProvider gesture_provider_;

  // Handles gesture based text selection
  StylusTextSelector stylus_text_selector_;

  // Manages selection handle rendering and manipulation.
  // This will always be NULL if |content_view_core_| is NULL.
  std::unique_ptr<ui::TouchSelectionController> touch_selection_controller_;
  std::unique_ptr<ui::TouchSelectionControllerClient>
      touch_selection_controller_client_for_test_;
  // Keeps track of currently active touch selection controller clients (some
  // may be representing out-of-process iframes).
  std::unique_ptr<TouchSelectionControllerClientManagerAndroid>
      touch_selection_controller_client_manager_;

  // Bounds to use if we have no backing ContentViewCore
  gfx::Rect default_bounds_;

  const bool using_browser_compositor_;
  std::unique_ptr<SynchronousCompositorHost> sync_compositor_;

  SynchronousCompositorClient* synchronous_compositor_client_;

  std::unique_ptr<viz::FrameEvictor> frame_evictor_;

  bool observing_root_window_;

  // The last scroll offset of the view.
  gfx::Vector2dF last_scroll_offset_;

  float prev_top_shown_pix_;
  float prev_bottom_shown_pix_;

  base::TimeTicks prev_mousedown_timestamp_;
  gfx::Point prev_mousedown_point_;
  int left_click_count_ = 0;

  cc::mojom::CompositorFrameSinkClient* renderer_compositor_frame_sink_ =
      nullptr;

  base::ObserverList<DestructionObserver> destruction_observers_;

  MouseWheelPhaseHandler mouse_wheel_phase_handler_;

  base::WeakPtrFactory<RenderWidgetHostViewAndroid> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(RenderWidgetHostViewAndroid);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_ANDROID_H_
