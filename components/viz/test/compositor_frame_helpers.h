// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_TEST_COMPOSITOR_FRAME_HELPERS_H_
#define COMPONENTS_VIZ_TEST_COMPOSITOR_FRAME_HELPERS_H_

#include <vector>

#include "cc/resources/transferable_resource.h"
#include "components/viz/common/surfaces/surface_id.h"

namespace cc {
class CompositorFrame;
}

namespace viz {
namespace test {

// Creates a valid cc::CompositorFrame.
cc::CompositorFrame MakeCompositorFrame();

// Creates a cc::CompositorFrame that will be valid once its render_pass_list is
// initialized.
cc::CompositorFrame MakeEmptyCompositorFrame();

cc::CompositorFrame MakeCompositorFrame(
    std::vector<SurfaceId> activation_dependencies,
    std::vector<SurfaceId> referenced_surfaces,
    std::vector<cc::TransferableResource> resource_list);

}  // namespace test
}  // namespace viz

#endif  // COMPONENTS_VIZ_TEST_COMPOSITOR_FRAME_HELPERS_H_
