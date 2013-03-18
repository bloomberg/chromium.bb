// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/texture_layer_impl.h"

#include "base/stringprintf.h"
#include "cc/layers/quad_sink.h"
#include "cc/output/renderer.h"
#include "cc/quads/texture_draw_quad.h"
#include "cc/trees/layer_tree_impl.h"

namespace cc {

TextureLayerImpl::TextureLayerImpl(LayerTreeImpl* tree_impl,
                                   int id,
                                   bool uses_mailbox)
    : LayerImpl(tree_impl, id),
      texture_id_(0),
      external_texture_resource_(0),
      premultiplied_alpha_(true),
      flipped_(true),
      uv_top_left_(0.f, 0.f),
      uv_bottom_right_(1.f, 1.f),
      uses_mailbox_(uses_mailbox),
      own_mailbox_(false) {
  vertex_opacity_[0] = 1.0f;
  vertex_opacity_[1] = 1.0f;
  vertex_opacity_[2] = 1.0f;
  vertex_opacity_[3] = 1.0f;
}

TextureLayerImpl::~TextureLayerImpl() { FreeTextureMailbox(); }

void TextureLayerImpl::SetTextureMailbox(const TextureMailbox& mailbox) {
  DCHECK(uses_mailbox_);
  DCHECK(mailbox.IsEmpty() || !mailbox.Equals(texture_mailbox_));
  FreeTextureMailbox();
  texture_mailbox_ = mailbox;
  own_mailbox_ = true;
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

void TextureLayerImpl::WillDraw(ResourceProvider* resource_provider) {
  if (uses_mailbox_ || !texture_id_)
    return;
  DCHECK(!external_texture_resource_);
  external_texture_resource_ =
      resource_provider->CreateResourceFromExternalTexture(texture_id_);
}

void TextureLayerImpl::AppendQuads(QuadSink* quad_sink,
                                   AppendQuadsData* append_quads_data) {
  if (!external_texture_resource_)
    return;

  SharedQuadState* shared_quad_state =
      quad_sink->UseSharedQuadState(CreateSharedQuadState());
  AppendDebugBorderQuad(quad_sink, shared_quad_state, append_quads_data);

  gfx::Rect quad_rect(content_bounds());
  gfx::Rect opaque_rect(contents_opaque() ? quad_rect : gfx::Rect());
  scoped_ptr<TextureDrawQuad> quad = TextureDrawQuad::Create();
  quad->SetNew(shared_quad_state,
               quad_rect,
               opaque_rect,
               external_texture_resource_,
               premultiplied_alpha_,
               uv_top_left_,
               uv_bottom_right_,
               vertex_opacity_,
               flipped_);

  // Perform explicit clipping on a quad to avoid setting a scissor later.
  if (shared_quad_state->is_clipped && quad->PerformClipping())
    shared_quad_state->is_clipped = false;
  if (!quad->rect.IsEmpty())
    quad_sink->Append(quad.PassAs<DrawQuad>(), append_quads_data);
}

void TextureLayerImpl::DidDraw(ResourceProvider* resource_provider) {
  if (uses_mailbox_ || !external_texture_resource_)
    return;
  // FIXME: the following assert will not be true when sending resources to a
  // parent compositor. A synchronization scheme (double-buffering or
  // pipelining of updates) for the client will need to exist to solve this.
  DCHECK(!resource_provider->InUseByConsumer(external_texture_resource_));
  resource_provider->DeleteResource(external_texture_resource_);
  external_texture_resource_ = 0;
}

void TextureLayerImpl::DumpLayerProperties(std::string* str, int indent) const {
  str->append(IndentString(indent));
  base::StringAppendF(str,
                      "texture layer texture id: %u premultiplied: %d\n",
                      texture_id_,
                      premultiplied_alpha_);
  LayerImpl::DumpLayerProperties(str, indent);
}

void TextureLayerImpl::DidLoseOutputSurface() {
  texture_id_ = 0;
  external_texture_resource_ = 0;
}

const char* TextureLayerImpl::LayerTypeAsString() const {
  return "TextureLayer";
}

bool TextureLayerImpl::CanClipSelf() const {
  return true;
}

void TextureLayerImpl::DidBecomeActive() {
  if (!own_mailbox_)
    return;
  DCHECK(!external_texture_resource_);
  ResourceProvider* resource_provider = layer_tree_impl()->resource_provider();
  if (!texture_mailbox_.IsEmpty()) {
    external_texture_resource_ =
        resource_provider->CreateResourceFromTextureMailbox(texture_mailbox_);
  }
  own_mailbox_ = false;
}

void TextureLayerImpl::FreeTextureMailbox() {
  if (!uses_mailbox_)
    return;
  if (own_mailbox_) {
    DCHECK(!external_texture_resource_);
    texture_mailbox_.RunReleaseCallback(texture_mailbox_.sync_point());
  } else if (external_texture_resource_) {
    DCHECK(!own_mailbox_);
    ResourceProvider* resource_provider =
        layer_tree_impl()->resource_provider();
    resource_provider->DeleteResource(external_texture_resource_);
    external_texture_resource_ = 0;
  }
}

}  // namespace cc
