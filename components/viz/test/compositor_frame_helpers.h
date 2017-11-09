// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_TEST_COMPOSITOR_FRAME_HELPERS_H_
#define COMPONENTS_VIZ_TEST_COMPOSITOR_FRAME_HELPERS_H_

#include <vector>

#include "components/viz/common/resources/transferable_resource.h"
#include "components/viz/common/surfaces/surface_id.h"

namespace viz {
class CompositorFrame;

namespace test {

// Creates a valid CompositorFrame.
CompositorFrame MakeCompositorFrame(const gfx::Size& size = gfx::Size(20, 20));

// Creates a CompositorFrame that will be valid once its render_pass_list is
// initialized.
CompositorFrame MakeEmptyCompositorFrame();

CompositorFrame MakeCompositorFrame(
    std::vector<SurfaceId> activation_dependencies,
    std::vector<SurfaceId> referenced_surfaces,
    std::vector<TransferableResource> resource_list);

}  // namespace test
}  // namespace viz

#endif  // COMPONENTS_VIZ_TEST_COMPOSITOR_FRAME_HELPERS_H_
