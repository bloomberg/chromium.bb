// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_CLIENT_HIT_TEST_DATA_PROVIDER_SURFACE_LAYER_H_
#define COMPONENTS_VIZ_CLIENT_HIT_TEST_DATA_PROVIDER_SURFACE_LAYER_H_

#include "base/macros.h"
#include "components/viz/client/hit_test_data_provider.h"
#include "components/viz/client/viz_client_export.h"

namespace cc {

class LayerTreeHostImpl;
}

namespace viz {

// HitTestDataProviderSurfaceLayer is used to extract hit test data from
// cc::LayerTreeHostImpl when submitting CompositorFrame.
class VIZ_CLIENT_EXPORT HitTestDataProviderSurfaceLayer
    : public HitTestDataProvider {
 public:
  HitTestDataProviderSurfaceLayer() = default;

  mojom::HitTestRegionListPtr GetHitTestData(
      const CompositorFrame& compositor_frame) const override;

  void UpdateLayerTreeHostImpl(const cc::LayerTreeHostImpl* host_impl) override;

 private:
  const cc::LayerTreeHostImpl* host_impl_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(HitTestDataProviderSurfaceLayer);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_CLIENT_HIT_TEST_DATA_PROVIDER_SURFACE_LAYER_H_
