// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_CLIENT_HIT_TEST_DATA_PROVIDER_SIMPLE_BOUNDS_H_
#define COMPONENTS_VIZ_CLIENT_HIT_TEST_DATA_PROVIDER_SIMPLE_BOUNDS_H_

#include "components/viz/client/hit_test_data_provider.h"

namespace viz {

// HitTestDataProviderSimpleBounds is used to derive hit test data from
// the bounds data in the CompositorFrame.
class VIZ_CLIENT_EXPORT HitTestDataProviderSimpleBounds
    : public HitTestDataProvider {
 public:
  HitTestDataProviderSimpleBounds();
  ~HitTestDataProviderSimpleBounds() override;

  mojom::HitTestRegionListPtr GetHitTestData(
      const CompositorFrame& compositor_frame) const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(HitTestDataProviderSimpleBounds);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_CLIENT_HIT_TEST_DATA_PROVIDER_SIMPLE_BOUNDS_H_
