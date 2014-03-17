// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/io_surface_layer_impl.h"

#include "base/strings/stringprintf.h"
#include "cc/layers/quad_sink.h"
#include "cc/output/gl_renderer.h"  // For the GLC() macro.
#include "cc/output/output_surface.h"
#include "cc/quads/io_surface_draw_quad.h"
#include "cc/trees/layer_tree_impl.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/khronos/GLES2/gl2ext.h"

namespace cc {

IOSurfaceLayerImpl::IOSurfaceLayerImpl(LayerTreeImpl* tree_impl, int id)
    : LayerImpl(tree_impl, id),
      io_surface_id_(0),
      io_surface_changed_(false),
      io_surface_texture_id_(0),
      io_surface_resource_id_(0) {}

IOSurfaceLayerImpl::~IOSurfaceLayerImpl() {
  if (!io_surface_texture_id_)
    return;

  DestroyTexture();
}

void IOSurfaceLayerImpl::DestroyTexture() {
  if (io_surface_resource_id_) {
    ResourceProvider* resource_provider =
        layer_tree_impl()->resource_provider();
    resource_provider->DeleteResource(io_surface_resource_id_);
    io_surface_resource_id_ = 0;
  }

  if (io_surface_texture_id_) {
    ContextProvider* context_provider =
        layer_tree_impl()->output_surface()->context_provider().get();
    // TODO(skaslev): Implement this path for software compositing.
    if (context_provider)
      context_provider->ContextGL()->DeleteTextures(1, &io_surface_texture_id_);
    io_surface_texture_id_ = 0;
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
  if (draw_mode == DRAW_MODE_RESOURCELESS_SOFTWARE)
    return false;

  if (io_surface_changed_) {
    ContextProvider* context_provider =
        layer_tree_impl()->output_surface()->context_provider().get();
    if (!context_provider) {
      // TODO(skaslev): Implement this path for software compositing.
      return false;
    }

    gpu::gles2::GLES2Interface* gl = context_provider->ContextGL();

    // TODO(ernstm): Do this in a way that we can track memory usage.
    if (!io_surface_texture_id_) {
      gl->GenTextures(1, &io_surface_texture_id_);
      io_surface_resource_id_ =
          resource_provider->CreateResourceFromExternalTexture(
              GL_TEXTURE_RECTANGLE_ARB,
              io_surface_texture_id_);
    }

    GLC(gl, gl->BindTexture(GL_TEXTURE_RECTANGLE_ARB, io_surface_texture_id_));
    gl->TexImageIOSurface2DCHROMIUM(GL_TEXTURE_RECTANGLE_ARB,
                                    io_surface_size_.width(),
                                    io_surface_size_.height(),
                                    io_surface_id_,
                                    0);
    // Do not check for error conditions. texImageIOSurface2DCHROMIUM() is
    // supposed to hold on to the last good IOSurface if the new one is already
    // closed. This is only a possibility during live resizing of plugins.
    // However, it seems that this is not sufficient to completely guard against
    // garbage being drawn. If this is found to be a significant issue, it may
    // be necessary to explicitly tell the embedder when to free the surfaces it
    // has allocated.
    io_surface_changed_ = false;
  }

  return LayerImpl::WillDraw(draw_mode, resource_provider);
}

void IOSurfaceLayerImpl::AppendQuads(QuadSink* quad_sink,
                                     AppendQuadsData* append_quads_data) {
  SharedQuadState* shared_quad_state =
      quad_sink->UseSharedQuadState(CreateSharedQuadState());
  AppendDebugBorderQuad(quad_sink, shared_quad_state, append_quads_data);

  gfx::Rect quad_rect(content_bounds());
  gfx::Rect opaque_rect(contents_opaque() ? quad_rect : gfx::Rect());
  gfx::Rect visible_quad_rect = quad_sink->UnoccludedContentRect(
      quad_rect, draw_properties().target_space_transform);
  if (visible_quad_rect.IsEmpty())
    return;

  scoped_ptr<IOSurfaceDrawQuad> quad = IOSurfaceDrawQuad::Create();
  quad->SetNew(shared_quad_state,
               quad_rect,
               opaque_rect,
               visible_quad_rect,
               io_surface_size_,
               io_surface_resource_id_,
               IOSurfaceDrawQuad::FLIPPED);
  quad_sink->Append(quad.PassAs<DrawQuad>());
}

void IOSurfaceLayerImpl::ReleaseResources() {
  // We don't have a valid texture ID in the new context; however,
  // the IOSurface is still valid.
  DestroyTexture();
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
