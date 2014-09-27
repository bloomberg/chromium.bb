// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/io_surface_layer_impl.h"

#include "base/strings/stringprintf.h"
#include "cc/output/gl_renderer.h"  // For the GLC() macro.
#include "cc/output/output_surface.h"
#include "cc/quads/io_surface_draw_quad.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/occlusion_tracker.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/khronos/GLES2/gl2ext.h"

namespace cc {

IOSurfaceLayerImpl::IOSurfaceLayerImpl(LayerTreeImpl* tree_impl, int id)
    : LayerImpl(tree_impl, id),
      io_surface_id_(0),
      io_surface_changed_(false),
      io_surface_resource_id_(0) {}

IOSurfaceLayerImpl::~IOSurfaceLayerImpl() {
  DestroyResource();
}

void IOSurfaceLayerImpl::DestroyResource() {
  if (io_surface_resource_id_) {
    ResourceProvider* resource_provider =
        layer_tree_impl()->resource_provider();
    resource_provider->DeleteResource(io_surface_resource_id_);
    io_surface_resource_id_ = 0;
  }
}

scoped_ptr<LayerImpl> IOSurfaceLayerImpl::CreateLayerImpl(
    LayerTreeImpl* tree_impl) {
  return IOSurfaceLayerImpl::Create(tree_impl, id()).PassAs<LayerImpl>();
}

void IOSurfaceLayerImpl::PushPropertiesTo(LayerImpl* layer) {
  LayerImpl::PushPropertiesTo(layer);

  IOSurfaceLayerImpl* io_surface_layer =
      static_cast<IOSurfaceLayerImpl*>(layer);
  io_surface_layer->SetIOSurfaceProperties(io_surface_id_, io_surface_size_);
}

bool IOSurfaceLayerImpl::WillDraw(DrawMode draw_mode,
                                  ResourceProvider* resource_provider) {
  if (draw_mode != DRAW_MODE_HARDWARE)
    return false;

  if (io_surface_changed_) {
    DestroyResource();
    io_surface_resource_id_ = resource_provider->CreateResourceFromIOSurface(
        io_surface_size_, io_surface_id_);
    io_surface_changed_ = false;
  }

  return LayerImpl::WillDraw(draw_mode, resource_provider);
}

void IOSurfaceLayerImpl::AppendQuads(
    RenderPass* render_pass,
    const OcclusionTracker<LayerImpl>& occlusion_tracker,
    AppendQuadsData* append_quads_data) {
  SharedQuadState* shared_quad_state =
      render_pass->CreateAndAppendSharedQuadState();
  PopulateSharedQuadState(shared_quad_state);

  AppendDebugBorderQuad(
      render_pass, content_bounds(), shared_quad_state, append_quads_data);

  gfx::Rect quad_rect(content_bounds());
  gfx::Rect opaque_rect(contents_opaque() ? quad_rect : gfx::Rect());
  gfx::Rect visible_quad_rect =
      occlusion_tracker.GetCurrentOcclusionForLayer(
                            draw_properties().target_space_transform)
          .GetUnoccludedContentRect(quad_rect);
  if (visible_quad_rect.IsEmpty())
    return;

  IOSurfaceDrawQuad* quad =
      render_pass->CreateAndAppendDrawQuad<IOSurfaceDrawQuad>();
  quad->SetNew(shared_quad_state,
               quad_rect,
               opaque_rect,
               visible_quad_rect,
               io_surface_size_,
               io_surface_resource_id_,
               IOSurfaceDrawQuad::FLIPPED);
}

void IOSurfaceLayerImpl::ReleaseResources() {
  // We don't have a valid resource ID in the new context; however,
  // the IOSurface is still valid.
  DestroyResource();
  io_surface_changed_ = true;
}

void IOSurfaceLayerImpl::SetIOSurfaceProperties(unsigned io_surface_id,
                                                const gfx::Size& size) {
  if (io_surface_id_ != io_surface_id)
    io_surface_changed_ = true;

  io_surface_id_ = io_surface_id;
  io_surface_size_ = size;
}

const char* IOSurfaceLayerImpl::LayerTypeAsString() const {
  return "cc::IOSurfaceLayerImpl";
}

}  // namespace cc
