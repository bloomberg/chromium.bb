// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_TEXTURE_LAYER_IMPL_H_
#define CC_LAYERS_TEXTURE_LAYER_IMPL_H_

#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "cc/cc_export.h"
#include "cc/layers/layer_impl.h"

namespace cc {
class SingleReleaseCallbackImpl;
class ScopedResource;

class CC_EXPORT TextureLayerImpl : public LayerImpl {
 public:
  static std::unique_ptr<TextureLayerImpl> Create(LayerTreeImpl* tree_impl,
                                                  int id) {
    return base::WrapUnique(new TextureLayerImpl(tree_impl, id));
  }
  ~TextureLayerImpl() override;

  std::unique_ptr<LayerImpl> CreateLayerImpl(
      LayerTreeImpl* layer_tree_impl) override;
  bool IsSnapped() override;
  void PushPropertiesTo(LayerImpl* layer) override;

  bool WillDraw(DrawMode draw_mode,
                ResourceProvider* resource_provider) override;
  void AppendQuads(RenderPass* render_pass,
                   AppendQuadsData* append_quads_data) override;
  SimpleEnclosedRegion VisibleOpaqueRegion() const override;
  void ReleaseResources() override;

  // These setter methods don't cause any implicit damage, so the texture client
  // must explicitly invalidate if they intend to cause a visible change in the
  // layer's output.
  void SetTextureId(unsigned id);
  void SetPremultipliedAlpha(bool premultiplied_alpha);
  void SetBlendBackgroundColor(bool blend);
  void SetFlipped(bool flipped);
  void SetNearestNeighbor(bool nearest_neighbor);
  void SetUVTopLeft(const gfx::PointF& top_left);
  void SetUVBottomRight(const gfx::PointF& bottom_right);

  // 1--2
  // |  |
  // 0--3
  void SetVertexOpacity(const float vertex_opacity[4]);

  void SetTextureMailbox(
      const viz::TextureMailbox& mailbox,
      std::unique_ptr<SingleReleaseCallbackImpl> release_callback);

 private:
  TextureLayerImpl(LayerTreeImpl* tree_impl, int id);

  const char* LayerTypeAsString() const override;
  void FreeTextureMailbox();

  ResourceId external_texture_resource_;
  bool premultiplied_alpha_;
  bool blend_background_color_;
  bool flipped_;
  bool nearest_neighbor_;
  gfx::PointF uv_top_left_;
  gfx::PointF uv_bottom_right_;
  float vertex_opacity_[4];
  // This is a resource that's a GL copy of a software texture mailbox.
  std::unique_ptr<ScopedResource> texture_copy_;

  viz::TextureMailbox texture_mailbox_;
  std::unique_ptr<SingleReleaseCallbackImpl> release_callback_;
  bool own_mailbox_;
  bool valid_texture_copy_;

  DISALLOW_COPY_AND_ASSIGN(TextureLayerImpl);
};

}  // namespace cc

#endif  // CC_LAYERS_TEXTURE_LAYER_IMPL_H_
