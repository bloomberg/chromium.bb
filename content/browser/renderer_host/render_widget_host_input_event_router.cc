// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_widget_host_input_event_router.h"

#include "cc/surfaces/surface_manager.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"

namespace content {

RenderWidgetHostInputEventRouter::RenderWidgetHostInputEventRouter() {}

RenderWidgetHostInputEventRouter::~RenderWidgetHostInputEventRouter() {
  owner_map_.clear();
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

  // The conversion of point to transform_point is done over the course of the
  // hit testing, and reflect transformations that would normally be applied in
  // the renderer process if the event was being routed between frames within a
  // single process with only one RenderWidgetHost.
  uint32_t surface_id_namespace =
      root_view->SurfaceIdNamespaceAtPoint(point, transformed_point);
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
  event->x = transformed_point.x();
  event->y = transformed_point.y();

  target->ProcessMouseWheelEvent(*event);
}

void RenderWidgetHostInputEventRouter::AddSurfaceIdNamespaceOwner(
    uint32_t id,
    RenderWidgetHostViewBase* owner) {
  DCHECK(owner_map_.find(id) == owner_map_.end());
  owner_map_.insert(std::make_pair(id, owner));
}

void RenderWidgetHostInputEventRouter::RemoveSurfaceIdNamespaceOwner(
    uint32_t id) {
  owner_map_.erase(id);
}

}  // namespace content
