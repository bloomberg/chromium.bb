// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_widget_host_input_event_router.h"

#include <vector>

#include "base/metrics/histogram_macros.h"
#include "components/viz/common/quads/surface_draw_quad.h"
#include "components/viz/service/surfaces/surface_manager.h"
#include "content/browser/frame_host/render_widget_host_view_guest.h"
#include "content/browser/renderer_host/cursor_manager.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/browser/renderer_host/render_widget_host_view_child_frame.h"
#include "content/common/frame_messages.h"
#include "third_party/WebKit/public/platform/WebInputEvent.h"
#include "ui/events/blink/web_input_event_traits.h"

namespace {

void TransformEventTouchPositions(blink::WebTouchEvent* event,
                                  const gfx::Vector2d& delta) {
  for (unsigned i = 0; i < event->touches_length; ++i) {
    event->touches[i].SetPositionInWidget(
        event->touches[i].PositionInWidget().x + delta.x(),
        event->touches[i].PositionInWidget().y + delta.y());
  }
}

blink::WebGestureEvent DummyGestureScrollUpdate(double timeStampSeconds) {
  return blink::WebGestureEvent(blink::WebInputEvent::kGestureScrollUpdate,
                                blink::WebInputEvent::kNoModifiers,
                                timeStampSeconds);
}

}  // anonymous namespace

namespace content {

void RenderWidgetHostInputEventRouter::OnRenderWidgetHostViewBaseDestroyed(
    RenderWidgetHostViewBase* view) {
  // RenderWidgetHostViewBase::RemoveObserver() should only ever be called
  // in this function, except during the shutdown of this class. This prevents
  // removal of an observed view that is being tracked as an event target
  // without cleaning up dangling pointers to it.
  view->RemoveObserver(this);

  // Remove this view from the owner_map.
  for (auto entry : owner_map_) {
    if (entry.second == view) {
      owner_map_.erase(entry.first);
      // There will only be one instance of a particular view in the map.
      break;
    }
  }

  if (view == touch_target_.target) {
    touch_target_.target = nullptr;
    active_touches_ = 0;
  }

  if (view == wheel_target_.target)
    wheel_target_.target = nullptr;

  // If the target that's being destroyed is in the gesture target map, we
  // replace it with nullptr so that we maintain the 1:1 correspondence between
  // map entries and the touch sequences that underly them.
  for (auto it : touchscreen_gesture_target_map_) {
    if (it.second.target == view)
      it.second.target = nullptr;
  }

  if (view == mouse_capture_target_.target)
    mouse_capture_target_.target = nullptr;

  if (view == touchscreen_gesture_target_.target)
    touchscreen_gesture_target_.target = nullptr;

  if (view == touchpad_gesture_target_.target)
    touchpad_gesture_target_.target = nullptr;

  if (view == bubbling_gesture_scroll_target_.target) {
    bubbling_gesture_scroll_target_.target = nullptr;
    first_bubbling_scroll_target_.target = nullptr;
  } else if (view == first_bubbling_scroll_target_.target) {
    first_bubbling_scroll_target_.target = nullptr;
    // When wheel scroll latching is disabled
    // bubbling_gesture_scroll_target_.target should also get reset since
    // gesture scroll events are bubbled one target at a time and they need the
    // first target for getting bubbled to the current bubbling target. With
    // latching enabled gesture scroll events (other than GSB) are bubbled
    // directly to the bubbling target, the bubbling target should wait for the
    // GSE to arrive and finish scrolling sequence rather than getting reset.
    if (bubbling_gesture_scroll_target_.target &&
        !bubbling_gesture_scroll_target_.target
             ->wheel_scroll_latching_enabled()) {
      bubbling_gesture_scroll_target_.target = nullptr;
    }
  }

  if (view == last_mouse_move_target_) {
    // When a child iframe is destroyed, consider its parent to be to be the
    // most recent target, if possible. In some cases the parent might already
    // have been destroyed, in which case the last target is cleared.
    if (view != last_mouse_move_root_view_)
      last_mouse_move_target_ =
          static_cast<RenderWidgetHostViewChildFrame*>(last_mouse_move_target_)
              ->GetParentView();
    else
      last_mouse_move_target_ = nullptr;

    if (!last_mouse_move_target_ || view == last_mouse_move_root_view_)
      last_mouse_move_root_view_ = nullptr;
  }
}

void RenderWidgetHostInputEventRouter::ClearAllObserverRegistrations() {
  // Since we're shutting down, it's safe to call RenderWidgetHostViewBase::
  // RemoveObserver() directly here.
  for (auto entry : owner_map_)
    entry.second->RemoveObserver(this);
  owner_map_.clear();
}

RenderWidgetHostInputEventRouter::HittestDelegate::HittestDelegate(
    const std::unordered_map<viz::SurfaceId, HittestData, viz::SurfaceIdHash>&
        hittest_data)
    : hittest_data_(hittest_data) {}

bool RenderWidgetHostInputEventRouter::HittestDelegate::RejectHitTarget(
    const viz::SurfaceDrawQuad* surface_quad,
    const gfx::Point& point_in_quad_space) {
  auto it = hittest_data_.find(surface_quad->surface_id);
  if (it != hittest_data_.end() && it->second.ignored_for_hittest)
    return true;
  return false;
}

bool RenderWidgetHostInputEventRouter::HittestDelegate::AcceptHitTarget(
    const viz::SurfaceDrawQuad* surface_quad,
    const gfx::Point& point_in_quad_space) {
  auto it = hittest_data_.find(surface_quad->surface_id);
  if (it != hittest_data_.end() && !it->second.ignored_for_hittest)
    return true;
  return false;
}

RenderWidgetHostInputEventRouter::RenderWidgetHostInputEventRouter()
    : last_mouse_move_target_(nullptr),
      last_mouse_move_root_view_(nullptr),
      active_touches_(0),
      in_touchscreen_gesture_pinch_(false),
      gesture_pinch_did_send_scroll_begin_(false) {}

RenderWidgetHostInputEventRouter::~RenderWidgetHostInputEventRouter() {
  // We may be destroyed before some of the owners in the map, so we must
  // remove ourself from their observer lists.
  ClearAllObserverRegistrations();
}

RenderWidgetHostViewBase* RenderWidgetHostInputEventRouter::FindEventTarget(
    RenderWidgetHostViewBase* root_view,
    const gfx::Point& point,
    gfx::Point* transformed_point) {
  // Short circuit if owner_map has only one RenderWidgetHostView, no need for
  // hit testing.
  if (owner_map_.size() <= 1) {
    *transformed_point = point;
    return root_view;
  }

  // The hittest delegate is used to reject hittesting quads based on extra
  // hittesting data send by the renderer.
  HittestDelegate delegate(hittest_data_);

  // The conversion of point to transform_point is done over the course of the
  // hit testing, and reflect transformations that would normally be applied in
  // the renderer process if the event was being routed between frames within a
  // single process with only one RenderWidgetHost.
  viz::FrameSinkId frame_sink_id =
      root_view->FrameSinkIdAtPoint(&delegate, point, transformed_point);
  const FrameSinkIdOwnerMap::iterator iter = owner_map_.find(frame_sink_id);
  // If the point hit a Surface whose namspace is no longer in the map, then
  // it likely means the RenderWidgetHostView has been destroyed but its
  // parent frame has not sent a new compositor frame since that happened.
  if (iter == owner_map_.end())
    return root_view;

  return iter->second;
}

void RenderWidgetHostInputEventRouter::RouteMouseEvent(
    RenderWidgetHostViewBase* root_view,
    blink::WebMouseEvent* event,
    const ui::LatencyInfo& latency) {
  RenderWidgetHostViewBase* target = nullptr;
  gfx::Point transformed_point;

  // When the mouse is locked, directly route the events to the widget that
  // holds the lock and return.
  if (root_view->IsMouseLocked()) {
    target = RenderWidgetHostImpl::From(root_view->GetRenderWidgetHost())
                 ->delegate()
                 ->GetMouseLockWidget()
                 ->GetView();
    if (!root_view->TransformPointToCoordSpaceForView(
            gfx::Point(event->PositionInWidget().x,
                       event->PositionInWidget().y),
            target, &transformed_point))
      return;

    event->SetPositionInWidget(transformed_point.x(), transformed_point.y());
    target->ProcessMouseEvent(*event, latency);
    return;
  }

  const int mouse_button_modifiers = blink::WebInputEvent::kLeftButtonDown |
                                     blink::WebInputEvent::kMiddleButtonDown |
                                     blink::WebInputEvent::kRightButtonDown |
                                     blink::WebInputEvent::kBackButtonDown |
                                     blink::WebInputEvent::kForwardButtonDown;
  if (mouse_capture_target_.target &&
      event->GetType() != blink::WebInputEvent::kMouseDown &&
      (event->GetType() == blink::WebInputEvent::kMouseUp ||
       event->GetModifiers() & mouse_button_modifiers)) {
    target = mouse_capture_target_.target;
    if (!root_view->TransformPointToCoordSpaceForView(
            gfx::Point(event->PositionInWidget().x,
                       event->PositionInWidget().y),
            target, &transformed_point))
      return;
    if (event->GetType() == blink::WebInputEvent::kMouseUp)
      mouse_capture_target_.target = nullptr;
  } else {
    target = FindEventTarget(
        root_view,
        gfx::Point(event->PositionInWidget().x, event->PositionInWidget().y),
        &transformed_point);
  }

  // RenderWidgetHostViewGuest does not properly handle direct routing of mouse
  // events, so they have to go by the double-hop forwarding path through
  // the embedding renderer and then BrowserPluginGuest.
  if (target && target->IsRenderWidgetHostViewGuest()) {
    ui::LatencyInfo latency_info;
    RenderWidgetHostViewBase* owner_view =
        static_cast<RenderWidgetHostViewGuest*>(target)
            ->GetOwnerRenderWidgetHostView();
    // In case there is nested RenderWidgetHostViewGuests (i.e., PDF inside
    // <webview>), we will need the owner view of the top-most guest for input
    // routing.
    while (owner_view->IsRenderWidgetHostViewGuest()) {
      owner_view = static_cast<RenderWidgetHostViewGuest*>(owner_view)
                       ->GetOwnerRenderWidgetHostView();
    }

    if (owner_view != root_view) {
      // This happens when the view is embedded inside a cross-process frame
      // (i.e., owner view is a RenderWidgetHostViewChildFrame).
      gfx::Point owner_point;
      if (!root_view->TransformPointToCoordSpaceForView(
              gfx::Point(event->PositionInWidget().x,
                         event->PositionInWidget().y),
              owner_view, &owner_point)) {
        return;
      }
      event->SetPositionInWidget(owner_point.x(), owner_point.y());
    }
    owner_view->ProcessMouseEvent(*event, latency_info);
    return;
  }

  if (event->GetType() == blink::WebInputEvent::kMouseDown)
    mouse_capture_target_.target = target;

  if (!target)
    return;

  // SendMouseEnterOrLeaveEvents is called with the original event
  // coordinates, which are transformed independently for each view that will
  // receive an event. Also, since the view under the mouse has changed,
  // notify the CursorManager that it might need to change the cursor.
  if ((event->GetType() == blink::WebInputEvent::kMouseLeave ||
       event->GetType() == blink::WebInputEvent::kMouseMove) &&
      target != last_mouse_move_target_) {
    SendMouseEnterOrLeaveEvents(event, target, root_view);
    if (root_view->GetCursorManager())
      root_view->GetCursorManager()->UpdateViewUnderCursor(target);
  }

  event->SetPositionInWidget(transformed_point.x(), transformed_point.y());
  target->ProcessMouseEvent(*event, latency);
}

void RenderWidgetHostInputEventRouter::RouteMouseWheelEvent(
    RenderWidgetHostViewBase* root_view,
    blink::WebMouseWheelEvent* event,
    const ui::LatencyInfo& latency) {
  RenderWidgetHostViewBase* target = nullptr;
  gfx::Point transformed_point;

  if (root_view->IsMouseLocked()) {
    target = RenderWidgetHostImpl::From(root_view->GetRenderWidgetHost())
                 ->delegate()
                 ->GetMouseLockWidget()
                 ->GetView();
    if (!root_view->TransformPointToCoordSpaceForView(
            gfx::Point(event->PositionInWidget().x,
                       event->PositionInWidget().y),
            target, &transformed_point)) {
      root_view->WheelEventAck(*event,
                               INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS);
      return;
    }
  } else if (root_view->wheel_scroll_latching_enabled()) {
    if (event->phase == blink::WebMouseWheelEvent::kPhaseBegan) {
      wheel_target_.target = FindEventTarget(
          root_view,
          gfx::Point(event->PositionInWidget().x, event->PositionInWidget().y),
          &transformed_point);
      wheel_target_.delta =
          transformed_point -
          gfx::Point(event->PositionInWidget().x, event->PositionInWidget().y);
      target = wheel_target_.target;
    } else {
      if (wheel_target_.target) {
        target = wheel_target_.target;
        transformed_point = gfx::Point(event->PositionInWidget().x,
                                       event->PositionInWidget().y) +
                            wheel_target_.delta;
      } else if ((event->phase == blink::WebMouseWheelEvent::kPhaseEnded ||
                  event->momentum_phase ==
                      blink::WebMouseWheelEvent::kPhaseEnded) &&
                 bubbling_gesture_scroll_target_.target) {
        // Send a GSE to the bubbling target and cancel scroll bubbling since
        // the wheel target view is destroyed and the wheel end event won't get
        // processed.
        blink::WebGestureEvent fake_scroll_update =
            DummyGestureScrollUpdate(event->TimeStampSeconds());
        fake_scroll_update.source_device = blink::kWebGestureDeviceTouchpad;
        SendGestureScrollEnd(bubbling_gesture_scroll_target_.target,
                             fake_scroll_update);
        bubbling_gesture_scroll_target_.target = nullptr;
        first_bubbling_scroll_target_.target = nullptr;
      }
    }

  } else {  // !root_view->IsMouseLocked() &&
            // !root_view->wheel_scroll_latching_enabled()
    target = FindEventTarget(
        root_view,
        gfx::Point(event->PositionInWidget().x, event->PositionInWidget().y),
        &transformed_point);
  }

  if (!target) {
    root_view->WheelEventAck(*event, INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS);
    return;
  }

  event->SetPositionInWidget(transformed_point.x(), transformed_point.y());
  target->ProcessMouseWheelEvent(*event, latency);

  DCHECK(root_view->wheel_scroll_latching_enabled() || !wheel_target_.target);
  if (event->phase == blink::WebMouseWheelEvent::kPhaseEnded ||
      event->momentum_phase == blink::WebMouseWheelEvent::kPhaseEnded) {
    wheel_target_.target = nullptr;
  }
}

void RenderWidgetHostInputEventRouter::RouteGestureEvent(
    RenderWidgetHostViewBase* root_view,
    blink::WebGestureEvent* event,
    const ui::LatencyInfo& latency) {
  if (event->IsTargetViewport()) {
    root_view->ProcessGestureEvent(*event, latency);
    return;
  }

  switch (event->source_device) {
    case blink::kWebGestureDeviceUninitialized:
    case blink::kWebGestureDeviceCount:
      NOTREACHED() << "Uninitialized device type is not allowed";
      break;
    case blink::kWebGestureDeviceSyntheticAutoscroll:
      NOTREACHED() << "Only target_viewport synthetic autoscrolls are "
                      "currently supported";
      break;
    case blink::kWebGestureDeviceTouchpad:
      RouteTouchpadGestureEvent(root_view, event, latency);
      break;
    case blink::kWebGestureDeviceTouchscreen:
      RouteTouchscreenGestureEvent(root_view, event, latency);
      break;
  };
}

namespace {

unsigned CountChangedTouchPoints(const blink::WebTouchEvent& event) {
  unsigned changed_count = 0;

  blink::WebTouchPoint::State required_state =
      blink::WebTouchPoint::kStateUndefined;
  switch (event.GetType()) {
    case blink::WebInputEvent::kTouchStart:
      required_state = blink::WebTouchPoint::kStatePressed;
      break;
    case blink::WebInputEvent::kTouchEnd:
      required_state = blink::WebTouchPoint::kStateReleased;
      break;
    case blink::WebInputEvent::kTouchCancel:
      required_state = blink::WebTouchPoint::kStateCancelled;
      break;
    default:
      // We'll only ever call this method for TouchStart, TouchEnd
      // and TounchCancel events, so mark the rest as not-reached.
      NOTREACHED();
    }
    for (unsigned i = 0; i < event.touches_length; ++i) {
      if (event.touches[i].state == required_state)
        ++changed_count;
  }

  DCHECK(event.GetType() == blink::WebInputEvent::kTouchCancel ||
         changed_count == 1);
  return changed_count;
}

}  // namespace

// Any time a touch start event is handled/consumed/default prevented it is
// removed from the gesture map, because it will never create a gesture.
void RenderWidgetHostInputEventRouter::OnHandledTouchStartOrFirstTouchMove(
    uint32_t unique_touch_event_id) {
  // unique_touch_event_id of 0 implies a gesture not created by a touch.
  DCHECK_NE(unique_touch_event_id, 0U);
  touchscreen_gesture_target_map_.erase(unique_touch_event_id);
}

void RenderWidgetHostInputEventRouter::RouteTouchEvent(
    RenderWidgetHostViewBase* root_view,
    blink::WebTouchEvent* event,
    const ui::LatencyInfo& latency) {
  switch (event->GetType()) {
    case blink::WebInputEvent::kTouchStart: {
      int current_active_touches = active_touches_;
      active_touches_ += CountChangedTouchPoints(*event);
      DCHECK(active_touches_);
      if (!current_active_touches) {
        // Since this is the first touch, it defines the target for the rest
        // of this sequence.
        DCHECK(!touch_target_.target);
        gfx::Point transformed_point;
        gfx::Point original_point(event->touches[0].PositionInWidget().x,
                                  event->touches[0].PositionInWidget().y);
        touch_target_.target =
            FindEventTarget(root_view, original_point, &transformed_point);

        // TODO(wjmaclean): Instead of just computing a delta, we should extract
        // the complete transform. We assume it doesn't change for the duration
        // of the touch sequence, though this could be wrong; a better approach
        // might be to always transform each point to the |touch_target_.target|
        // for the duration of the sequence.
        touch_target_.delta = transformed_point - original_point;
        DCHECK(touchscreen_gesture_target_map_.find(
                   event->unique_touch_event_id) ==
               touchscreen_gesture_target_map_.end());
        touchscreen_gesture_target_map_[event->unique_touch_event_id] =
            touch_target_;

        if (!touch_target_.target) {
          TouchEventWithLatencyInfo touch_with_latency(*event, latency);
          root_view->ProcessAckedTouchEvent(
              touch_with_latency, INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS);
          return;
        }

        if (touch_target_.target == bubbling_gesture_scroll_target_.target) {
          SendGestureScrollEnd(
              bubbling_gesture_scroll_target_.target,
              DummyGestureScrollUpdate(event->TimeStampSeconds()));
          CancelScrollBubbling(bubbling_gesture_scroll_target_.target);
        }
      }

      if (touch_target_.target) {
        TransformEventTouchPositions(event, touch_target_.delta);
        touch_target_.target->ProcessTouchEvent(*event, latency);
      }
      break;
    }
    case blink::WebInputEvent::kTouchMove:
      if (!touch_target_.target) {
        TouchEventWithLatencyInfo touch_with_latency(*event, latency);
        root_view->ProcessAckedTouchEvent(
            touch_with_latency, INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS);
        return;
      }

      TransformEventTouchPositions(event, touch_target_.delta);
      touch_target_.target->ProcessTouchEvent(*event, latency);
      break;
    case blink::WebInputEvent::kTouchEnd:
    case blink::WebInputEvent::kTouchCancel:
      // Test active_touches_ before decrementing, since its value can be
      // reset to 0 in OnRenderWidgetHostViewBaseDestroyed, and this can
      // happen between the TouchStart and a subsequent TouchMove/End/Cancel.
      if (active_touches_)
        active_touches_ -= CountChangedTouchPoints(*event);
      DCHECK_GE(active_touches_, 0);

      if (!touch_target_.target) {
        TouchEventWithLatencyInfo touch_with_latency(*event, latency);
        root_view->ProcessAckedTouchEvent(
            touch_with_latency, INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS);
        return;
      }

      TransformEventTouchPositions(event, touch_target_.delta);
      touch_target_.target->ProcessTouchEvent(*event, latency);
      if (!active_touches_)
        touch_target_.target = nullptr;
      break;
    default:
      NOTREACHED();
  }
}

void RenderWidgetHostInputEventRouter::SendMouseEnterOrLeaveEvents(
    blink::WebMouseEvent* event,
    RenderWidgetHostViewBase* target,
    RenderWidgetHostViewBase* root_view) {
  // This method treats RenderWidgetHostViews as a tree, where the mouse
  // cursor is potentially leaving one node and entering another somewhere
  // else in the tree. Since iframes are graphically self-contained (i.e. an
  // iframe can't have a descendant that renders outside of its rect
  // boundaries), all affected RenderWidgetHostViews are ancestors of either
  // the node being exited or the node being entered.
  // Approach:
  // 1. Find lowest common ancestor (LCA) of the last view and current target
  //    view.
  // 2. The last view, and its ancestors up to but not including the LCA,
  //    receive a MouseLeave.
  // 3. The LCA itself, unless it is the new target, receives a MouseOut
  //    because the cursor has passed between elements within its bounds.
  // 4. The new target view's ancestors, up to but not including the LCA,
  //    receive a MouseEnter.
  // Ordering does not matter since these are handled asynchronously relative
  // to each other.

  // If the mouse has moved onto a different root view (typically meaning it
  // has crossed over a popup or context menu boundary), then we invalidate
  // last_mouse_move_target_ because we have no reference for its coordinate
  // space.
  if (root_view != last_mouse_move_root_view_)
    last_mouse_move_target_ = nullptr;

  // Finding the LCA uses a standard approach. We build vectors of the
  // ancestors of each node up to the root, and then remove common ancestors.
  std::vector<RenderWidgetHostViewBase*> entered_views;
  std::vector<RenderWidgetHostViewBase*> exited_views;
  RenderWidgetHostViewBase* cur_view = target;
  entered_views.push_back(cur_view);
  while (cur_view->IsRenderWidgetHostViewChildFrame()) {
    cur_view =
        static_cast<RenderWidgetHostViewChildFrame*>(cur_view)->GetParentView();
    // cur_view can possibly be nullptr for guestviews that are not currently
    // connected to the webcontents tree.
    if (!cur_view) {
      last_mouse_move_target_ = target;
      last_mouse_move_root_view_ = root_view;
      return;
    }
    entered_views.push_back(cur_view);
  }
  // Non-root RWHVs are guaranteed to be RenderWidgetHostViewChildFrames,
  // as long as they are the only embeddable RWHVs.
  DCHECK_EQ(cur_view, root_view);

  cur_view = last_mouse_move_target_;
  if (cur_view) {
    exited_views.push_back(cur_view);
    while (cur_view->IsRenderWidgetHostViewChildFrame()) {
      cur_view = static_cast<RenderWidgetHostViewChildFrame*>(cur_view)
                     ->GetParentView();
      if (!cur_view) {
        last_mouse_move_target_ = target;
        last_mouse_move_root_view_ = root_view;
        return;
      }
      exited_views.push_back(cur_view);
    }
    DCHECK_EQ(cur_view, root_view);
  }

  // This removes common ancestors from the root downward.
  RenderWidgetHostViewBase* common_ancestor = nullptr;
  while (entered_views.size() > 0 && exited_views.size() > 0 &&
         entered_views.back() == exited_views.back()) {
    common_ancestor = entered_views.back();
    entered_views.pop_back();
    exited_views.pop_back();
  }

  gfx::Point transformed_point;
  // Send MouseLeaves.
  for (auto* view : exited_views) {
    blink::WebMouseEvent mouse_leave(*event);
    mouse_leave.SetType(blink::WebInputEvent::kMouseLeave);
    // There is a chance of a race if the last target has recently created a
    // new compositor surface. The SurfaceID for that might not have
    // propagated to its embedding surface, which makes it impossible to
    // compute the transformation for it
    if (!root_view->TransformPointToCoordSpaceForView(
            gfx::Point(event->PositionInWidget().x,
                       event->PositionInWidget().y),
            view, &transformed_point))
      transformed_point = gfx::Point();
    mouse_leave.SetPositionInWidget(transformed_point.x(),
                                    transformed_point.y());
    view->ProcessMouseEvent(mouse_leave, ui::LatencyInfo());
  }

  // The ancestor might need to trigger MouseOut handlers.
  if (common_ancestor && common_ancestor != target) {
    blink::WebMouseEvent mouse_move(*event);
    mouse_move.SetType(blink::WebInputEvent::kMouseMove);
    if (!root_view->TransformPointToCoordSpaceForView(
            gfx::Point(event->PositionInWidget().x,
                       event->PositionInWidget().y),
            common_ancestor, &transformed_point))
      transformed_point = gfx::Point();
    mouse_move.SetPositionInWidget(transformed_point.x(),
                                   transformed_point.y());
    common_ancestor->ProcessMouseEvent(mouse_move, ui::LatencyInfo());
  }

  // Send MouseMoves to trigger MouseEnter handlers.
  for (auto* view : entered_views) {
    if (view == target)
      continue;
    blink::WebMouseEvent mouse_enter(*event);
    mouse_enter.SetType(blink::WebInputEvent::kMouseMove);
    if (!root_view->TransformPointToCoordSpaceForView(
            gfx::Point(event->PositionInWidget().x,
                       event->PositionInWidget().y),
            view, &transformed_point))
      transformed_point = gfx::Point();
    mouse_enter.SetPositionInWidget(transformed_point.x(),
                                    transformed_point.y());
    view->ProcessMouseEvent(mouse_enter, ui::LatencyInfo());
  }

  last_mouse_move_target_ = target;
  last_mouse_move_root_view_ = root_view;
}

void RenderWidgetHostInputEventRouter::BubbleScrollEvent(
    RenderWidgetHostViewBase* target_view,
    const blink::WebGestureEvent& event) {
  DCHECK(target_view);
  DCHECK((target_view->wheel_scroll_latching_enabled() &&
          event.GetType() == blink::WebInputEvent::kGestureScrollBegin) ||
         event.GetType() == blink::WebInputEvent::kGestureScrollUpdate ||
         event.GetType() == blink::WebInputEvent::kGestureScrollEnd ||
         event.GetType() == blink::WebInputEvent::kGestureFlingStart);

  ui::LatencyInfo latency_info =
      ui::WebInputEventTraits::CreateLatencyInfoForWebGestureEvent(event);

  if (target_view->wheel_scroll_latching_enabled()) {
    if (event.GetType() == blink::WebInputEvent::kGestureScrollBegin) {
      // If target_view has unrelated gesture events in progress, do
      // not proceed. This could cause confusion between independent
      // scrolls.
      if (target_view == touchscreen_gesture_target_.target ||
          target_view == touchpad_gesture_target_.target ||
          target_view == touch_target_.target) {
        return;
      }

      // This accounts for bubbling through nested OOPIFs. A gesture scroll
      // begin has been bubbled but the target has sent back a gesture scroll
      // event ack which didn't consume any scroll delta, and so another level
      // of bubbling is needed. This requires a GestureScrollEnd be sent to the
      // last view, which will no longer be the scroll target.
      if (bubbling_gesture_scroll_target_.target)
        SendGestureScrollEnd(bubbling_gesture_scroll_target_.target, event);
      else
        first_bubbling_scroll_target_.target = target_view;

      bubbling_gesture_scroll_target_.target = target_view;
    } else {  // !(event.GetType() == blink::WebInputEvent::kGestureScrollBegin)
      if (!bubbling_gesture_scroll_target_.target) {
        // The GestureScrollBegin event is not bubbled, don't bubble the rest of
        // the scroll events.
        return;
      }

      // Don't bubble the GSE events that are generated and sent to intermediate
      // bubbling targets.
      if (event.GetType() == blink::WebInputEvent::kGestureScrollEnd &&
          target_view != first_bubbling_scroll_target_.target) {
        return;
      }
    }

    bubbling_gesture_scroll_target_.target->ProcessGestureEvent(event,
                                                                latency_info);
    if (event.GetType() == blink::WebInputEvent::kGestureScrollEnd ||
        event.GetType() == blink::WebInputEvent::kGestureFlingStart) {
      first_bubbling_scroll_target_.target = nullptr;
      bubbling_gesture_scroll_target_.target = nullptr;
    }

    return;
  }

  DCHECK(!target_view->wheel_scroll_latching_enabled());

  // DCHECK_XNOR the current and original bubble targets. Both should be set
  // if a bubbling gesture scroll is in progress.
  DCHECK(!first_bubbling_scroll_target_.target ==
         !bubbling_gesture_scroll_target_.target);

  // If target_view is already set up for bubbled scrolls, we forward
  // the event to the current scroll target without further consideration.
  if (target_view == first_bubbling_scroll_target_.target) {
    bubbling_gesture_scroll_target_.target->ProcessGestureEvent(event,
                                                                latency_info);
    if (event.GetType() == blink::WebInputEvent::kGestureScrollEnd ||
        event.GetType() == blink::WebInputEvent::kGestureFlingStart) {
      first_bubbling_scroll_target_.target = nullptr;
      bubbling_gesture_scroll_target_.target = nullptr;
    }
    return;
  }

  // Disregard GestureScrollEnd events going to non-current targets.
  // These should only happen on ACKs of synthesized GSE events that are
  // sent from SendGestureScrollEnd calls, and are not relevant here.
  if (event.GetType() == blink::WebInputEvent::kGestureScrollEnd)
    return;

  // This is a special case to catch races where multiple GestureScrollUpdates
  // have been sent to a renderer before the first one was ACKed, and the ACK
  // caused a bubble retarget. In this case they all get forwarded.
  if (target_view == bubbling_gesture_scroll_target_.target) {
    bubbling_gesture_scroll_target_.target->ProcessGestureEvent(event,
                                                                latency_info);
    return;
  }

  // If target_view has unrelated gesture events in progress, do
  // not proceed. This could cause confusion between independent
  // scrolls.
  if (target_view == touchscreen_gesture_target_.target ||
      target_view == touchpad_gesture_target_.target ||
      target_view == touch_target_.target)
    return;

  // This accounts for bubbling through nested OOPIFs. A gesture scroll has
  // been bubbled but the target has sent back a gesture scroll event ack with
  // unused scroll delta, and so another level of bubbling is needed. This
  // requires a GestureScrollEnd be sent to the last view, which will no
  // longer be the scroll target.
  if (bubbling_gesture_scroll_target_.target)
    SendGestureScrollEnd(bubbling_gesture_scroll_target_.target, event);
  else
    first_bubbling_scroll_target_.target = target_view;

  bubbling_gesture_scroll_target_.target = target_view;

  SendGestureScrollBegin(target_view, event);
  target_view->ProcessGestureEvent(event, latency_info);
}

void RenderWidgetHostInputEventRouter::SendGestureScrollBegin(
    RenderWidgetHostViewBase* view,
    const blink::WebGestureEvent& event) {
  blink::WebGestureEvent scroll_begin(event);
  scroll_begin.SetType(blink::WebInputEvent::kGestureScrollBegin);
  switch (event.GetType()) {
    case blink::WebInputEvent::kGestureScrollUpdate:
      scroll_begin.data.scroll_begin.delta_x_hint =
          event.data.scroll_update.delta_x;
      scroll_begin.data.scroll_begin.delta_y_hint =
          event.data.scroll_update.delta_y;
      scroll_begin.data.scroll_begin.delta_hint_units =
          event.data.scroll_update.delta_units;
      break;
    case blink::WebInputEvent::kGesturePinchBegin:
      scroll_begin.data.scroll_begin.delta_x_hint = 0;
      scroll_begin.data.scroll_begin.delta_y_hint = 0;
      scroll_begin.data.scroll_begin.delta_hint_units =
          blink::WebGestureEvent::kPrecisePixels;
      break;
    default:
      NOTREACHED();
  }
  view->ProcessGestureEvent(
      scroll_begin,
      ui::WebInputEventTraits::CreateLatencyInfoForWebGestureEvent(event));
}

void RenderWidgetHostInputEventRouter::SendGestureScrollEnd(
    RenderWidgetHostViewBase* view,
    const blink::WebGestureEvent& event) {
  blink::WebGestureEvent scroll_end(event);
  scroll_end.SetType(blink::WebInputEvent::kGestureScrollEnd);
  scroll_end.SetTimeStampSeconds(
      (base::TimeTicks::Now() - base::TimeTicks()).InSecondsF());
  switch (event.GetType()) {
    case blink::WebInputEvent::kGestureScrollBegin:
      DCHECK(view->wheel_scroll_latching_enabled());
      scroll_end.data.scroll_end.inertial_phase =
          event.data.scroll_begin.inertial_phase;
      scroll_end.data.scroll_end.delta_units =
          event.data.scroll_begin.delta_hint_units;
      break;
    case blink::WebInputEvent::kGestureScrollUpdate:
      scroll_end.data.scroll_end.inertial_phase =
          event.data.scroll_update.inertial_phase;
      scroll_end.data.scroll_end.delta_units =
          event.data.scroll_update.delta_units;
      break;
    case blink::WebInputEvent::kGesturePinchEnd:
      scroll_end.data.scroll_end.inertial_phase =
          blink::WebGestureEvent::kUnknownMomentumPhase;
      scroll_end.data.scroll_end.delta_units =
          blink::WebGestureEvent::kPrecisePixels;
      break;
    default:
      NOTREACHED();
  }
  view->ProcessGestureEvent(
      scroll_end,
      ui::WebInputEventTraits::CreateLatencyInfoForWebGestureEvent(event));
}

void RenderWidgetHostInputEventRouter::CancelScrollBubbling(
    RenderWidgetHostViewBase* target_view) {
  DCHECK(target_view);
  if (target_view == first_bubbling_scroll_target_.target) {
    first_bubbling_scroll_target_.target = nullptr;
    bubbling_gesture_scroll_target_.target = nullptr;
  }
}

void RenderWidgetHostInputEventRouter::AddFrameSinkIdOwner(
    const viz::FrameSinkId& id,
    RenderWidgetHostViewBase* owner) {
  DCHECK(owner_map_.find(id) == owner_map_.end());
  // We want to be notified if the owner is destroyed so we can remove it from
  // our map.
  owner->AddObserver(this);
  owner_map_.insert(std::make_pair(id, owner));
}

void RenderWidgetHostInputEventRouter::RemoveFrameSinkIdOwner(
    const viz::FrameSinkId& id) {
  auto it_to_remove = owner_map_.find(id);
  if (it_to_remove != owner_map_.end()) {
    // If we remove a view from the observer list, we need to be sure to do a
    // cleanup of the various targets and target maps, else we will end up with
    // stale values if the view destructs and isn't an observer anymore.
    // Note: the view the iterator points at will be deleted in the following
    // call, and shouldn't be used after this point.
    OnRenderWidgetHostViewBaseDestroyed(it_to_remove->second);
  }

  for (auto it = hittest_data_.begin(); it != hittest_data_.end();) {
    if (it->first.frame_sink_id() == id)
      it = hittest_data_.erase(it);
    else
      ++it;
  }
}

void RenderWidgetHostInputEventRouter::OnHittestData(
    const FrameHostMsg_HittestData_Params& params) {
  if (owner_map_.find(params.surface_id.frame_sink_id()) == owner_map_.end()) {
    return;
  }
  HittestData data;
  data.ignored_for_hittest = params.ignored_for_hittest;
  hittest_data_[params.surface_id] = data;
}

RenderWidgetHostImpl*
RenderWidgetHostInputEventRouter::GetRenderWidgetHostAtPoint(
    RenderWidgetHostViewBase* root_view,
    const gfx::Point& point,
    gfx::Point* transformed_point) {
  if (!root_view)
    return nullptr;
  return RenderWidgetHostImpl::From(
      FindEventTarget(root_view, point, transformed_point)
          ->GetRenderWidgetHost());
}

void RenderWidgetHostInputEventRouter::RouteTouchscreenGestureEvent(
    RenderWidgetHostViewBase* root_view,
    blink::WebGestureEvent* event,
    const ui::LatencyInfo& latency) {
  DCHECK_EQ(blink::kWebGestureDeviceTouchscreen, event->source_device);

  if (event->GetType() == blink::WebInputEvent::kGesturePinchBegin) {
    in_touchscreen_gesture_pinch_ = true;
    // If the root view wasn't already receiving the gesture stream, then we
    // need to wrap the diverted pinch events in a GestureScrollBegin/End.
    // TODO(wjmaclean,kenrb,tdresser): When scroll latching lands, we can
    // revisit how this code should work.
    // https://crbug.com/526463
    auto* rwhi =
        static_cast<RenderWidgetHostImpl*>(root_view->GetRenderWidgetHost());
    // If the root view is the current gesture target, then we explicitly don't
    // send a GestureScrollBegin, as by the time we see GesturePinchBegin there
    // should have been one.
    if (root_view != touchscreen_gesture_target_.target &&
        !rwhi->is_in_touchscreen_gesture_scroll()) {
      gesture_pinch_did_send_scroll_begin_ = true;
      SendGestureScrollBegin(root_view, *event);
    }
  }

  if (in_touchscreen_gesture_pinch_) {
    root_view->ProcessGestureEvent(*event, latency);
    if (event->GetType() == blink::WebInputEvent::kGesturePinchEnd) {
      in_touchscreen_gesture_pinch_ = false;
      // If the root view wasn't already receiving the gesture stream, then we
      // need to wrap the diverted pinch events in a GestureScrollBegin/End.
      auto* rwhi =
          static_cast<RenderWidgetHostImpl*>(root_view->GetRenderWidgetHost());
      if (root_view != touchscreen_gesture_target_.target &&
          gesture_pinch_did_send_scroll_begin_ &&
          rwhi->is_in_touchscreen_gesture_scroll()) {
        SendGestureScrollEnd(root_view, *event);
      }
      gesture_pinch_did_send_scroll_begin_ = false;
    }
    return;
  }

  auto gesture_target_it =
      touchscreen_gesture_target_map_.find(event->unique_touch_event_id);
  bool no_matching_id =
      gesture_target_it == touchscreen_gesture_target_map_.end();

  // We use GestureTapDown to detect the start of a gesture sequence since
  // there is no WebGestureEvent equivalent for ET_GESTURE_BEGIN. Note that
  // this means the GestureFlingCancel that always comes between
  // ET_GESTURE_BEGIN and GestureTapDown is sent to the previous target, in
  // case it is still in a fling.
  bool is_gesture_start =
      event->GetType() == blink::WebInputEvent::kGestureTapDown;

  // On Android it is possible for touchscreen gesture events to arrive that
  // are not associated with touch events, because non-synthetic events can be
  // created by ContentView.
  // These gesture events should always have a unique_touch_event_id of 0.
  bool no_gesture_target =
      event->unique_touch_event_id != 0 && no_matching_id && is_gesture_start;

  if (no_gesture_target) {
    UMA_HISTOGRAM_BOOLEAN("Event.FrameEventRouting.NoGestureTarget",
                          no_gesture_target);
    NOTREACHED() << "Gesture sequence start detected with no target available.";
    // It is still safe to continue; we will recalculate the target.
  }

  // If this is a non-touch gesture or there was an no_matching_id error on
  // gesture start, then the target must be recalculated.
  if (event->unique_touch_event_id == 0 ||
      (no_matching_id && is_gesture_start)) {
    gfx::Point transformed_point;
    gfx::Point original_point(event->x, event->y);
    touchscreen_gesture_target_.target =
        FindEventTarget(root_view, original_point, &transformed_point);
    touchscreen_gesture_target_.delta = transformed_point - original_point;
  } else if (is_gesture_start) {
    touchscreen_gesture_target_ = gesture_target_it->second;
    touchscreen_gesture_target_map_.erase(gesture_target_it);

    // Abort any scroll bubbling in progress to avoid double entry.
    if (touchscreen_gesture_target_.target &&
        touchscreen_gesture_target_.target ==
            bubbling_gesture_scroll_target_.target) {
      SendGestureScrollEnd(bubbling_gesture_scroll_target_.target,
                           DummyGestureScrollUpdate(event->TimeStampSeconds()));
      CancelScrollBubbling(bubbling_gesture_scroll_target_.target);
    }
  }

  if (!touchscreen_gesture_target_.target) {
    root_view->GestureEventAck(*event,
                               INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS);
    return;
  }

  // TODO(mohsen): Add tests to check event location.
  event->x += touchscreen_gesture_target_.delta.x();
  event->y += touchscreen_gesture_target_.delta.y();
  touchscreen_gesture_target_.target->ProcessGestureEvent(*event, latency);
}

void RenderWidgetHostInputEventRouter::RouteTouchpadGestureEvent(
    RenderWidgetHostViewBase* root_view,
    blink::WebGestureEvent* event,
    const ui::LatencyInfo& latency) {
  DCHECK_EQ(blink::kWebGestureDeviceTouchpad, event->source_device);

  if (event->GetType() == blink::WebInputEvent::kGesturePinchBegin ||
      event->GetType() == blink::WebInputEvent::kGestureFlingStart) {
    gfx::Point transformed_point;
    gfx::Point original_point(event->x, event->y);
    touchpad_gesture_target_.target =
        FindEventTarget(root_view, original_point, &transformed_point);
    // TODO(mohsen): Instead of just computing a delta, we should extract the
    // complete transform. We assume it doesn't change for the duration of the
    // touchpad gesture sequence, though this could be wrong; a better approach
    // might be to always transform each point to the
    // |touchpad_gesture_target_.target| for the duration of the sequence.
    touchpad_gesture_target_.delta = transformed_point - original_point;

    // Abort any scroll bubbling in progress to avoid double entry.
    if (touchpad_gesture_target_.target &&
        touchpad_gesture_target_.target ==
            bubbling_gesture_scroll_target_.target) {
      SendGestureScrollEnd(bubbling_gesture_scroll_target_.target,
                           DummyGestureScrollUpdate(event->TimeStampSeconds()));
      CancelScrollBubbling(bubbling_gesture_scroll_target_.target);
    }
  }

  if (!touchpad_gesture_target_.target) {
    root_view->GestureEventAck(*event,
                               INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS);
    return;
  }

  // TODO(mohsen): Add tests to check event location.
  event->x += touchpad_gesture_target_.delta.x();
  event->y += touchpad_gesture_target_.delta.y();
  touchpad_gesture_target_.target->ProcessGestureEvent(*event, latency);
}

std::vector<RenderWidgetHostView*>
RenderWidgetHostInputEventRouter::GetRenderWidgetHostViewsForTests() const {
  std::vector<RenderWidgetHostView*> hosts;
  for (auto entry : owner_map_)
    hosts.push_back(entry.second);

  return hosts;
}

}  // namespace content
