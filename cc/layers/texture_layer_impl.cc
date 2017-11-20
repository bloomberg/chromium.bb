// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/texture_layer_impl.h"

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "base/strings/stringprintf.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/occlusion.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/common/quads/texture_draw_quad.h"
#include "components/viz/common/resources/platform_color.h"
#include "components/viz/common/resources/single_release_callback.h"

namespace cc {

TextureLayerImpl::TextureLayerImpl(LayerTreeImpl* tree_impl, int id)
    : LayerImpl(tree_impl, id) {}

TextureLayerImpl::~TextureLayerImpl() {
  FreeTransferableResource();
}

void TextureLayerImpl::SetTransferableResource(
    const viz::TransferableResource& resource,
    std::unique_ptr<viz::SingleReleaseCallback> release_callback) {
  DCHECK_EQ(resource.mailbox_holder.mailbox.IsZero(), !release_callback);
  FreeTransferableResource();
  transferable_resource_ = resource;
  release_callback_ = std::move(release_callback);
  own_resource_ = true;
  SetNeedsPushProperties();
}

std::unique_ptr<LayerImpl> TextureLayerImpl::CreateLayerImpl(
    LayerTreeImpl* tree_impl) {
  return TextureLayerImpl::Create(tree_impl, id());
}

bool TextureLayerImpl::IsSnapped() {
  return true;
}

void TextureLayerImpl::PushPropertiesTo(LayerImpl* layer) {
  LayerImpl::PushPropertiesTo(layer);
  TextureLayerImpl* texture_layer = static_cast<TextureLayerImpl*>(layer);
  texture_layer->SetFlipped(flipped_);
  texture_layer->SetUVTopLeft(uv_top_left_);
  texture_layer->SetUVBottomRight(uv_bottom_right_);
  texture_layer->SetVertexOpacity(vertex_opacity_);
  texture_layer->SetPremultipliedAlpha(premultiplied_alpha_);
  texture_layer->SetBlendBackgroundColor(blend_background_color_);
  texture_layer->SetNearestNeighbor(nearest_neighbor_);
  if (own_resource_) {
    texture_layer->SetTransferableResource(transferable_resource_,
                                           std::move(release_callback_));
    own_resource_ = false;
  }
}

bool TextureLayerImpl::WillDraw(DrawMode draw_mode,
                                LayerTreeResourceProvider* resource_provider) {
  if (draw_mode == DRAW_MODE_RESOURCELESS_SOFTWARE)
    return false;
  // These imply some synchronization problem where the compositor is in gpu
  // compositing but the client thinks it is in software, or vice versa. These
  // should only happen transiently, and should resolve when the client hears
  // about the mode switch.
  if (draw_mode == DRAW_MODE_HARDWARE && transferable_resource_.is_software) {
    DLOG(ERROR) << "Gpu compositor has software resource in TextureLayer";
    return false;
  }
  if (draw_mode == DRAW_MODE_SOFTWARE && !transferable_resource_.is_software) {
    DLOG(ERROR) << "Software compositor has gpu resource in TextureLayer";
    return false;
  }

  if (own_resource_) {
    DCHECK(!resource_id_);
    if (!transferable_resource_.mailbox_holder.mailbox.IsZero()) {
      resource_id_ = resource_provider->ImportResource(
          transferable_resource_, std::move(release_callback_));
      DCHECK(resource_id_);
    }
    own_resource_ = false;
  }

  return resource_id_ && LayerImpl::WillDraw(draw_mode, resource_provider);
}

void TextureLayerImpl::AppendQuads(viz::RenderPass* render_pass,
                                   AppendQuadsData* append_quads_data) {
  DCHECK(resource_id_);

  SkColor bg_color =
      blend_background_color_ ? background_color() : SK_ColorTRANSPARENT;
  bool are_contents_opaque =
      contents_opaque() || (SkColorGetA(bg_color) == 0xFF);

  viz::SharedQuadState* shared_quad_state =
      render_pass->CreateAndAppendSharedQuadState();
  PopulateSharedQuadState(shared_quad_state, are_contents_opaque);

  AppendDebugBorderQuad(render_pass, gfx::Rect(bounds()), shared_quad_state,
                        append_quads_data);

  gfx::Rect quad_rect(bounds());
  gfx::Rect visible_quad_rect =
      draw_properties().occlusion_in_content_space.GetUnoccludedContentRect(
          quad_rect);
  bool needs_blending = !are_contents_opaque;
  if (visible_quad_rect.IsEmpty())
    return;

  if (!vertex_opacity_[0] && !vertex_opacity_[1] && !vertex_opacity_[2] &&
      !vertex_opacity_[3])
    return;

  auto* quad = render_pass->CreateAndAppendDrawQuad<viz::TextureDrawQuad>();
  quad->SetNew(shared_quad_state, quad_rect, visible_quad_rect, needs_blending,
               resource_id_, premultiplied_alpha_, uv_top_left_,
               uv_bottom_right_, bg_color, vertex_opacity_, flipped_,
               nearest_neighbor_, false);
  quad->set_resource_size_in_pixels(transferable_resource_.size);
  ValidateQuadResources(quad);
}

SimpleEnclosedRegion TextureLayerImpl::VisibleOpaqueRegion() const {
  if (contents_opaque())
    return SimpleEnclosedRegion(visible_layer_rect());

  if (blend_background_color_ && (SkColorGetA(background_color()) == 0xFF))
    return SimpleEnclosedRegion(visible_layer_rect());

  return SimpleEnclosedRegion();
}

void TextureLayerImpl::ReleaseResources() {
  FreeTransferableResource();
  resource_id_ = 0;
}

void TextureLayerImpl::SetPremultipliedAlpha(bool premultiplied_alpha) {
  premultiplied_alpha_ = premultiplied_alpha;
  SetNeedsPushProperties();
}

void TextureLayerImpl::SetBlendBackgroundColor(bool blend) {
  blend_background_color_ = blend;
  SetNeedsPushProperties();
}

void TextureLayerImpl::SetFlipped(bool flipped) {
  flipped_ = flipped;
  SetNeedsPushProperties();
}

void TextureLayerImpl::SetNearestNeighbor(bool nearest_neighbor) {
  nearest_neighbor_ = nearest_neighbor;
  SetNeedsPushProperties();
}

void TextureLayerImpl::SetUVTopLeft(const gfx::PointF& top_left) {
  uv_top_left_ = top_left;
  SetNeedsPushProperties();
}

void TextureLayerImpl::SetUVBottomRight(const gfx::PointF& bottom_right) {
  uv_bottom_right_ = bottom_right;
  SetNeedsPushProperties();
}

// 1--2
// |  |
// 0--3
void TextureLayerImpl::SetVertexOpacity(const float vertex_opacity[4]) {
  vertex_opacity_[0] = vertex_opacity[0];
  vertex_opacity_[1] = vertex_opacity[1];
  vertex_opacity_[2] = vertex_opacity[2];
  vertex_opacity_[3] = vertex_opacity[3];
  SetNeedsPushProperties();
}

const char* TextureLayerImpl::LayerTypeAsString() const {
  return "cc::TextureLayerImpl";
}

void TextureLayerImpl::FreeTransferableResource() {
  if (own_resource_) {
    DCHECK(!resource_id_);
    if (release_callback_) {
      // We didn't use the resource, so don't need to return a SyncToken.
      release_callback_->Run(gpu::SyncToken(), false);
    }
    transferable_resource_ = viz::TransferableResource();
    release_callback_ = nullptr;
  } else if (resource_id_) {
    DCHECK(!own_resource_);
    auto* resource_provider = layer_tree_impl()->resource_provider();
    resource_provider->RemoveImportedResource(resource_id_);
    resource_id_ = 0;
  }
}

}  // namespace cc
