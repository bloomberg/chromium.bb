// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_SURFACE_LAYER_IMPL_H_
#define CC_LAYERS_SURFACE_LAYER_IMPL_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "cc/base/cc_export.h"
#include "cc/layers/layer_impl.h"
#include "cc/quads/surface_draw_quad.h"
#include "cc/surfaces/surface_id.h"
#include "cc/surfaces/surface_info.h"

namespace cc {

class CC_EXPORT SurfaceLayerImpl : public LayerImpl {
 public:
  static std::unique_ptr<SurfaceLayerImpl> Create(LayerTreeImpl* tree_impl,
                                                  int id) {
    return base::WrapUnique(new SurfaceLayerImpl(tree_impl, id));
  }
  ~SurfaceLayerImpl() override;

  void SetPrimarySurfaceInfo(const SurfaceInfo& surface_info);
  const SurfaceInfo& primary_surface_info() const {
    return primary_surface_info_;
  }

  void SetFallbackSurfaceInfo(const SurfaceInfo& surface_info);
  const SurfaceInfo& fallback_surface_info() const {
    return fallback_surface_info_;
  }

  void SetStretchContentToFillBounds(bool stretch_content);

  // LayerImpl overrides.
  std::unique_ptr<LayerImpl> CreateLayerImpl(LayerTreeImpl* tree_impl) override;
  void PushPropertiesTo(LayerImpl* layer) override;
  void AppendQuads(RenderPass* render_pass,
                   AppendQuadsData* append_quads_data) override;

 protected:
  SurfaceLayerImpl(LayerTreeImpl* tree_impl, int id);

 private:
  SurfaceDrawQuad* CreateSurfaceDrawQuad(
      RenderPass* render_pass,
      SurfaceDrawQuadType surface_draw_quad_type,
      const SurfaceInfo& surface_info);

  void GetDebugBorderProperties(SkColor* color, float* width) const override;
  void AppendRainbowDebugBorder(RenderPass* render_pass);
  void AsValueInto(base::trace_event::TracedValue* dict) const override;
  const char* LayerTypeAsString() const override;

  SurfaceInfo primary_surface_info_;
  SurfaceInfo fallback_surface_info_;

  bool stretch_content_to_fill_bounds_ = false;

  DISALLOW_COPY_AND_ASSIGN(SurfaceLayerImpl);
};

}  // namespace cc

#endif  // CC_LAYERS_SURFACE_LAYER_IMPL_H_
