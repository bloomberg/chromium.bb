// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FRAME_HOST_RENDER_WIDGET_HOST_VIEW_GUEST_H_
#define CONTENT_BROWSER_FRAME_HOST_RENDER_WIDGET_HOST_VIEW_GUEST_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <vector>

#include "base/macros.h"
#include "build/build_config.h"
#include "content/browser/renderer_host/render_widget_host_view_child_frame.h"
#include "content/common/content_export.h"
#include "content/common/cursors/webcursor.h"
#include "third_party/blink/public/platform/web_input_event.h"
#include "ui/events/event.h"
#include "ui/events/gestures/gesture_recognizer.h"
#include "ui/events/gestures/gesture_types.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/vector2d_f.h"
#include "ui/gfx/native_widget_types.h"

namespace base {
class UnguessableToken;
}

namespace content {
class BrowserPluginGuest;
class RenderWidgetHost;
class RenderWidgetHostImpl;
struct TextInputState;

// See comments in render_widget_host_view.h about this class and its members.
// This version is for the BrowserPlugin which handles a lot of the
// functionality in a diffent place and isn't platform specific.
// The BrowserPlugin is currently a special case for out-of-process rendered
// content and therefore inherits from RenderWidgetHostViewChildFrame.
// Eventually all RenderWidgetHostViewGuest code will be subsumed by
// RenderWidgetHostViewChildFrame and this class will be removed.
//
// Some elements that are platform specific will be deal with by delegating
// the relevant calls to the platform view.
class CONTENT_EXPORT RenderWidgetHostViewGuest
    : public RenderWidgetHostViewChildFrame,
      public ui::GestureConsumer {
 public:
  static RenderWidgetHostViewGuest* Create(
      RenderWidgetHost* widget,
      BrowserPluginGuest* guest,
      base::WeakPtr<RenderWidgetHostViewBase> platform_view);
  static RenderWidgetHostViewBase* GetRootView(RenderWidgetHostViewBase* rwhv);

  ~RenderWidgetHostViewGuest() override;

  bool OnMessageReceivedFromEmbedder(const IPC::Message& message,
                                     RenderWidgetHostImpl* embedder);

  // Called when this RenderWidgetHostViewGuest is attached.
  void OnAttached();

  // RenderWidgetHostView implementation.
  bool OnMessageReceived(const IPC::Message& msg) override;
  void InitAsChild(gfx::NativeView parent_view) override;
  void SetSize(const gfx::Size& size) override;
  void SetBounds(const gfx::Rect& rect) override;
  void Focus() override;
  bool HasFocus() const override;
  void Show() override;
  void Hide() override;
  gfx::NativeView GetNativeView() const override;
  gfx::NativeViewAccessible GetNativeViewAccessible() override;
  gfx::Rect GetViewBounds() const override;
  gfx::Rect GetBoundsInRootWindow() override;
  gfx::Size GetCompositorViewportPixelSize() const override;
  base::string16 GetSelectedText() override;
  void SetNeedsBeginFrames(bool needs_begin_frames) override;
  TouchSelectionControllerClientManager*
  GetTouchSelectionControllerClientManager() override;
  gfx::PointF TransformPointToRootCoordSpaceF(
      const gfx::PointF& point) override;
  bool TransformPointToLocalCoordSpace(const gfx::PointF& point,
                                       const viz::SurfaceId& original_surface,
                                       gfx::PointF* transformed_point) override;
  gfx::PointF TransformRootPointToViewCoordSpace(
      const gfx::PointF& point) override;

  // RenderWidgetHostViewBase implementation.
  void InitAsPopup(RenderWidgetHostView* parent_host_view,
                   const gfx::Rect& bounds) override;
  void InitAsFullscreen(RenderWidgetHostView* reference_host_view) override;
  void UpdateCursor(const WebCursor& cursor) override;
  void SetIsLoading(bool is_loading) override;
  bool HasSize() const override;
  void TextInputStateChanged(const TextInputState& params) override;
  void ImeCancelComposition() override;
#if defined(OS_MACOSX) || defined(USE_AURA)
  void ImeCompositionRangeChanged(
      const gfx::Range& range,
      const std::vector<gfx::Rect>& character_bounds) override;
#endif
  void RenderProcessGone(base::TerminationStatus status,
                         int error_code) override;
  void Destroy() override;
  void SetTooltipText(const base::string16& tooltip_text) override;
  void SelectionChanged(const base::string16& text,
                        size_t offset,
                        const gfx::Range& range) override;
  void SelectionBoundsChanged(
      const ViewHostMsg_SelectionBounds_Params& params) override;
#if defined(USE_AURA)
  void ProcessAckedTouchEvent(const TouchEventWithLatencyInfo& touch,
                              InputEventAckState ack_result) override;
#endif
  void PreProcessMouseEvent(const blink::WebMouseEvent& event) override;
  void PreProcessTouchEvent(const blink::WebTouchEvent& event) override;

  void DidStopFlinging() override;
  bool LockMouse() override;
  void UnlockMouse() override;
  viz::LocalSurfaceId GetLocalSurfaceId() const override;
  void DidCreateNewRendererCompositorFrameSink(
      viz::mojom::CompositorFrameSinkClient* renderer_compositor_frame_sink)
      override;

#if defined(OS_MACOSX)
  // RenderWidgetHostView implementation.
  void SetActive(bool active) override;
  void ShowDefinitionForSelection() override;
  void SpeakSelection() override;
#endif  // defined(OS_MACOSX)

  void WheelEventAck(const blink::WebMouseWheelEvent& event,
                     InputEventAckState ack_result) override;

  void GestureEventAck(const blink::WebGestureEvent& event,
                       InputEventAckState ack_result) override;

  InputEventAckState FilterInputEvent(
      const blink::WebInputEvent& input_event) override;

  bool IsRenderWidgetHostViewGuest() override;
  RenderWidgetHostViewBase* GetOwnerRenderWidgetHostView() const;

  void GetScreenInfo(ScreenInfo* screen_info) const override;

  void EnableAutoResize(const gfx::Size& min_size,
                        const gfx::Size& max_size) override;
  void DisableAutoResize(const gfx::Size& new_size) override;

  viz::ScopedSurfaceIdAllocator ResizeDueToAutoResize(
      const gfx::Size& new_size,
      uint64_t sequence_number) override;

 private:
  friend class RenderWidgetHostView;

  void SendSurfaceInfoToEmbedderImpl(
      const viz::SurfaceInfo& surface_info) override;

  RenderWidgetHostViewGuest(
      RenderWidgetHost* widget,
      BrowserPluginGuest* guest,
      base::WeakPtr<RenderWidgetHostViewBase> platform_view);

  // Since we now route GestureEvents directly to the guest renderer, we need
  // a way to make sure that the BrowserPlugin in the embedder gets focused so
  // that keyboard input (which still travels via BrowserPlugin) is routed to
  // the plugin and thus onwards to the guest.
  // TODO(wjmaclean): When we remove BrowserPlugin, delete this code.
  // http://crbug.com/533069
  void MaybeSendSyntheticTapGesture(
      const blink::WebFloatPoint& position,
      const blink::WebFloatPoint& screenPosition) const;

  void OnHandleInputEvent(RenderWidgetHostImpl* embedder,
                          int browser_plugin_instance_id,
                          const blink::WebInputEvent* event);

  bool HasEmbedderChanged() override;

#if defined(USE_AURA)
  void OnGotEmbedToken(const base::UnguessableToken& token);
#endif

  // BrowserPluginGuest and RenderWidgetHostViewGuest's lifetimes are not tied
  // to one another, therefore we access |guest_| through WeakPtr.
  base::WeakPtr<BrowserPluginGuest> guest_;

  // The platform view for this RenderWidgetHostView.
  // RenderWidgetHostViewGuest mostly only cares about stuff related to
  // compositing, the rest are directly forwarded to this |platform_view_|.
  base::WeakPtr<RenderWidgetHostViewBase> platform_view_;

  // When true the guest will forward its selection updates to the owner RWHV.
  // The guest may forward its updates only when there is an ongoing IME
  // session.
  bool should_forward_text_selection_ = false;

  base::WeakPtrFactory<RenderWidgetHostViewGuest> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(RenderWidgetHostViewGuest);
};

}  // namespace content

#endif  // CONTENT_BROWSER_FRAME_HOST_RENDER_WIDGET_HOST_VIEW_GUEST_H_
