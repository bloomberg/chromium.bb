// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/io_surface_layer_impl.h"

#include "base/stringprintf.h"
#include "cc/layers/quad_sink.h"
#include "cc/output/gl_renderer.h"  // For the GLC() macro.
#include "cc/output/output_surface.h"
#include "cc/quads/io_surface_draw_quad.h"
#include "cc/trees/layer_tree_impl.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebGraphicsContext3D.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/khronos/GLES2/gl2ext.h"

namespace cc {

IOSurfaceLayerImpl::IOSurfaceLayerImpl(LayerTreeImpl* tree_impl, int id)
    : LayerImpl(tree_impl, id),
      io_surface_id_(0),
      io_surface_changed_(false),
      io_surface_texture_id_(0) {}

IOSurfaceLayerImpl::~IOSurfaceLayerImpl() {
  if (!io_surface_texture_id_)
    return;

  OutputSurface* output_surface = layer_tree_impl()->output_surface();
  // FIXME: Implement this path for software compositing.
  WebKit::WebGraphicsContext3D* context3d = output_surface->context3d();
  if (context3d)
    context3d->deleteTexture(io_surface_texture_id_);
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

void IOSurfaceLayerImpl::WillDraw(ResourceProvider* resource_provider) {
  LayerImpl::WillDraw(resource_provider);

  if (io_surface_changed_) {
    WebKit::WebGraphicsContext3D* context3d =
        resource_provider->GraphicsContext3D();
    if (!context3d) {
      // FIXME: Implement this path for software compositing.
      return;
    }

    // FIXME: Do this in a way that we can track memory usage.
    if (!io_surface_texture_id_)
      io_surface_texture_id_ = context3d->createTexture();

    GLC(context3d, context3d->activeTexture(GL_TEXTURE0));
    GLC(context3d,
        context3d->bindTexture(GL_TEXTURE_RECTANGLE_ARB,
                               io_surface_texture_id_));
    GLC(context3d,
        context3d->texParameteri(
            GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
    GLC(context3d,
        context3d->texParameteri(
            GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
    GLC(context3d,
        context3d->texParameteri(
            GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
    GLC(context3d,
        context3d->texParameteri(
            GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
    context3d->texImageIOSurface2DCHROMIUM(GL_TEXTURE_RECTANGLE_ARB,
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
}

void IOSurfaceLayerImpl::AppendQuads(QuadSink* quad_sink,
                                     AppendQuadsData* append_quads_data) {
  SharedQuadState* shared_quad_state =
      quad_sink->UseSharedQuadState(CreateSharedQuadState());
  AppendDebugBorderQuad(quad_sink, shared_quad_state, append_quads_data);

  gfx::Rect quad_rect(content_bounds());
  gfx::Rect opaque_rect(contents_opaque() ? quad_rect : gfx::Rect());
  scoped_ptr<IOSurfaceDrawQuad> quad = IOSurfaceDrawQuad::Create();
  quad->SetNew(shared_quad_state,
               quad_rect,
               opaque_rect,
               io_surface_size_,
               io_surface_texture_id_,
               IOSurfaceDrawQuad::FLIPPED);
  quad_sink->Append(quad.PassAs<DrawQuad>(), append_quads_data);
}

void IOSurfaceLayerImpl::DumpLayerProperties(std::string* str,
                                             int indent) const {
  str->append(IndentString(indent));
  base::StringAppendF(str,
                      "iosurface id: %u texture id: %u\n",
                      io_surface_id_,
                      io_surface_texture_id_);
  LayerImpl::DumpLayerProperties(str, indent);
}

void IOSurfaceLayerImpl::DidLoseOutputSurface() {
  // We don't have a valid texture ID in the new context; however,
  // the IOSurface is still valid.
  io_surface_texture_id_ = 0;
  io_surface_changed_ = true;
}

void IOSurfaceLayerImpl::SetIOSurfaceProperties(unsigned io_surface_id,
                                                gfx::Size size) {
  if (io_surface_id_ != io_surface_id)
    io_surface_changed_ = true;

  io_surface_id_ = io_surface_id;
  io_surface_size_ = size;
}

const char* IOSurfaceLayerImpl::LayerTypeAsString() const {
  return "IOSurfaceLayer";
}

}  // namespace cc
