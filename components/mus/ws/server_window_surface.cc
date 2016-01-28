// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/ws/server_window_surface.h"

#include "cc/output/compositor_frame.h"
#include "cc/quads/shared_quad_state.h"
#include "cc/quads/surface_draw_quad.h"
#include "components/mus/surfaces/surfaces_state.h"
#include "components/mus/ws/server_window.h"
#include "components/mus/ws/server_window_delegate.h"
#include "components/mus/ws/server_window_surface_manager.h"
#include "mojo/converters/geometry/geometry_type_converters.h"
#include "mojo/converters/surfaces/surfaces_type_converters.h"

namespace mus {
namespace ws {
namespace {

void CallCallback(const mojo::Closure& callback, cc::SurfaceDrawStatus status) {
  callback.Run();
}

}  // namespace

ServerWindowSurface::ServerWindowSurface(
    ServerWindowSurfaceManager* manager,
    mojom::SurfaceType surface_type,
    mojo::InterfaceRequest<Surface> request,
    mojom::SurfaceClientPtr client)
    : manager_(manager),
      surface_type_(surface_type),
      surface_id_(manager->GenerateId()),
      surface_factory_(manager_->window()
                           ->delegate()
                           ->GetSurfacesState()
                           ->manager(),
                       this),
      client_(std::move(client)),
      binding_(this, std::move(request)) {
  surface_factory_.Create(surface_id_);
}

ServerWindowSurface::~ServerWindowSurface() {
  // SurfaceFactory's destructor will attempt to return resources which will
  // call back into here and access |client_| so we should destroy
  // |surface_factory_|'s resources early on.
  surface_factory_.DestroyAll();
}

void ServerWindowSurface::SubmitCompositorFrame(
    mojom::CompositorFramePtr frame,
    const SubmitCompositorFrameCallback& callback) {
  gfx::Size frame_size = frame->passes[0]->output_rect.To<gfx::Rect>().size();
  if (!surface_id_.is_null()) {
    // If the size of the CompostiorFrame has changed then destroy the existing
    // Surface and create a new one of the appropriate size.
    if (frame_size != last_submitted_frame_size_) {
      // Rendering of the topmost frame happens in two phases. First the frame
      // is generated and submitted, and a later date it is actually drawn.
      // During the time the frame is generated and drawn we can't destroy the
      // surface, otherwise when drawn you get an empty surface. To deal with
      // this we schedule destruction via the delegate. The delegate will call
      // us back when we're not waiting on a frame to be drawn (which may be
      // synchronously).
      surfaces_scheduled_for_destruction_.insert(surface_id_);
      window()->delegate()->ScheduleSurfaceDestruction(window());
      surface_id_ = manager_->GenerateId();
      surface_factory_.Create(surface_id_);
    }
  }
  surface_factory_.SubmitCompositorFrame(surface_id_,
                                         ConvertCompositorFrame(frame),
                                         base::Bind(&CallCallback, callback));
  window()->delegate()->OnScheduleWindowPaint(window());
  last_submitted_frame_size_ = frame_size;
}

void ServerWindowSurface::DestroySurfacesScheduledForDestruction() {
  std::set<cc::SurfaceId> surfaces;
  surfaces.swap(surfaces_scheduled_for_destruction_);
  for (auto& id : surfaces)
    surface_factory_.Destroy(id);
}

ServerWindow* ServerWindowSurface::window() {
  return manager_->window();
}

scoped_ptr<cc::CompositorFrame> ServerWindowSurface::ConvertCompositorFrame(
    const mojom::CompositorFramePtr& input) {
  referenced_window_ids_.clear();
  return ConvertToCompositorFrame(input, this);
}

bool ServerWindowSurface::ConvertSurfaceDrawQuad(
    const mojom::QuadPtr& input,
    const mojom::CompositorFrameMetadataPtr& metadata,
    cc::SharedQuadState* sqs,
    cc::RenderPass* render_pass) {
  Id id = static_cast<Id>(
      input->surface_quad_state->surface.To<cc::SurfaceId>().id);
  // TODO(sky): this is now wrong, needs mapping from client to window.
  WindowId other_window_id = WindowIdFromTransportId(id);
  ServerWindow* other_window = window()->GetChildWindow(other_window_id);
  if (!other_window) {
    DVLOG(2) << "The window ID '" << id << "' does not exist.";
    // TODO(fsamuel): We return true here so that the CompositorFrame isn't
    // entirely rejected. We just drop this SurfaceDrawQuad. This failure
    // can happen if the client has an out of date view of the window tree.
    // It would be nice if we can avoid reaching this state in the future.
    return true;
  }

  referenced_window_ids_.insert(other_window_id);

  ServerWindowSurface* default_surface =
      other_window->GetOrCreateSurfaceManager()->GetDefaultSurface();
  ServerWindowSurface* underlay_surface =
      other_window->GetOrCreateSurfaceManager()->GetUnderlaySurface();

  if (!default_surface && !underlay_surface)
    return true;

  cc::SurfaceDrawQuad* surface_quad =
      render_pass->CreateAndAppendDrawQuad<cc::SurfaceDrawQuad>();
  if (default_surface) {
    surface_quad->SetAll(sqs, input->rect.To<gfx::Rect>(),
                         input->opaque_rect.To<gfx::Rect>(),
                         input->visible_rect.To<gfx::Rect>(),
                         input->needs_blending, default_surface->id());
  }
  if (underlay_surface) {
    cc::SurfaceDrawQuad* underlay_quad =
        render_pass->CreateAndAppendDrawQuad<cc::SurfaceDrawQuad>();
    underlay_quad->SetAll(sqs, input->rect.To<gfx::Rect>(),
                          input->opaque_rect.To<gfx::Rect>(),
                          input->visible_rect.To<gfx::Rect>(),
                          input->needs_blending, underlay_surface->id());
  }
  return true;
}

void ServerWindowSurface::ReturnResources(
    const cc::ReturnedResourceArray& resources) {
  if (!client_)
    return;
  client_->ReturnResources(
      mojo::Array<mojom::ReturnedResourcePtr>::From(resources));
}

void ServerWindowSurface::SetBeginFrameSource(
    cc::SurfaceId surface_id,
    cc::BeginFrameSource* begin_frame_source) {
  // TODO(tansell): Implement this.
}

}  // namespace ws
}  // namespace mus
