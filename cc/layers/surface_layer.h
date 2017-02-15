// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_SURFACE_LAYER_H_
#define CC_LAYERS_SURFACE_LAYER_H_

#include "base/macros.h"
#include "cc/base/cc_export.h"
#include "cc/layers/layer.h"
#include "cc/surfaces/surface_info.h"
#include "cc/surfaces/surface_reference_factory.h"
#include "ui/gfx/geometry/size.h"

namespace cc {

// A layer that renders a surface referencing the output of another compositor
// instance or client.
class CC_EXPORT SurfaceLayer : public Layer {
 public:
  static scoped_refptr<SurfaceLayer> Create(
      scoped_refptr<SurfaceReferenceFactory> ref_factory);

  void SetPrimarySurfaceInfo(const SurfaceInfo& surface_info);
  void SetFallbackSurfaceInfo(const SurfaceInfo& surface_info);

  // When stretch_content_to_fill_bounds is true, the scale of the embedded
  // surface is ignored and the content will be stretched to fill the bounds.
  void SetStretchContentToFillBounds(bool stretch_content_to_fill_bounds);

  // Layer overrides.
  std::unique_ptr<LayerImpl> CreateLayerImpl(LayerTreeImpl* tree_impl) override;
  void SetLayerTreeHost(LayerTreeHost* host) override;
  void PushPropertiesTo(LayerImpl* layer) override;

  scoped_refptr<SurfaceReferenceFactory> surface_reference_factory() const {
    return ref_factory_;
  }

  const SurfaceInfo& primary_surface_info() const {
    return primary_surface_info_;
  }

  const SurfaceInfo& fallback_surface_info() const {
    return fallback_surface_info_;
  }

 protected:
  explicit SurfaceLayer(scoped_refptr<SurfaceReferenceFactory> ref_factory);
  bool HasDrawableContent() const override;

 private:
  ~SurfaceLayer() override;
  void RemoveReference(base::Closure reference_returner);

  SurfaceInfo primary_surface_info_;
  base::Closure primary_reference_returner_;

  SurfaceInfo fallback_surface_info_;
  base::Closure fallback_reference_returner_;

  scoped_refptr<SurfaceReferenceFactory> ref_factory_;
  bool stretch_content_to_fill_bounds_ = false;

  DISALLOW_COPY_AND_ASSIGN(SurfaceLayer);
};

}  // namespace cc

#endif  // CC_LAYERS_SURFACE_LAYER_H_
