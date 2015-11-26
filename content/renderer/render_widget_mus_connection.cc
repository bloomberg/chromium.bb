// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/render_widget_mus_connection.h"

#include "components/mus/public/interfaces/compositor_frame.mojom.h"
#include "components/mus/public/interfaces/window_tree.mojom.h"
#include "content/public/common/mojo_shell_connection.h"
#include "mojo/application/public/cpp/application_impl.h"
#include "mojo/converters/geometry/geometry_type_converters.h"
#include "mojo/converters/surfaces/surfaces_utils.h"

namespace content {

RenderWidgetMusConnection::RenderWidgetMusConnection(
    int routing_id,
    mojo::InterfaceRequest<mus::mojom::WindowTreeClient> request)
    : routing_id_(routing_id), root_(nullptr) {
  // TODO(fsamuel): We should probably introduce a
  // RenderWidgetMusConnection::FromRoutingID that's usable from RenderWidget.
  // TODO(fsamuel): We probably want to pause processing of incoming
  // messages until we have an associated RenderWidget.
  mus::WindowTreeConnection::Create(
      this, request.Pass(),
      mus::WindowTreeConnection::CreateType::DONT_WAIT_FOR_EMBED);
}

RenderWidgetMusConnection::~RenderWidgetMusConnection() {}

void RenderWidgetMusConnection::SubmitCompositorFrame() {
  const gfx::Rect bounds(root_->bounds());
  mus::mojom::PassPtr pass = mojo::CreateDefaultPass(1, bounds);
  mus::mojom::CompositorFramePtr frame = mus::mojom::CompositorFrame::New();

  mus::mojom::CompositorFrameMetadataPtr meta =
      mus::mojom::CompositorFrameMetadata::New();
  meta->device_scale_factor = 1.0f;
  frame->metadata = meta.Pass();

  frame->resources.resize(0u);

  pass->quads.resize(0u);
  pass->shared_quad_states.push_back(mojo::CreateDefaultSQS(bounds.size()));

  mus::mojom::QuadPtr quad = mus::mojom::Quad::New();
  quad->material = mus::mojom::MATERIAL_SOLID_COLOR;
  quad->rect = mojo::Rect::From(bounds);
  quad->opaque_rect = mojo::Rect::New();
  quad->visible_rect = mojo::Rect::From(bounds);
  quad->needs_blending = false;
  quad->shared_quad_state_index = 0u;

  mus::mojom::SolidColorQuadStatePtr color_state =
      mus::mojom::SolidColorQuadState::New();
  color_state->color = mus::mojom::Color::New();
  color_state->color->rgba = 0xff00ff00;
  color_state->force_anti_aliasing_off = false;

  quad->solid_color_quad_state = color_state.Pass();
  pass->quads.push_back(quad.Pass());
  frame->passes.push_back(pass.Pass());
  surface_->SubmitCompositorFrame(frame.Pass(), mojo::Closure());
}

void RenderWidgetMusConnection::OnConnectionLost(
    mus::WindowTreeConnection* connection) {
  delete this;
}

void RenderWidgetMusConnection::OnEmbed(mus::Window* root) {
  root_ = root;
  root_->AddObserver(this);
  surface_ = root_->RequestSurface(mus::mojom::SURFACE_TYPE_DEFAULT);
  surface_->BindToThread();
  SubmitCompositorFrame();
}

void RenderWidgetMusConnection::OnUnembed() {}

void RenderWidgetMusConnection::OnWindowBoundsChanged(
    mus::Window* window,
    const gfx::Rect& old_bounds,
    const gfx::Rect& new_bounds) {
  SubmitCompositorFrame();
}

}  // namespace content
