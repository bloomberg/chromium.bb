// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/quads/compositor_frame.h"

namespace viz {

CompositorFrame::CompositorFrame() = default;

CompositorFrame::CompositorFrame(CompositorFrame&& other) = default;

CompositorFrame::~CompositorFrame() = default;

CompositorFrame& CompositorFrame::operator=(CompositorFrame&& other) = default;

}  // namespace viz
