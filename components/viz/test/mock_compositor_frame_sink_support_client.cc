// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/test/mock_compositor_frame_sink_support_client.h"

#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "components/viz/common/surfaces/local_surface_id.h"
#include "ui/gfx/geometry/rect.h"

namespace viz {
namespace test {

MockCompositorFrameSinkSupportClient::MockCompositorFrameSinkSupportClient() =
    default;

MockCompositorFrameSinkSupportClient::~MockCompositorFrameSinkSupportClient() =
    default;

}  // namespace test

}  // namespace viz
