// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/mock_compositor_frame_sink_support_client.h"

#include "cc/output/begin_frame_args.h"
#include "cc/surfaces/local_surface_id.h"
#include "ui/gfx/geometry/rect.h"

namespace cc {
namespace test {

MockCompositorFrameSinkSupportClient::MockCompositorFrameSinkSupportClient() =
    default;

MockCompositorFrameSinkSupportClient::~MockCompositorFrameSinkSupportClient() =
    default;

}  // namespace test

}  // namespace cc
