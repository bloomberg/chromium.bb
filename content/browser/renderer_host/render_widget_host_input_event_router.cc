// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_widget_host_input_event_router.h"

#include "cc/quads/surface_draw_quad.h"
#include "cc/surfaces/surface_id_allocator.h"
#include "cc/surfaces/surface_manager.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/common/frame_messages.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"

namespace {

void TransformEventTouchPositions(blink::WebTouchEvent* event,
                                  const gfx::Vector2d& delta) {
  for (unsigned i = 0; i < event->touchesLength; ++i) {
     event->touches[i].position.x += delta.x();
     event->touches[i].position.y += delta.y();
  }
}

}  // anonymous namespace

namespace content {

void RenderWidgetHostInputEventRouter::OnRenderWidgetHostViewBaseDestroyed(
    RenderWidgetHostViewBase* view) {
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

  // If the target that's being destroyed is in the gesture target queue, we
  // replace it with nullptr so that we maintain the 1:1 correspondence between
  // queue entries and the touch sequences that underly them.
  for (size_t i = 0; i < touchscreen_gesture_target_queue_.size(); ++i) {
    if (touchscreen_gesture_target_queue_[i].target == view)
      touchscreen_gesture_target_queue_[i].target = nullptr;
  }

  if (view == touchscreen_gesture_target_.target)
    touchscreen_gesture_target_.target = nullptr;

  if (view == touchpad_gesture_target_.target)
    touchpad_gesture_target_.target = nullptr;

  if (view == bubbling_gesture_scroll_target_.target ||
      view == first_bubbling_scroll_target_.target) {
    bubbling_gesture_scroll_target_.target = nullptr;
    first_bubbling_scroll_target_.target = nullptr;
  }
}

void RenderWidgetHostInputEventRouter::ClearAllObserverRegistrations() {
  for (auto entry : owner_map_)
    entry.second->RemoveObserver(this);
  owner_map_.clear();
}

RenderWidgetHostInputEventRouter::HittestDelegate::HittestDelegate(
    const std::unordered_map<cc::SurfaceId, HittestData, cc::SurfaceIdHash>&
        hittest_data)
    : hittest_data_(hittest_data) {}

bool RenderWidgetHostInputEventRouter::HittestDelegate::RejectHitTarget(
    const cc::SurfaceDrawQuad* surface_quad,
    const gfx::Point& point_in_quad_space) {
  auto it = hittest_data_.find(surface_quad->surface_id);
  if (it != hittest_data_.end() && it->second.ignored_for_hittest)
    return true;
  return false;
}

bool RenderWidgetHostInputEventRouter::HittestDelegate::AcceptHitTarget(
    const cc::SurfaceDrawQuad* surface_quad,
    const gfx::Point& point_in_quad_space) {
  auto it = hittest_data_.find(surface_quad->surface_id);
  if (it != hittest_data_.end() && !it->second.ignored_for_hittest)
    return true;
  return false;
}

RenderWidgetHostInputEventRouter::RenderWidgetHostInputEventRouter()
    : active_touches_(0),
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
  uint32_t surface_client_id =
      root_view->SurfaceClientIdAtPoint(&delegate, point, transformed_point);
  const SurfaceClientIdOwnerMap::iterator iter =
      owner_map_.find(surface_client_id);
  // If the point hit a Surface whose namspace is no longer in the map, then
  // it likely means the RenderWidgetHostView has been destroyed but its
  // parent frame has not sent a new compositor frame since that happened.
  if (iter == owner_map_.end())
    return root_view;

  return iter->second;
}

void RenderWidgetHostInputEventRouter::RouteMouseEvent(
    RenderWidgetHostViewBase* root_view,
    blink::WebMouseEvent* event) {
  gfx::Point transformed_point;
  RenderWidgetHostViewBase* target = FindEventTarget(
      root_view, gfx::Point(event->x, event->y), &transformed_point);
  if (!target)
    return;

  event->x = transformed_point.x();
  event->y = transformed_point.y();
  // TODO(wjmaclean): Initialize latency info correctly for OOPIFs.
  // https://crbug.com/613628
  ui::LatencyInfo latency_info;
  target->ProcessMouseEvent(*event, latency_info);
}

void RenderWidgetHostInputEventRouter::RouteMouseWheelEvent(
    RenderWidgetHostViewBase* root_view,
    blink::WebMouseWheelEvent* event) {
  gfx::Point transformed_point;
  RenderWidgetHostViewBase* target = FindEventTarget(
      root_view, gfx::Point(event->x, event->y), &transformed_point);
  if (!target)
    return;

  event->x = transformed_point.x();
  event->y = transformed_point.y();
  // TODO(wjmaclean): Initialize latency info correctly for OOPIFs.
  // https://crbug.com/613628
  ui::LatencyInfo latency_info;
  target->ProcessMouseWheelEvent(*event, latency_info);
}

void RenderWidgetHostInputEventRouter::RouteGestureEvent(
    RenderWidgetHostViewBase* root_view,
    blink::WebGestureEvent* event,
    const ui::LatencyInfo& latency) {
  switch (event->sourceDevice) {
    case blink::WebGestureDeviceUninitialized:
      NOTREACHED() << "Uninitialized device type is not allowed";
      break;
    case blink::WebGestureDeviceTouchpad:
      RouteTouchpadGestureEvent(root_view, event, latency);
      break;
    case blink::WebGestureDeviceTouchscreen:
      RouteTouchscreenGestureEvent(root_view, event, latency);
      break;
  };
}

void RenderWidgetHostInputEventRouter::RouteTouchEvent(
    RenderWidgetHostViewBase* root_view,
    blink::WebTouchEvent* event,
    const ui::LatencyInfo& latency) {
  switch (event->type) {
    case blink::WebInputEvent::TouchStart: {
      if (!active_touches_) {
        // Since this is the first touch, it defines the target for the rest
        // of this sequence.
        DCHECK(!touch_target_.target);
        gfx::Point transformed_point;
        gfx::Point original_point(event->touches[0].position.x,
                                  event->touches[0].position.y);
        touch_target_.target =
            FindEventTarget(root_view, original_point, &transformed_point);

        // TODO(wjmaclean): Instead of just computing a delta, we should extract
        // the complete transform. We assume it doesn't change for the duration
        // of the touch sequence, though this could be wrong; a better approach
        // might be to always transform each point to the |touch_target_.target|
        // for the duration of the sequence.
        touch_target_.delta = transformed_point - original_point;
        touchscreen_gesture_target_queue_.push_back(touch_target_);

        if (!touch_target_.target) {
          return;
        } else if (touch_target_.target ==
                   bubbling_gesture_scroll_target_.target) {
          SendGestureScrollEnd(bubbling_gesture_scroll_target_.target,
                               blink::WebGestureEvent());
          CancelScrollBubbling(bubbling_gesture_scroll_target_.target);
        }
      }

      ++active_touches_;
      if (touch_target_.target) {
        TransformEventTouchPositions(event, touch_target_.delta);
        touch_target_.target->ProcessTouchEvent(*event, latency);
      }
      break;
    }
    case blink::WebInputEvent::TouchMove:
      if (touch_target_.target) {
        TransformEventTouchPositions(event, touch_target_.delta);
        touch_target_.target->ProcessTouchEvent(*event, latency);
      }
      break;
    case blink::WebInputEvent::TouchEnd:
    case blink::WebInputEvent::TouchCancel:
      if (!touch_target_.target)
        break;

      DCHECK(active_touches_);
      TransformEventTouchPositions(event, touch_target_.delta);
      touch_target_.target->ProcessTouchEvent(*event, latency);
      --active_touches_;
      if (!active_touches_)
        touch_target_.target = nullptr;
      break;
    default:
      NOTREACHED();
  }
}

void RenderWidgetHostInputEventRouter::BubbleScrollEvent(
    RenderWidgetHostViewBase* target_view,
    const blink::WebGestureEvent& event) {
  // TODO(kenrb, tdresser): This needs to be refactored when scroll latching
  // is implemented (see https://crbug.com/526463). This design has some
  // race problems that can result in lost scroll delta, which are very
  // difficult to resolve until this is changed to do all scroll targeting,
  // including bubbling, based on GestureScrollBegin.
  DCHECK(target_view);
  DCHECK(event.type == blink::WebInputEvent::GestureScrollUpdate ||
         event.type == blink::WebInputEvent::GestureScrollEnd);
  // DCHECK_XNOR the current and original bubble targets. Both should be set
  // if a bubbling gesture scroll is in progress.
  DCHECK(!first_bubbling_scroll_target_.target ==
         !bubbling_gesture_scroll_target_.target);

  // If target_view is already set up for bubbled scrolls, we forward
  // the event to the current scroll target without further consideration.
  if (target_view == first_bubbling_scroll_target_.target) {
    bubbling_gesture_scroll_target_.target->ProcessGestureEvent(
        event, ui::LatencyInfo());
    if (event.type == blink::WebInputEvent::GestureScrollEnd) {
      first_bubbling_scroll_target_.target = nullptr;
      bubbling_gesture_scroll_target_.target = nullptr;
    }
    return;
  }

  // Disregard GestureScrollEnd events going to non-current targets.
  // These should only happen on ACKs of synthesized GSE events that are
  // sent from SendGestureScrollEnd calls, and are not relevant here.
  if (event.type == blink::WebInputEvent::GestureScrollEnd)
    return;

  // This is a special case to catch races where multiple GestureScrollUpdates
  // have been sent to a renderer before the first one was ACKed, and the ACK
  // caused a bubble retarget. In this case they all get forwarded.
  if (target_view == bubbling_gesture_scroll_target_.target) {
    bubbling_gesture_scroll_target_.target->ProcessGestureEvent(
        event, ui::LatencyInfo());
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
  target_view->ProcessGestureEvent(event, ui::LatencyInfo());
}

void RenderWidgetHostInputEventRouter::SendGestureScrollBegin(
    RenderWidgetHostViewBase* view,
    const blink::WebGestureEvent& event) {
  DCHECK(event.type == blink::WebInputEvent::GestureScrollUpdate ||
         event.type == blink::WebInputEvent::GesturePinchBegin);
  blink::WebGestureEvent scroll_begin(event);
  scroll_begin.type = blink::WebInputEvent::GestureScrollBegin;
  scroll_begin.data.scrollBegin.deltaXHint = event.data.scrollUpdate.deltaX;
  scroll_begin.data.scrollBegin.deltaYHint = event.data.scrollUpdate.deltaY;
  scroll_begin.data.scrollBegin.deltaHintUnits =
      event.data.scrollUpdate.deltaUnits;
  view->ProcessGestureEvent(scroll_begin, ui::LatencyInfo());
}

void RenderWidgetHostInputEventRouter::SendGestureScrollEnd(
    RenderWidgetHostViewBase* view,
    const blink::WebGestureEvent& event) {
  DCHECK(event.type == blink::WebInputEvent::GestureScrollUpdate ||
         event.type == blink::WebInputEvent::GesturePinchEnd);
  blink::WebGestureEvent scroll_end(event);
  scroll_end.type = blink::WebInputEvent::GestureScrollEnd;
  scroll_end.timeStampSeconds =
      (base::TimeTicks::Now() - base::TimeTicks()).InSecondsF();
  scroll_end.data.scrollEnd.inertialPhase =
      event.data.scrollUpdate.inertialPhase;
  scroll_end.data.scrollEnd.deltaUnits = event.data.scrollUpdate.deltaUnits;
  view->ProcessGestureEvent(scroll_end, ui::LatencyInfo());
}

void RenderWidgetHostInputEventRouter::CancelScrollBubbling(
    RenderWidgetHostViewBase* target_view) {
  DCHECK(target_view);
  if (target_view == first_bubbling_scroll_target_.target) {
    first_bubbling_scroll_target_.target = nullptr;
    bubbling_gesture_scroll_target_.target = nullptr;
  }
}

void RenderWidgetHostInputEventRouter::AddSurfaceClientIdOwner(
    uint32_t id,
    RenderWidgetHostViewBase* owner) {
  DCHECK(owner_map_.find(id) == owner_map_.end());
  // We want to be notified if the owner is destroyed so we can remove it from
  // our map.
  owner->AddObserver(this);
  owner_map_.insert(std::make_pair(id, owner));
}

void RenderWidgetHostInputEventRouter::RemoveSurfaceClientIdOwner(uint32_t id) {
  auto it_to_remove = owner_map_.find(id);
  if (it_to_remove != owner_map_.end()) {
    it_to_remove->second->RemoveObserver(this);
    owner_map_.erase(it_to_remove);
  }

  for (auto it = hittest_data_.begin(); it != hittest_data_.end();) {
    if (it->first.client_id() == id)
      it = hittest_data_.erase(it);
    else
      ++it;
  }
}

void RenderWidgetHostInputEventRouter::OnHittestData(
    const FrameHostMsg_HittestData_Params& params) {
  if (owner_map_.find(params.surface_id.client_id()) == owner_map_.end()) {
    return;
  }
  HittestData data;
  data.ignored_for_hittest = params.ignored_for_hittest;
  hittest_data_[params.surface_id] = data;
}

void RenderWidgetHostInputEventRouter::RouteTouchscreenGestureEvent(
    RenderWidgetHostViewBase* root_view,
    blink::WebGestureEvent* event,
    const ui::LatencyInfo& latency) {
  DCHECK_EQ(blink::WebGestureDeviceTouchscreen, event->sourceDevice);

  if (event->type == blink::WebInputEvent::GesturePinchBegin) {
    in_touchscreen_gesture_pinch_ = true;
    // If the root view wasn't already receiving the gesture stream, then we
    // need to wrap the diverted pinch events in a GestureScrollBegin/End.
    // TODO(wjmaclean,kenrb,tdresser): When scroll latching lands, we can
    // revisit how this code should work.
    // https://crbug.com/526463
    auto rwhi =
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
    if (event->type == blink::WebInputEvent::GesturePinchEnd) {
      in_touchscreen_gesture_pinch_ = false;
      // If the root view wasn't already receiving the gesture stream, then we
      // need to wrap the diverted pinch events in a GestureScrollBegin/End.
      auto rwhi =
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

  // We use GestureTapDown to detect the start of a gesture sequence since there
  // is no WebGestureEvent equivalent for ET_GESTURE_BEGIN. Note that this
  // means the GestureFlingCancel that always comes between ET_GESTURE_BEGIN and
  // GestureTapDown is sent to the previous target, in case it is still in a
  // fling.
  if (event->type == blink::WebInputEvent::GestureTapDown) {
    if (touchscreen_gesture_target_queue_.empty()) {
      LOG(ERROR) << "Gesture sequence start detected with no target available.";
      // Ignore this gesture sequence as no target is available.
      // TODO(wjmaclean): this only happens on Windows, and should not happen.
      // https://crbug.com/595422
      touchscreen_gesture_target_.target = nullptr;
      return;
    }

    touchscreen_gesture_target_ = touchscreen_gesture_target_queue_.front();
    touchscreen_gesture_target_queue_.pop_front();

    // Abort any scroll bubbling in progress to avoid double entry.
    if (touchscreen_gesture_target_.target &&
        touchscreen_gesture_target_.target ==
            bubbling_gesture_scroll_target_.target) {
      SendGestureScrollEnd(bubbling_gesture_scroll_target_.target,
                           blink::WebGestureEvent());
      CancelScrollBubbling(bubbling_gesture_scroll_target_.target);
    }
  }

  if (!touchscreen_gesture_target_.target)
    return;

  // TODO(mohsen): Add tests to check event location.
  event->x += touchscreen_gesture_target_.delta.x();
  event->y += touchscreen_gesture_target_.delta.y();
  touchscreen_gesture_target_.target->ProcessGestureEvent(*event, latency);
}

void RenderWidgetHostInputEventRouter::RouteTouchpadGestureEvent(
    RenderWidgetHostViewBase* root_view,
    blink::WebGestureEvent* event,
    const ui::LatencyInfo& latency) {
  DCHECK_EQ(blink::WebGestureDeviceTouchpad, event->sourceDevice);

  if (event->type == blink::WebInputEvent::GesturePinchBegin ||
      event->type == blink::WebInputEvent::GestureFlingStart) {
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
                           blink::WebGestureEvent());
      CancelScrollBubbling(bubbling_gesture_scroll_target_.target);
    }
  }

  if (!touchpad_gesture_target_.target)
    return;

  // TODO(mohsen): Add tests to check event location.
  event->x += touchpad_gesture_target_.delta.x();
  event->y += touchpad_gesture_target_.delta.y();
  touchpad_gesture_target_.target->ProcessGestureEvent(*event, latency);
}

}  // namespace content
