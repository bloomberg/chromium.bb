// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FRAME_HOST_CROSS_PROCESS_FRAME_CONNECTOR_H_
#define CONTENT_BROWSER_FRAME_HOST_CROSS_PROCESS_FRAME_CONNECTOR_H_

#include <stdint.h>

#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/common/surfaces/local_surface_id.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "content/browser/renderer_host/frame_connector_delegate.h"
#include "content/common/content_export.h"
#include "content/common/frame_resize_params.h"

namespace IPC {
class Message;
}

namespace content {
class RenderFrameProxyHost;

// CrossProcessFrameConnector provides the platform view abstraction for
// RenderWidgetHostViewChildFrame allowing RWHVChildFrame to remain ignorant
// of RenderFrameHost.
//
// The RenderWidgetHostView of an out-of-process child frame needs to
// communicate with the RenderFrameProxyHost representing this frame in the
// process of the parent frame. For example, assume you have this page:
//
//   -----------------
//   | frame 1       |
//   |  -----------  |
//   |  | frame 2 |  |
//   |  -----------  |
//   -----------------
//
// If frames 1 and 2 are in process A and B, there are 4 hosts:
//   A1 - RFH for frame 1 in process A
//   B1 - RFPH for frame 1 in process B
//   A2 - RFPH for frame 2 in process A
//   B2 - RFH for frame 2 in process B
//
// B2, having a parent frame in a different process, will have a
// RenderWidgetHostViewChildFrame. This RenderWidgetHostViewChildFrame needs
// to communicate with A2 because the embedding frame represents the platform
// that the child frame is rendering into -- it needs information necessary for
// compositing child frame textures, and also can pass platform messages such as
// view resizing. CrossProcessFrameConnector bridges between B2's
// RenderWidgetHostViewChildFrame and A2 to allow for this communication.
// (Note: B1 is only mentioned for completeness. It is not needed in this
// example.)
//
// CrossProcessFrameConnector objects are owned by the RenderFrameProxyHost
// in the child frame's RenderFrameHostManager corresponding to the parent's
// SiteInstance, A2 in the picture above. When a child frame navigates in a new
// process, SetView() is called to update to the new view.
//
class CONTENT_EXPORT CrossProcessFrameConnector
    : public FrameConnectorDelegate {
 public:
  // |frame_proxy_in_parent_renderer| corresponds to A2 in the example above.
  explicit CrossProcessFrameConnector(
      RenderFrameProxyHost* frame_proxy_in_parent_renderer);
  ~CrossProcessFrameConnector() override;

  bool OnMessageReceived(const IPC::Message &msg);

  // |view| corresponds to B2's RenderWidgetHostViewChildFrame in the example
  // above.
  RenderWidgetHostViewChildFrame* get_view_for_testing() { return view_; }

  // FrameConnectorDelegate implementation.
  void SetView(RenderWidgetHostViewChildFrame* view) override;
  RenderWidgetHostViewBase* GetParentRenderWidgetHostView() override;
  RenderWidgetHostViewBase* GetRootRenderWidgetHostView() override;
  void RenderProcessGone() override;
  void SetChildFrameSurface(const viz::SurfaceInfo& surface_info) override;
  void SendIntrinsicSizingInfoToParent(
      const blink::WebIntrinsicSizingInfo&) override;

  void UpdateCursor(const WebCursor& cursor) override;
  gfx::PointF TransformPointToRootCoordSpace(
      const gfx::PointF& point,
      const viz::SurfaceId& surface_id) override;
  bool TransformPointToLocalCoordSpace(const gfx::PointF& point,
                                       const viz::SurfaceId& original_surface,
                                       const viz::SurfaceId& local_surface_id,
                                       gfx::PointF* transformed_point) override;
  bool TransformPointToCoordSpaceForView(
      const gfx::PointF& point,
      RenderWidgetHostViewBase* target_view,
      const viz::SurfaceId& local_surface_id,
      gfx::PointF* transformed_point) override;
  void ForwardProcessAckedTouchEvent(const TouchEventWithLatencyInfo& touch,
                                     InputEventAckState ack_result) override;
  void BubbleScrollEvent(const blink::WebGestureEvent& event) override;
  bool HasFocus() override;
  void FocusRootView() override;
  bool LockMouse() override;
  void UnlockMouse() override;
  void EnableAutoResize(const gfx::Size& min_size,
                        const gfx::Size& max_size) override;
  void DisableAutoResize() override;
  bool IsInert() const override;
  bool IsHidden() const override;
  bool IsThrottled() const override;
  bool IsSubtreeThrottled() const override;
#if defined(USE_AURA)
  void EmbedRendererWindowTreeClientInParent(
      ui::mojom::WindowTreeClientPtr window_tree_client) override;
#endif
  void ResizeDueToAutoResize(const gfx::Size& new_size,
                             uint64_t sequence_number) override;

  // Set the visibility of immediate child views, i.e. views whose parent view
  // is |view_|.
  void SetVisibilityForChildViews(bool visible) const override;

  void SetScreenSpaceRect(const gfx::Rect& screen_space_rect) override;

  // Exposed for tests.
  RenderWidgetHostViewBase* GetRootRenderWidgetHostViewForTesting() {
    return GetRootRenderWidgetHostView();
  }

 private:
  friend class MockCrossProcessFrameConnector;

  // Resets the rect and the viz::LocalSurfaceId of the connector to ensure the
  // unguessable surface ID is not reused after a cross-process navigation.
  void ResetScreenSpaceRect();

  // Handlers for messages received from the parent frame.
  void OnUpdateResizeParams(const viz::SurfaceId& surface_id,
                            const FrameResizeParams& frame_resize_params);
  void OnUpdateViewportIntersection(const gfx::Rect& viewport_intersection,
                                    const gfx::Rect& compositor_visible_rect);
  void OnVisibilityChanged(bool visible);
  void OnSetIsInert(bool);
  void OnUpdateRenderThrottlingStatus(bool is_throttled,
                                      bool subtree_throttled);

  // The RenderFrameProxyHost that routes messages to the parent frame's
  // renderer process.
  RenderFrameProxyHost* frame_proxy_in_parent_renderer_;

  bool is_inert_ = false;

  bool is_throttled_ = false;
  bool subtree_throttled_ = false;

  // Visibility state of the corresponding frame owner element in parent process
  // which is set through CSS.
  bool is_hidden_ = false;

  bool is_scroll_bubbling_;

  // The last pre-transform frame size received from the parent renderer.
  // |last_received_local_frame_size_| may be in DIP if use zoom for DSF is
  // off.
  gfx::Size last_received_local_frame_size_;

  DISALLOW_COPY_AND_ASSIGN(CrossProcessFrameConnector);
};

}  // namespace content

#endif  // CONTENT_BROWSER_FRAME_HOST_CROSS_PROCESS_FRAME_CONNECTOR_H_

