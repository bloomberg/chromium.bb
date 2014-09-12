// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/native_viewport/viewport_surface.h"

#include "base/bind.h"
#include "cc/surfaces/surface_id_allocator.h"
#include "mojo/services/public/cpp/geometry/geometry_type_converters.h"
#include "mojo/services/public/cpp/surfaces/surfaces_type_converters.h"
#include "mojo/services/public/cpp/surfaces/surfaces_utils.h"
#include "ui/gfx/transform.h"

namespace mojo {

ViewportSurface::ViewportSurface(SurfacesService* surfaces_service,
                                 Gpu* gpu_service,
                                 const gfx::Size& size,
                                 cc::SurfaceId child_id)
    : gpu_service_(gpu_service),
      widget_id_(0u),
      size_(size),
      child_id_(child_id),
      weak_factory_(this) {
  surfaces_service->CreateSurfaceConnection(
      base::Bind(&ViewportSurface::OnSurfaceConnectionCreated,
                 weak_factory_.GetWeakPtr()));
}

ViewportSurface::~ViewportSurface() {
}

void ViewportSurface::SetWidgetId(uint64_t widget_id) {
  widget_id_ = widget_id;
  if (id_allocator_)
    BindSurfaceToNativeViewport();
}

void ViewportSurface::SetSize(const gfx::Size& size) {
  if (size_ == size)
    return;

  if (id_.is_null())
    return;

  surface_->DestroySurface(SurfaceId::From(id_));
  if (widget_id_)
    BindSurfaceToNativeViewport();
}

void ViewportSurface::SetChildId(cc::SurfaceId child_id) {
  child_id_ = child_id;
  SubmitFrame();
}

void ViewportSurface::OnSurfaceConnectionCreated(SurfacePtr surface,
                                                 uint32_t id_namespace) {
  surface_ = surface.Pass();
  surface_.set_client(this);
  id_allocator_.reset(new cc::SurfaceIdAllocator(id_namespace));
  if (widget_id_ != 0u)
    BindSurfaceToNativeViewport();
}

void ViewportSurface::BindSurfaceToNativeViewport() {
  CommandBufferPtr cb;
  gpu_service_->CreateOnscreenGLES2Context(
      widget_id_, Size::From(size_), Get(&cb));

  id_ = id_allocator_->GenerateId();
  surface_->CreateGLES2BoundSurface(
      cb.Pass(), SurfaceId::From(id_), Size::From(size_));

  SubmitFrame();
}

void ViewportSurface::SubmitFrame() {
  if (child_id_.is_null() || id_.is_null())
    return;

  SurfaceQuadStatePtr surface_quad_state = SurfaceQuadState::New();
  surface_quad_state->surface = SurfaceId::From(child_id_);

  gfx::Rect bounds(size_);

  QuadPtr surface_quad = Quad::New();
  surface_quad->material = Material::MATERIAL_SURFACE_CONTENT;
  surface_quad->rect = Rect::From(bounds);
  surface_quad->opaque_rect = Rect::From(bounds);
  surface_quad->visible_rect = Rect::From(bounds);
  surface_quad->needs_blending = true;
  surface_quad->shared_quad_state_index = 0;
  surface_quad->surface_quad_state = surface_quad_state.Pass();

  PassPtr pass = CreateDefaultPass(1, bounds);

  pass->quads.push_back(surface_quad.Pass());
  pass->shared_quad_states.push_back(CreateDefaultSQS(size_));

  FramePtr frame = Frame::New();
  frame->passes.push_back(pass.Pass());
  frame->resources.resize(0u);
  surface_->SubmitFrame(SurfaceId::From(id_), frame.Pass());
}

void ViewportSurface::ReturnResources(Array<ReturnedResourcePtr> resources) {
  // We never submit resources so we should never get any back.
  DCHECK_EQ(0u, resources.size());
}

}  // namespace mojo
