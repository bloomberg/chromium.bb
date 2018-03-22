// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_CLIENT_HIT_TEST_DATA_PROVIDER_H_
#define COMPONENTS_VIZ_CLIENT_HIT_TEST_DATA_PROVIDER_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "components/viz/client/viz_client_export.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "services/viz/public/interfaces/hit_test/hit_test_region_list.mojom.h"

namespace cc {
class LayerTreeHostImpl;
}

namespace viz {

class VIZ_CLIENT_EXPORT HitTestDataProvider {
 public:
  HitTestDataProvider() = default;
  virtual ~HitTestDataProvider() = default;

  // Returns an array of hit-test regions. May return nullptr to disable
  // hit-testing.
  virtual mojom::HitTestRegionListPtr GetHitTestData(
      const CompositorFrame& compositor_frame) const = 0;

  // Exclusively called by HitTestDataProviderSurfaceLayer.
  virtual void UpdateLayerTreeHostImpl(const cc::LayerTreeHostImpl* host_impl) {
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(HitTestDataProvider);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_CLIENT_HIT_TEST_DATA_PROVIDER_H_
