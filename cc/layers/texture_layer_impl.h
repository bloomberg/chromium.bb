// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_TEXTURE_LAYER_IMPL_H_
#define CC_LAYERS_TEXTURE_LAYER_IMPL_H_

#include <string>

#include "base/callback.h"
#include "cc/base/cc_export.h"
#include "cc/layers/layer_impl.h"

namespace cc {

class CC_EXPORT TextureLayerImpl : public LayerImpl {
 public:
  static scoped_ptr<TextureLayerImpl> Create(LayerTreeImpl* tree_impl,
                                             int id,
                                             bool uses_mailbox) {
    return make_scoped_ptr(new TextureLayerImpl(tree_impl, id, uses_mailbox));
  }
  virtual ~TextureLayerImpl();

  virtual scoped_ptr<LayerImpl> CreateLayerImpl(LayerTreeImpl* layer_tree_impl)
      OVERRIDE;
  virtual void PushPropertiesTo(LayerImpl* layer) OVERRIDE;

  virtual void WillDraw(ResourceProvider* resource_provider) OVERRIDE;
  virtual void AppendQuads(QuadSink* quad_sink,
                           AppendQuadsData* append_quads_data) OVERRIDE;
  virtual void DidDraw(ResourceProvider* resource_provider) OVERRIDE;

  virtual void DidLoseOutputSurface() OVERRIDE;

  virtual void DumpLayerProperties(std::string* str, int indent) const OVERRIDE;
  virtual void DidBecomeActive() OVERRIDE;

  unsigned texture_id() const { return texture_id_; }
  void set_texture_id(unsigned id) { texture_id_ = id; }
  void set_premultiplied_alpha(bool premultiplied_alpha) {
    premultiplied_alpha_ = premultiplied_alpha;
  }
  void set_flipped(bool flipped) { flipped_ = flipped; }
  void set_uv_top_left(gfx::PointF top_left) { uv_top_left_ = top_left; }
  void set_uv_bottom_right(gfx::PointF bottom_right) {
    uv_bottom_right_ = bottom_right;
  }

  // 1--2
  // |  |
  // 0--3
  void set_vertex_opacity(const float vertex_opacity[4]) {
    vertex_opacity_[0] = vertex_opacity[0];
    vertex_opacity_[1] = vertex_opacity[1];
    vertex_opacity_[2] = vertex_opacity[2];
    vertex_opacity_[3] = vertex_opacity[3];
  }

  virtual bool CanClipSelf() const OVERRIDE;

  void SetTextureMailbox(const TextureMailbox& mailbox);

 private:
  TextureLayerImpl(LayerTreeImpl* tree_impl, int id, bool uses_mailbox);

  virtual const char* LayerTypeAsString() const OVERRIDE;
  void FreeTextureMailbox();

  unsigned texture_id_;
  ResourceProvider::ResourceId external_texture_resource_;
  bool premultiplied_alpha_;
  bool flipped_;
  gfx::PointF uv_top_left_;
  gfx::PointF uv_bottom_right_;
  float vertex_opacity_[4];

  TextureMailbox texture_mailbox_;
  bool uses_mailbox_;
  bool own_mailbox_;

  DISALLOW_COPY_AND_ASSIGN(TextureLayerImpl);
};

}  // namespace cc

#endif  // CC_LAYERS_TEXTURE_LAYER_IMPL_H_
