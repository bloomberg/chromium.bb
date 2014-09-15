// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/view_manager/display_manager.h"

#include "base/numerics/safe_conversions.h"
#include "cc/surfaces/surface_id_allocator.h"
#include "mojo/public/cpp/application/application_connection.h"
#include "mojo/services/public/cpp/geometry/geometry_type_converters.h"
#include "mojo/services/public/cpp/surfaces/surfaces_type_converters.h"
#include "mojo/services/public/cpp/surfaces/surfaces_utils.h"
#include "mojo/services/public/interfaces/gpu/gpu.mojom.h"
#include "mojo/services/public/interfaces/surfaces/quads.mojom.h"
#include "mojo/services/view_manager/connection_manager.h"

namespace mojo {
namespace service {
namespace {

gfx::Rect ConvertRectToRoot(const ServerView* view, const gfx::Rect& bounds) {
  gfx::Point origin(bounds.origin());
  while (view->parent()) {
    origin += view->bounds().OffsetFromOrigin();
    view = view->parent();
    if (!view->visible())
      return gfx::Rect();
  }
  return gfx::Rect(origin, bounds.size());
}

void DrawViewTree(Pass* pass, const ServerView* view, gfx::Vector2d offset) {
  if (!view->visible())
    return;

  gfx::Rect node_bounds = view->bounds() + offset;
  std::vector<const ServerView*> children(view->GetChildren());
  for (std::vector<const ServerView*>::reverse_iterator it = children.rbegin();
       it != children.rend();
       ++it) {
    DrawViewTree(pass, *it, offset + view->bounds().OffsetFromOrigin());
  }

  cc::SurfaceId node_id = view->surface_id();

  SurfaceQuadStatePtr surface_quad_state = SurfaceQuadState::New();
  surface_quad_state->surface = SurfaceId::From(node_id);

  gfx::Transform node_transform;
  node_transform.Translate(node_bounds.x(), node_bounds.y());

  QuadPtr surface_quad = Quad::New();
  surface_quad->material = Material::MATERIAL_SURFACE_CONTENT;
  surface_quad->rect = Rect::From(node_bounds);
  surface_quad->opaque_rect = Rect::From(node_bounds);
  surface_quad->visible_rect = Rect::From(node_bounds);
  surface_quad->needs_blending = true;
  surface_quad->shared_quad_state_index =
      base::saturated_cast<int32_t>(pass->shared_quad_states.size());
  surface_quad->surface_quad_state = surface_quad_state.Pass();

  SharedQuadStatePtr sqs = CreateDefaultSQS(node_bounds.size());
  sqs->content_to_target_transform = Transform::From(node_transform);

  pass->quads.push_back(surface_quad.Pass());
  pass->shared_quad_states.push_back(sqs.Pass());
}

}  // namespace

DisplayManager::DisplayManager(
    ApplicationConnection* app_connection,
    ConnectionManager* connection_manager,
    const Callback<void()>& native_viewport_closed_callback)
    : connection_manager_(connection_manager),
      in_setup_(false),
      bounds_(800, 600),
      draw_timer_(false, false),
      weak_factory_(this) {
  app_connection->ConnectToService("mojo:mojo_native_viewport_service",
                                   &native_viewport_);
  native_viewport_.set_client(this);
  native_viewport_->Create(Size::From(bounds_));
  native_viewport_->Show();
  app_connection->ConnectToService("mojo:mojo_surfaces_service",
                                   &surfaces_service_);
  surfaces_service_->CreateSurfaceConnection(base::Bind(
      &DisplayManager::OnSurfaceConnectionCreated, weak_factory_.GetWeakPtr()));
}

DisplayManager::~DisplayManager() {
}

void DisplayManager::SchedulePaint(const ServerView* view,
                                   const gfx::Rect& bounds) {
  if (!view->visible())
    return;
  gfx::Rect root_relative_rect = ConvertRectToRoot(view, bounds);
  if (root_relative_rect.IsEmpty())
    return;
  dirty_rect_.Union(root_relative_rect);
  if (!draw_timer_.IsRunning()) {
    draw_timer_.Start(
        FROM_HERE,
        base::TimeDelta(),
        base::Bind(&DisplayManager::Draw, base::Unretained(this)));
  }
}

void DisplayManager::OnSurfaceConnectionCreated(SurfacePtr surface,
                                                uint32_t id_namespace) {
  surface_ = surface.Pass();
  surface_.set_client(this);
  surface_id_allocator_.reset(new cc::SurfaceIdAllocator(id_namespace));
  Draw();
}

void DisplayManager::Draw() {
  if (!surface_)
    return;
  if (surface_id_.is_null()) {
    surface_id_ = surface_id_allocator_->GenerateId();
    surface_->CreateSurface(SurfaceId::From(surface_id_), Size::From(bounds_));
  }

  PassPtr pass = CreateDefaultPass(1, gfx::Rect(bounds_));
  pass->damage_rect = Rect::From(dirty_rect_);

  DrawViewTree(pass.get(), connection_manager_->root(), gfx::Vector2d());

  FramePtr frame = Frame::New();
  frame->passes.push_back(pass.Pass());
  frame->resources.resize(0u);
  surface_->SubmitFrame(SurfaceId::From(surface_id_), frame.Pass());

  native_viewport_->SubmittedFrame(SurfaceId::From(surface_id_));

  dirty_rect_ = gfx::Rect();
}

void DisplayManager::OnCreated(uint64_t native_viewport_id) {
}

void DisplayManager::OnDestroyed() {
  native_viewport_closed_callback_.Run();
}

void DisplayManager::OnBoundsChanged(SizePtr bounds) {
  bounds_ = bounds.To<gfx::Size>();
  connection_manager_->root()->SetBounds(gfx::Rect(bounds_));
  if (surface_id_.is_null())
    return;
  surface_->DestroySurface(SurfaceId::From(surface_id_));
  surface_id_ = cc::SurfaceId();
  SchedulePaint(connection_manager_->root(), gfx::Rect(bounds_));
}

void DisplayManager::OnEvent(EventPtr event,
                             const mojo::Callback<void()>& callback) {
  connection_manager_->DispatchViewInputEventToWindowManager(event.Pass());
  callback.Run();
}

void DisplayManager::ReturnResources(Array<ReturnedResourcePtr> resources) {
  DCHECK_EQ(0u, resources.size());
}

}  // namespace service
}  // namespace mojo
