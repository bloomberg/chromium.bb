// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/texture_layer_impl.h"

#include "base/strings/stringprintf.h"
#include "cc/layers/quad_sink.h"
#include "cc/output/renderer.h"
#include "cc/quads/texture_draw_quad.h"
#include "cc/resources/platform_color.h"
#include "cc/resources/scoped_resource.h"
#include "cc/trees/layer_tree_impl.h"

namespace cc {

TextureLayerImpl::TextureLayerImpl(LayerTreeImpl* tree_impl,
                                   int id,
                                   bool uses_mailbox)
    : LayerImpl(tree_impl, id),
      texture_id_(0),
      external_texture_resource_(0),
      premultiplied_alpha_(true),
      blend_background_color_(false),
      flipped_(true),
      uv_top_left_(0.f, 0.f),
      uv_bottom_right_(1.f, 1.f),
      uses_mailbox_(uses_mailbox),
      own_mailbox_(false),
      valid_texture_copy_(false) {
  vertex_opacity_[0] = 1.0f;
  vertex_opacity_[1] = 1.0f;
  vertex_opacity_[2] = 1.0f;
  vertex_opacity_[3] = 1.0f;
}

TextureLayerImpl::~TextureLayerImpl() { FreeTextureMailbox(); }

void TextureLayerImpl::SetTextureMailbox(const TextureMailbox& mailbox) {
  DCHECK(uses_mailbox_);
  FreeTextureMailbox();
  texture_mailbox_ = mailbox;
  own_mailbox_ = true;
  valid_texture_copy_ = false;
}

scoped_ptr<LayerImpl> TextureLayerImpl::CreateLayerImpl(
    LayerTreeImpl* tree_impl) {
  return TextureLayerImpl::Create(tree_impl, id(), uses_mailbox_).
      PassAs<LayerImpl>();
}

void TextureLayerImpl::PushPropertiesTo(LayerImpl* layer) {
  LayerImpl::PushPropertiesTo(layer);

  TextureLayerImpl* texture_layer = static_cast<TextureLayerImpl*>(layer);
  texture_layer->set_flipped(flipped_);
  texture_layer->set_uv_top_left(uv_top_left_);
  texture_layer->set_uv_bottom_right(uv_bottom_right_);
  texture_layer->set_vertex_opacity(vertex_opacity_);
  texture_layer->set_premultiplied_alpha(premultiplied_alpha_);
  if (uses_mailbox_ && own_mailbox_) {
    texture_layer->SetTextureMailbox(texture_mailbox_);
    own_mailbox_ = false;
  } else {
    texture_layer->set_texture_id(texture_id_);
  }
}

bool TextureLayerImpl::WillDraw(DrawMode draw_mode,
                                ResourceProvider* resource_provider) {
  if (draw_mode == DRAW_MODE_RESOURCELESS_SOFTWARE)
    return false;

  if (uses_mailbox_) {
    if (own_mailbox_) {
      DCHECK(!external_texture_resource_);
      if ((draw_mode == DRAW_MODE_HARDWARE && texture_mailbox_.IsTexture()) ||
          (draw_mode == DRAW_MODE_SOFTWARE &&
           texture_mailbox_.IsSharedMemory())) {
        external_texture_resource_ =
            resource_provider->CreateResourceFromTextureMailbox(
                texture_mailbox_);
        DCHECK(external_texture_resource_);
        texture_copy_.reset();
        valid_texture_copy_ = false;
      }
      if (external_texture_resource_)
        own_mailbox_ = false;
    }

    if (!valid_texture_copy_ && draw_mode == DRAW_MODE_HARDWARE &&
        texture_mailbox_.IsSharedMemory()) {
      DCHECK(!external_texture_resource_);
      // Have to upload a copy to a texture for it to be used in a
      // hardware draw.
      if (!texture_copy_)
        texture_copy_ = ScopedResource::create(resource_provider);
      if (texture_copy_->size() != texture_mailbox_.shared_memory_size() ||
          resource_provider->InUseByConsumer(texture_copy_->id()))
        texture_copy_->Free();

      if (!texture_copy_->id()) {
        texture_copy_->Allocate(texture_mailbox_.shared_memory_size(),
                                resource_provider->best_texture_format(),
                                ResourceProvider::TextureUsageAny);
      }

      if (texture_copy_->id()) {
        std::vector<uint8> swizzled;
        uint8* pixels =
            static_cast<uint8*>(texture_mailbox_.shared_memory()->memory());

        if (!PlatformColor::SameComponentOrder(texture_copy_->format())) {
          // Swizzle colors. This is slow, but should be really uncommon.
          swizzled.resize(texture_mailbox_.shared_memory_size_in_bytes());
          for (size_t i = 0; i < texture_mailbox_.shared_memory_size_in_bytes();
               i += 4) {
            swizzled[i] = pixels[i + 2];
            swizzled[i + 1] = pixels[i + 1];
            swizzled[i + 2] = pixels[i];
            swizzled[i + 3] = pixels[i + 3];
          }
          pixels = &swizzled[0];
        }

        resource_provider->SetPixels(
            texture_copy_->id(),
            pixels,
            gfx::Rect(texture_mailbox_.shared_memory_size()),
            gfx::Rect(texture_mailbox_.shared_memory_size()),
            gfx::Vector2d());

        valid_texture_copy_ = true;
      }
    }
  } else if (texture_id_) {
    DCHECK(!external_texture_resource_);
    if (draw_mode == DRAW_MODE_HARDWARE) {
      external_texture_resource_ =
          resource_provider->CreateResourceFromExternalTexture(
              GL_TEXTURE_2D,
              texture_id_);
    }
  }
  return (external_texture_resource_ || valid_texture_copy_) &&
         LayerImpl::WillDraw(draw_mode, resource_provider);
}

void TextureLayerImpl::AppendQuads(QuadSink* quad_sink,
                                   AppendQuadsData* append_quads_data) {
  DCHECK(external_texture_resource_ || valid_texture_copy_);

  SharedQuadState* shared_quad_state =
      quad_sink->UseSharedQuadState(CreateSharedQuadState());
  AppendDebugBorderQuad(quad_sink, shared_quad_state, append_quads_data);

  SkColor bg_color = blend_background_color_ ?
      background_color() : SK_ColorTRANSPARENT;
  bool opaque = contents_opaque() || (SkColorGetA(bg_color) == 0xFF);

  gfx::Rect quad_rect(content_bounds());
  gfx::Rect opaque_rect = opaque ? quad_rect : gfx::Rect();
  scoped_ptr<TextureDrawQuad> quad = TextureDrawQuad::Create();
  ResourceProvider::ResourceId id =
      valid_texture_copy_ ? texture_copy_->id() : external_texture_resource_;
  quad->SetNew(shared_quad_state,
               quad_rect,
               opaque_rect,
               id,
               premultiplied_alpha_,
               uv_top_left_,
               uv_bottom_right_,
               bg_color,
               vertex_opacity_,
               flipped_);

  // Perform explicit clipping on a quad to avoid setting a scissor later.
  if (shared_quad_state->is_clipped && quad->PerformClipping())
    shared_quad_state->is_clipped = false;
  if (!quad->rect.IsEmpty())
    quad_sink->Append(quad.PassAs<DrawQuad>(), append_quads_data);
}

void TextureLayerImpl::DidDraw(ResourceProvider* resource_provider) {
  LayerImpl::DidDraw(resource_provider);
  if (uses_mailbox_ || !external_texture_resource_)
    return;
  // TODO(danakj): the following assert will not be true when sending resources
  // to a parent compositor. A synchronization scheme (double-buffering or
  // pipelining of updates) for the client will need to exist to solve this.
  DCHECK(!resource_provider->InUseByConsumer(external_texture_resource_));
  resource_provider->DeleteResource(external_texture_resource_);
  external_texture_resource_ = 0;
}

Region TextureLayerImpl::VisibleContentOpaqueRegion() const {
  if (contents_opaque())
    return visible_content_rect();

  if (blend_background_color_ && (SkColorGetA(background_color()) == 0xFF))
    return visible_content_rect();

  return Region();
}

void TextureLayerImpl::DidLoseOutputSurface() {
  if (external_texture_resource_ && !uses_mailbox_) {
    ResourceProvider* resource_provider =
        layer_tree_impl()->resource_provider();
    resource_provider->DeleteResource(external_texture_resource_);
  }
  texture_copy_.reset();
  texture_id_ = 0;
  external_texture_resource_ = 0;
  valid_texture_copy_ = false;
}

const char* TextureLayerImpl::LayerTypeAsString() const {
  return "cc::TextureLayerImpl";
}

bool TextureLayerImpl::CanClipSelf() const {
  return true;
}

void TextureLayerImpl::FreeTextureMailbox() {
  if (!uses_mailbox_)
    return;
  if (own_mailbox_) {
    DCHECK(!external_texture_resource_);
    texture_mailbox_.RunReleaseCallback(texture_mailbox_.sync_point(), false);
  } else if (external_texture_resource_) {
    DCHECK(!own_mailbox_);
    ResourceProvider* resource_provider =
        layer_tree_impl()->resource_provider();
    resource_provider->DeleteResource(external_texture_resource_);
    external_texture_resource_ = 0;
  }
}

}  // namespace cc
