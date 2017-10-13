// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_SURFACE_LAYER_H_
#define CC_LAYERS_SURFACE_LAYER_H_

#include "base/macros.h"
#include "cc/cc_export.h"
#include "cc/layers/layer.h"
#include "components/viz/common/surfaces/surface_info.h"
#include "components/viz/common/surfaces/surface_reference_factory.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/size.h"

namespace cc {

// A layer that renders a surface referencing the output of another compositor
// instance or client.
class CC_EXPORT SurfaceLayer : public Layer {
 public:
  static scoped_refptr<SurfaceLayer> Create(
      scoped_refptr<viz::SurfaceReferenceFactory> ref_factory);

  void SetPrimarySurfaceInfo(const viz::SurfaceInfo& surface_info);
  void SetFallbackSurfaceId(const viz::SurfaceId& surface_id);

  // When stretch_content_to_fill_bounds is true, the scale of the embedded
  // surface is ignored and the content will be stretched to fill the bounds.
  void SetStretchContentToFillBounds(bool stretch_content_to_fill_bounds);

  // Specifies the |background_color| to use when a primary surface is
  // specified, and a fallback surface is unavailable.
  void SetDefaultBackgroundColor(SkColor background_color);

  // Layer overrides.
  std::unique_ptr<LayerImpl> CreateLayerImpl(LayerTreeImpl* tree_impl) override;
  void SetLayerTreeHost(LayerTreeHost* host) override;
  void PushPropertiesTo(LayerImpl* layer) override;

  scoped_refptr<viz::SurfaceReferenceFactory> surface_reference_factory()
      const {
    return ref_factory_;
  }

  const viz::SurfaceInfo& primary_surface_info() const {
    return primary_surface_info_;
  }

  const viz::SurfaceId& fallback_surface_id() const {
    return fallback_surface_id_;
  }

 protected:
  explicit SurfaceLayer(
      scoped_refptr<viz::SurfaceReferenceFactory> ref_factory);
  bool HasDrawableContent() const override;

 private:
  ~SurfaceLayer() override;
  void RemoveReference(base::Closure reference_returner);

  viz::SurfaceInfo primary_surface_info_;
  viz::SurfaceId fallback_surface_id_;
  base::Closure fallback_reference_returner_;

  scoped_refptr<viz::SurfaceReferenceFactory> ref_factory_;
  bool stretch_content_to_fill_bounds_ = false;
  SkColor default_background_color_ = SK_ColorWHITE;

  DISALLOW_COPY_AND_ASSIGN(SurfaceLayer);
};

}  // namespace cc

#endif  // CC_LAYERS_SURFACE_LAYER_H_
