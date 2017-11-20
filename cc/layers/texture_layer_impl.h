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
#include "components/viz/common/resources/transferable_resource.h"

namespace viz {
class SingleReleaseCallback;
}

namespace cc {

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
                LayerTreeResourceProvider* resource_provider) override;
  void AppendQuads(viz::RenderPass* render_pass,
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

  void SetTransferableResource(
      const viz::TransferableResource& resource,
      std::unique_ptr<viz::SingleReleaseCallback> release_callback);

 private:
  TextureLayerImpl(LayerTreeImpl* tree_impl, int id);

  const char* LayerTypeAsString() const override;
  void FreeTransferableResource();

  bool premultiplied_alpha_ = true;
  bool blend_background_color_ = false;
  bool flipped_ = true;
  bool nearest_neighbor_ = false;
  gfx::PointF uv_top_left_ = gfx::PointF();
  gfx::PointF uv_bottom_right_ = gfx::PointF(1.f, 1.f);
  float vertex_opacity_[4] = {1.f, 1.f, 1.f, 1.f};

  // True while the |transferable_resource_| is owned by this layer, and
  // becomes false once it is passed to another layer or to the
  // LayerTreeResourceProvider, at which point we get back a |resource_id_|.
  bool own_resource_ = false;
  // A TransferableResource from the layer's client that will be given
  // to the display compositor.
  viz::TransferableResource transferable_resource_;
  // Local ResourceId for the TransferableResource, to be used with the
  // compositor's LayerTreeResourceProvider in order to refer to the
  // TransferableResource given to it.
  viz::ResourceId resource_id_ = 0;
  std::unique_ptr<viz::SingleReleaseCallback> release_callback_;

  DISALLOW_COPY_AND_ASSIGN(TextureLayerImpl);
};

}  // namespace cc

#endif  // CC_LAYERS_TEXTURE_LAYER_IMPL_H_
