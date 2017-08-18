// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/test/test_frame_sink_manager.h"

namespace viz {

TestFrameSinkManagerImpl::TestFrameSinkManagerImpl() : binding_(this) {}

TestFrameSinkManagerImpl::~TestFrameSinkManagerImpl() {}

void TestFrameSinkManagerImpl::BindRequest(
    mojom::FrameSinkManagerRequest request) {
  binding_.Bind(std::move(request));
}

}  // namespace viz
