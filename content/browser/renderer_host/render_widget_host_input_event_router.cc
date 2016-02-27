// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_widget_host_input_event_router.h"

#include "cc/quads/surface_draw_quad.h"
#include "cc/surfaces/surface_id_allocator.h"
#include "cc/surfaces/surface_manager.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/common/frame_messages.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"

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

  if (view == current_touch_target_) {
    current_touch_target_ = nullptr;
    active_touches_ = 0;
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
    : current_touch_target_(nullptr), active_touches_(0) {}

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
  uint32_t surface_id_namespace =
      root_view->SurfaceIdNamespaceAtPoint(&delegate, point, transformed_point);
  const SurfaceIdNamespaceOwnerMap::iterator iter =
      owner_map_.find(surface_id_namespace);
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

  target->ProcessMouseEvent(*event);
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

  target->ProcessMouseWheelEvent(*event);
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
        DCHECK(!current_touch_target_);
        gfx::Point transformed_point;
        gfx::Point original_point(event->touches[0].position.x,
                                  event->touches[0].position.y);
        current_touch_target_ =
            FindEventTarget(root_view, original_point, &transformed_point);
        if (!current_touch_target_)
          return;
      }
      ++active_touches_;
      if (current_touch_target_)
        current_touch_target_->ProcessTouchEvent(*event, latency);
      break;
    }
    case blink::WebInputEvent::TouchMove:
      if (current_touch_target_)
        current_touch_target_->ProcessTouchEvent(*event, latency);
      break;
    case blink::WebInputEvent::TouchEnd:
    case blink::WebInputEvent::TouchCancel:
      if (!current_touch_target_)
        break;

      DCHECK(active_touches_);
      current_touch_target_->ProcessTouchEvent(*event, latency);
      --active_touches_;
      if (!active_touches_)
        current_touch_target_ = nullptr;
      break;
    default:
      NOTREACHED();
  }
}

void RenderWidgetHostInputEventRouter::AddSurfaceIdNamespaceOwner(
    uint32_t id,
    RenderWidgetHostViewBase* owner) {
  DCHECK(owner_map_.find(id) == owner_map_.end());
  // We want to be notified if the owner is destroyed so we can remove it from
  // our map.
  owner->AddObserver(this);
  owner_map_.insert(std::make_pair(id, owner));
}

void RenderWidgetHostInputEventRouter::RemoveSurfaceIdNamespaceOwner(
    uint32_t id) {
  auto it_to_remove = owner_map_.find(id);
  if (it_to_remove != owner_map_.end()) {
    it_to_remove->second->RemoveObserver(this);
    owner_map_.erase(it_to_remove);
  }

  for (auto it = hittest_data_.begin(); it != hittest_data_.end();) {
    if (cc::SurfaceIdAllocator::NamespaceForId(it->first) == id)
      it = hittest_data_.erase(it);
    else
      ++it;
  }
}

void RenderWidgetHostInputEventRouter::OnHittestData(
    const FrameHostMsg_HittestData_Params& params) {
  if (owner_map_.find(cc::SurfaceIdAllocator::NamespaceForId(
          params.surface_id)) == owner_map_.end()) {
    return;
  }
  HittestData data;
  data.ignored_for_hittest = params.ignored_for_hittest;
  hittest_data_[params.surface_id] = data;
}

}  // namespace content
