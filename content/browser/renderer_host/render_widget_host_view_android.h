// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_ANDROID_H_
#define CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_ANDROID_H_

#include <map>
#include <queue>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/i18n/rtl.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/process/process.h"
#include "cc/layers/delegated_frame_resource_collection.h"
#include "cc/output/begin_frame_args.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/browser/renderer_host/delegated_frame_evictor.h"
#include "content/browser/renderer_host/ime_adapter_android.h"
#include "content/browser/renderer_host/input/stylus_text_selector.h"
#include "content/browser/renderer_host/input/touch_selection_controller.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/common/content_export.h"
#include "content/public/browser/readback_types.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/WebKit/public/platform/WebGraphicsContext3D.h"
#include "ui/base/android/window_android_observer.h"
#include "ui/events/gesture_detection/filtered_gesture_provider.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/vector2d_f.h"

struct ViewHostMsg_TextInputState_Params;

namespace cc {
class CopyOutputResult;
class DelegatedFrameProvider;
class DelegatedRendererLayer;
class Layer;
}

namespace blink {
class WebExternalTextureLayer;
class WebTouchEvent;
class WebMouseEvent;
}

namespace content {
class ContentViewCoreImpl;
class OverscrollControllerAndroid;
class RenderWidgetHost;
class RenderWidgetHostImpl;
struct DidOverscrollParams;
struct NativeWebKeyboardEvent;

class ReadbackRequest {
 public:
  explicit ReadbackRequest(float scale,
                           SkColorType color_type,
                           gfx::Rect src_subrect,
                           ReadbackRequestCallback& result_callback);
  ~ReadbackRequest();
  float GetScale() { return scale_; }
  SkColorType GetColorFormat() { return color_type_; }
  const gfx::Rect GetCaptureRect() { return src_subrect_; }
  ReadbackRequestCallback& GetResultCallback() { return result_callback_; }

 private:
  ReadbackRequest();
  float scale_;
  SkColorType color_type_;
  gfx::Rect src_subrect_;
  ReadbackRequestCallback result_callback_;
};

// -----------------------------------------------------------------------------
// See comments in render_widget_host_view.h about this class and its members.
// -----------------------------------------------------------------------------
class CONTENT_EXPORT RenderWidgetHostViewAndroid
    : public RenderWidgetHostViewBase,
      public cc::DelegatedFrameResourceCollectionClient,
      public ui::GestureProviderClient,
      public ui::WindowAndroidObserver,
      public DelegatedFrameEvictorClient,
      public StylusTextSelectorClient,
      public TouchSelectionControllerClient {
 public:
  RenderWidgetHostViewAndroid(RenderWidgetHostImpl* widget,
                              ContentViewCoreImpl* content_view_core);
  virtual ~RenderWidgetHostViewAndroid();

  // RenderWidgetHostView implementation.
  virtual bool OnMessageReceived(const IPC::Message& msg) override;
  virtual void InitAsChild(gfx::NativeView parent_view) override;
  virtual void InitAsPopup(RenderWidgetHostView* parent_host_view,
                           const gfx::Rect& pos) override;
  virtual void InitAsFullscreen(
      RenderWidgetHostView* reference_host_view) override;
  virtual RenderWidgetHost* GetRenderWidgetHost() const override;
  virtual void WasShown() override;
  virtual void WasHidden() override;
  virtual void SetSize(const gfx::Size& size) override;
  virtual void SetBounds(const gfx::Rect& rect) override;
  virtual gfx::Vector2dF GetLastScrollOffset() const override;
  virtual gfx::NativeView GetNativeView() const override;
  virtual gfx::NativeViewId GetNativeViewId() const override;
  virtual gfx::NativeViewAccessible GetNativeViewAccessible() override;
  virtual void MovePluginWindows(
      const std::vector<WebPluginGeometry>& moves) override;
  virtual void Focus() override;
  virtual void Blur() override;
  virtual bool HasFocus() const override;
  virtual bool IsSurfaceAvailableForCopy() const override;
  virtual void Show() override;
  virtual void Hide() override;
  virtual bool IsShowing() override;
  virtual gfx::Rect GetViewBounds() const override;
  virtual gfx::Size GetPhysicalBackingSize() const override;
  virtual bool DoTopControlsShrinkBlinkSize() const override;
  virtual float GetTopControlsHeight() const override;
  virtual void UpdateCursor(const WebCursor& cursor) override;
  virtual void SetIsLoading(bool is_loading) override;
  virtual void TextInputTypeChanged(ui::TextInputType type,
                                    ui::TextInputMode input_mode,
                                    bool can_compose_inline,
                                    int flags) override;
  virtual void ImeCancelComposition() override;
  virtual void ImeCompositionRangeChanged(
      const gfx::Range& range,
      const std::vector<gfx::Rect>& character_bounds) override;
  virtual void FocusedNodeChanged(bool is_editable_node) override;
  virtual void RenderProcessGone(base::TerminationStatus status,
                                 int error_code) override;
  virtual void Destroy() override;
  virtual void SetTooltipText(const base::string16& tooltip_text) override;
  virtual void SelectionChanged(const base::string16& text,
                                size_t offset,
                                const gfx::Range& range) override;
  virtual void SelectionBoundsChanged(
      const ViewHostMsg_SelectionBounds_Params& params) override;
  virtual void AcceleratedSurfaceInitialized(int route_id) override;
  virtual bool HasAcceleratedSurface(const gfx::Size& desired_size) override;
  virtual void SetBackgroundColor(SkColor color) override;
  virtual void CopyFromCompositingSurface(
      const gfx::Rect& src_subrect,
      const gfx::Size& dst_size,
      ReadbackRequestCallback& callback,
      const SkColorType color_type) override;
  virtual void CopyFromCompositingSurfaceToVideoFrame(
      const gfx::Rect& src_subrect,
      const scoped_refptr<media::VideoFrame>& target,
      const base::Callback<void(bool)>& callback) override;
  virtual bool CanCopyToVideoFrame() const override;
  virtual void GetScreenInfo(blink::WebScreenInfo* results) override;
  virtual gfx::Rect GetBoundsInRootWindow() override;
  virtual gfx::GLSurfaceHandle GetCompositingSurface() override;
  virtual void ProcessAckedTouchEvent(const TouchEventWithLatencyInfo& touch,
                                      InputEventAckState ack_result) override;
  virtual InputEventAckState FilterInputEvent(
      const blink::WebInputEvent& input_event) override;
  virtual void OnSetNeedsFlushInput() override;
  virtual void GestureEventAck(const blink::WebGestureEvent& event,
                               InputEventAckState ack_result) override;
  virtual BrowserAccessibilityManager* CreateBrowserAccessibilityManager(
      BrowserAccessibilityDelegate* delegate) override;
  virtual bool LockMouse() override;
  virtual void UnlockMouse() override;
  virtual void OnSwapCompositorFrame(
      uint32 output_surface_id,
      scoped_ptr<cc::CompositorFrame> frame) override;
  virtual void DidOverscroll(const DidOverscrollParams& params) override;
  virtual void DidStopFlinging() override;
  virtual void ShowDisambiguationPopup(const gfx::Rect& rect_pixels,
                                       const SkBitmap& zoomed_bitmap) override;
  virtual scoped_ptr<SyntheticGestureTarget> CreateSyntheticGestureTarget()
      override;
  virtual void LockCompositingSurface() override;
  virtual void UnlockCompositingSurface() override;
  virtual void OnTextSurroundingSelectionResponse(const base::string16& content,
                                                  size_t start_offset,
                                                  size_t end_offset) override;

  // cc::DelegatedFrameResourceCollectionClient implementation.
  virtual void UnusedResourcesAreAvailable() override;

  // ui::GestureProviderClient implementation.
  virtual void OnGestureEvent(const ui::GestureEventData& gesture) override;

  // ui::WindowAndroidObserver implementation.
  virtual void OnCompositingDidCommit() override;
  virtual void OnAttachCompositor() override;
  virtual void OnDetachCompositor() override;
  virtual void OnVSync(base::TimeTicks frame_time,
                       base::TimeDelta vsync_period) override;
  virtual void OnAnimate(base::TimeTicks begin_frame_time) override;

  // DelegatedFrameEvictor implementation
  virtual void EvictDelegatedFrame() override;

  virtual SkColorType PreferredReadbackFormat() override;

  // StylusTextSelectorClient implementation.
  void OnStylusSelectBegin(float x0, float y0, float x1, float y1) override;
  void OnStylusSelectUpdate(float x, float y) override;
  void OnStylusSelectEnd() override;
  void OnStylusSelectTap(base::TimeTicks time, float x, float y) override;

  // TouchSelectionControllerClient implementation.
  virtual bool SupportsAnimation() const override;
  virtual void SetNeedsAnimate() override;
  virtual void MoveCaret(const gfx::PointF& position) override;
  virtual void MoveRangeSelectionExtent(const gfx::PointF& extent) override;
  virtual void SelectBetweenCoordinates(const gfx::PointF& base,
                                        const gfx::PointF& extent) override;
  virtual void OnSelectionEvent(SelectionEventType event,
                                const gfx::PointF& anchor_position) override;
  virtual scoped_ptr<TouchHandleDrawable> CreateDrawable() override;

  // Non-virtual methods
  void SetContentViewCore(ContentViewCoreImpl* content_view_core);
  SkColor GetCachedBackgroundColor() const;
  void SendKeyEvent(const NativeWebKeyboardEvent& event);
  void SendMouseEvent(const blink::WebMouseEvent& event);
  void SendMouseWheelEvent(const blink::WebMouseWheelEvent& event);
  void SendGestureEvent(const blink::WebGestureEvent& event);

  void OnTextInputStateChanged(const ViewHostMsg_TextInputState_Params& params);
  void OnDidChangeBodyBackgroundColor(SkColor color);
  void OnStartContentIntent(const GURL& content_url);
  void OnSetNeedsBeginFrames(bool enabled);
  void OnSmartClipDataExtracted(const base::string16& text,
                                const base::string16& html,
                                const gfx::Rect rect);

  bool OnTouchEvent(const ui::MotionEvent& event);
  bool OnTouchHandleEvent(const ui::MotionEvent& event);
  void ResetGestureDetection();
  void SetDoubleTapSupportEnabled(bool enabled);
  void SetMultiTouchZoomSupportEnabled(bool enabled);

  long GetNativeImeAdapter();

  void WasResized();

  void GetScaledContentBitmap(float scale,
                              SkColorType color_type,
                              gfx::Rect src_subrect,
                              ReadbackRequestCallback& result_callback);

  scoped_refptr<cc::DelegatedRendererLayer>
      CreateDelegatedLayerForFrameProvider() const;

  bool HasValidFrame() const;

  void MoveCaret(const gfx::Point& point);
  void DismissTextHandles();
  void SetTextHandlesTemporarilyHidden(bool hidden);
  void OnShowingPastePopup(const gfx::PointF& point);

  void SynchronousFrameMetadata(
      const cc::CompositorFrameMetadata& frame_metadata);

  void SetOverlayVideoMode(bool enabled);

  typedef base::Callback<
      void(const base::string16& content, int start_offset, int end_offset)>
      TextSurroundingSelectionCallback;
  void SetTextSurroundingSelectionCallback(
      const TextSurroundingSelectionCallback& callback);

  static void OnContextLost();

 private:
  void RunAckCallbacks();

  void DestroyDelegatedContent();
  void SwapDelegatedFrame(uint32 output_surface_id,
                          scoped_ptr<cc::DelegatedFrameData> frame_data);
  void SendDelegatedFrameAck(uint32 output_surface_id);
  void SendReturnedDelegatedResources(uint32 output_surface_id);

  void OnFrameMetadataUpdated(
      const cc::CompositorFrameMetadata& frame_metadata);
  void ComputeContentsSize(const cc::CompositorFrameMetadata& frame_metadata);

  void AttachLayers();
  void RemoveLayers();

  // Called after async screenshot task completes. Scales and crops the result
  // of the copy.
  static void PrepareTextureCopyOutputResult(
      const gfx::Size& dst_size_in_pixel,
      const SkColorType color_type,
      const base::TimeTicks& start_time,
      ReadbackRequestCallback& callback,
      scoped_ptr<cc::CopyOutputResult> result);
  static void PrepareTextureCopyOutputResultForDelegatedReadback(
      const gfx::Size& dst_size_in_pixel,
      const SkColorType color_type,
      const base::TimeTicks& start_time,
      scoped_refptr<cc::Layer> readback_layer,
      ReadbackRequestCallback& callback,
      scoped_ptr<cc::CopyOutputResult> result);

  // DevTools ScreenCast support for Android WebView.
  void SynchronousCopyContents(const gfx::Rect& src_subrect_in_pixel,
                               const gfx::Size& dst_size_in_pixel,
                               ReadbackRequestCallback& callback,
                               const SkColorType color_type);

  // If we have locks on a frame during a ContentViewCore swap or a context
  // lost, the frame is no longer valid and we can safely release all the locks.
  // Use this method to release all the locks.
  void ReleaseLocksOnSurface();

  // Drop any incoming frames from the renderer when there are locks on the
  // current frame.
  void RetainFrame(uint32 output_surface_id,
                   scoped_ptr<cc::CompositorFrame> frame);

  void InternalSwapCompositorFrame(uint32 output_surface_id,
                                   scoped_ptr<cc::CompositorFrame> frame);
  void OnLostResources();

  enum VSyncRequestType {
    FLUSH_INPUT = 1 << 0,
    BEGIN_FRAME = 1 << 1,
    PERSISTENT_BEGIN_FRAME = 1 << 2
  };
  void RequestVSyncUpdate(uint32 requests);
  void StartObservingRootWindow();
  void StopObservingRootWindow();
  void SendBeginFrame(base::TimeTicks frame_time, base::TimeDelta vsync_period);
  bool Animate(base::TimeTicks frame_time);

  // Handles all unprocessed and pending readback requests.
  void AbortPendingReadbackRequests();

  // The model object.
  RenderWidgetHostImpl* host_;

  // Used to control action dispatch at the next |OnVSync()| call.
  uint32 outstanding_vsync_requests_;

  bool is_showing_;

  // ContentViewCoreImpl is our interface to the view system.
  ContentViewCoreImpl* content_view_core_;

  ImeAdapterAndroid ime_adapter_android_;

  // Body background color of the underlying document.
  SkColor cached_background_color_;

  scoped_refptr<cc::DelegatedFrameResourceCollection> resource_collection_;
  scoped_refptr<cc::DelegatedFrameProvider> frame_provider_;
  scoped_refptr<cc::DelegatedRendererLayer> layer_;

  // The most recent texture size that was pushed to the texture layer.
  gfx::Size texture_size_in_layer_;

  // The most recent content size that was pushed to the texture layer.
  gfx::Size content_size_in_layer_;

  // The output surface id of the last received frame.
  uint32_t last_output_surface_id_;


  std::queue<base::Closure> ack_callbacks_;

  // Used to control and render overscroll-related effects.
  scoped_ptr<OverscrollControllerAndroid> overscroll_controller_;

  // Provides gesture synthesis given a stream of touch events (derived from
  // Android MotionEvent's) and touch event acks.
  ui::FilteredGestureProvider gesture_provider_;

  // Handles gesture based text selection
  StylusTextSelector stylus_text_selector_;

  // Manages selection handle rendering and manipulation.
  // This will always be NULL if |content_view_core_| is NULL.
  scoped_ptr<TouchSelectionController> selection_controller_;

  int accelerated_surface_route_id_;

  // Size to use if we have no backing ContentViewCore
  gfx::Size default_size_;

  const bool using_browser_compositor_;

  scoped_ptr<DelegatedFrameEvictor> frame_evictor_;

  size_t locks_on_frame_count_;
  bool observing_root_window_;

  struct LastFrameInfo {
    LastFrameInfo(uint32 output_id,
                  scoped_ptr<cc::CompositorFrame> output_frame);
    ~LastFrameInfo();
    uint32 output_surface_id;
    scoped_ptr<cc::CompositorFrame> frame;
  };

  scoped_ptr<LastFrameInfo> last_frame_info_;

  TextSurroundingSelectionCallback text_surrounding_selection_callback_;

  // List of readbackrequests waiting for arrival of a valid frame.
  std::queue<ReadbackRequest> readbacks_waiting_for_frame_;

  // The last scroll offset of the view.
  gfx::Vector2dF last_scroll_offset_;

  base::WeakPtrFactory<RenderWidgetHostViewAndroid> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(RenderWidgetHostViewAndroid);
};

} // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_ANDROID_H_
