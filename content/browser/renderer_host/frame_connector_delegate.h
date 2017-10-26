// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_FRAME_CONNECTOR_DELEGATE_H_
#define CONTENT_BROWSER_RENDERER_HOST_FRAME_CONNECTOR_DELEGATE_H_

#include "components/viz/common/surfaces/local_surface_id.h"
#include "content/browser/renderer_host/event_with_latency_info.h"
#include "content/common/content_export.h"
#include "content/public/common/input_event_ack_state.h"
#include "ui/gfx/geometry/rect.h"

#if defined(USE_AURA)
#include "services/ui/public/interfaces/window_tree.mojom.h"
#endif

namespace blink {
class WebGestureEvent;
}

namespace viz {
class SurfaceId;
class SurfaceInfo;
struct SurfaceSequence;
}  // namespace viz

namespace content {
class RenderWidgetHostViewBase;
class RenderWidgetHostViewChildFrame;
class WebCursor;

//
// FrameConnectorDelegate
//
// An interface to be implemented by an object supplying platform semantics
// for a child frame.
//
// A RenderWidgetHostViewChildFrame, specified by a call to |SetView|, uses
// this interface to communicate renderer-originating messages such as mouse
// cursor changes or input event ACKs to its platform.
// CrossProcessFrameConnector implements this interface and coordinates with
// higher-level RenderWidgetHostViews to ensure that the underlying platform
// (e.g. Mac, Aura, Android) correctly reflects state from frames in multiple
// processes.
//
// RenderWidgetHostViewChildFrame also uses this interface to query relevant
// platform information, such as the size of the rect that the frame will draw
// into, and whether the view currently has keyboard focus.
class CONTENT_EXPORT FrameConnectorDelegate {
 public:
  virtual void SetView(RenderWidgetHostViewChildFrame* view) {}

  // Returns the parent RenderWidgetHostView or nullptr if it doesn't have one.
  virtual RenderWidgetHostViewBase* GetParentRenderWidgetHostView();

  // Returns the view for the top-level frame under the same WebContents.
  virtual RenderWidgetHostViewBase* GetRootRenderWidgetHostView();

  // Notify the frame connector that the renderer process has terminated.
  virtual void RenderProcessGone() {}

  // Provide the SurfaceInfo to the embedder, which becomes a reference to the
  // current view's Surface that is included in higher-level compositor
  // frames.
  virtual void SetChildFrameSurface(const viz::SurfaceInfo& surface_info,
                                    const viz::SurfaceSequence& sequence) {}

  // Return the rect that the RenderWidgetHostViewChildFrame's content will
  // render into.
  virtual gfx::Rect ChildFrameRect();

  // Request that the platform change the mouse cursor when the mouse is
  // positioned over this view's content.
  virtual void UpdateCursor(const WebCursor& cursor) {}

  // Given a point in the current view's coordinate space, return the same
  // point transformed into the coordinate space of the top-level view's
  // coordinate space.
  virtual gfx::PointF TransformPointToRootCoordSpace(
      const gfx::PointF& point,
      const viz::SurfaceId& surface_id);

  // Given a point in the coordinate space of a different Surface, transform
  // it into the coordinate space for this view (corresponding to
  // local_surface_id).
  // TransformPointToLocalCoordSpace() can only transform points between
  // surfaces where one is embedded (not necessarily directly) within the
  // other, and will return false if this is not the case. For points that can
  // be in sibling surfaces, they must first be converted to the root
  // surface's coordinate space.
  virtual bool TransformPointToLocalCoordSpace(
      const gfx::PointF& point,
      const viz::SurfaceId& original_surface,
      const viz::SurfaceId& local_surface_id,
      gfx::PointF* transformed_point);

  // Transform a point into the coordinate space of the root
  // RenderWidgetHostView, for the current view's coordinate space.
  // Returns false if |target_view| and |view_| do not have the same root
  // RenderWidgetHostView.
  virtual bool TransformPointToCoordSpaceForView(
      const gfx::PointF& point,
      RenderWidgetHostViewBase* target_view,
      const viz::SurfaceId& local_surface_id,
      gfx::PointF* transformed_point);

  // Pass acked touch events to the root view for gesture processing.
  virtual void ForwardProcessAckedTouchEvent(
      const TouchEventWithLatencyInfo& touch,
      InputEventAckState ack_result) {}

  // Gesture events with unused scroll deltas must be bubbled to ancestors
  // who may consume the delta.
  virtual void BubbleScrollEvent(const blink::WebGestureEvent& event) {}

  // Determines whether the root RenderWidgetHostView (and thus the current
  // page) has focus.
  virtual bool HasFocus();

  // Cause the root RenderWidgetHostView to become focused.
  virtual void FocusRootView() {}

  // Locks the mouse. Returns true if mouse is locked.
  virtual bool LockMouse();

  // Unlocks the mouse if the mouse is locked.
  virtual void UnlockMouse() {}

  // Returns a rect that represents the intersection of the current view's
  // content bounds with the top-level browser viewport.
  const gfx::Rect& ViewportIntersection() const {
    return viewport_intersection_rect_;
  }

  // Returns the viz::LocalSurfaceId propagated from the parent to be used by
  // this child frame.
  const viz::LocalSurfaceId& local_surface_id() const {
    return local_surface_id_;
  }

  // Determines whether the current view's content is inert, either because
  // an HTMLDialogElement is being modally displayed in a higher-level frame,
  // or because the inert attribute has been specified.
  virtual bool IsInert() const;

  // Determines whether the RenderWidgetHostViewChildFrame is hidden due to
  // a higher-level embedder being hidden. This is distinct from the
  // RenderWidgetHostImpl being hidden, which is a property set when
  // RenderWidgetHostView::Hide() is called on the current view.
  virtual bool IsHidden() const;

  // Called by RenderWidgetHostViewChildFrame to update the visibility of any
  // nested child RWHVCFs inside it.
  virtual void SetVisibilityForChildViews(bool visible) const {}

#if defined(USE_AURA)
  // Embeds a WindowTreeClient in the parent. This results in the parent
  // creating a window in the ui server so that this can render to the screen.
  virtual void EmbedRendererWindowTreeClientInParent(
      ui::mojom::WindowTreeClientPtr window_tree_client) {}
#endif

 protected:
  virtual ~FrameConnectorDelegate() {}

  // This is here rather than in the implementation class so that
  // ViewportIntersection() can return a reference.
  gfx::Rect viewport_intersection_rect_;

  viz::LocalSurfaceId local_surface_id_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_FRAME_CONNECTOR_DELEGATE_H_
