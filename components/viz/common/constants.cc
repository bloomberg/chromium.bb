// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/constants.h"

namespace viz {

// We expect the begin frames that viz client read in can give a good estimate
// of the arrival time of the next begin frame. But in case the next begin frame
// is not arriving as expected, we start the polling process by spinning on the
// begin frame slot for |kClientSpinForBeginFrameIntervalUs| and sleep for
// |kClientSleepForBeginFrameIntervalMs| repeatedly until we see the next begin
// frame
const int64_t kClientSleepForBeginFrameIntervalMs = 1;
const int64_t kClientSpinForBeginFrameIntervalUs = 100;

const uint32_t kDefaultActivationDeadlineInFrames = 4u;

}  // namespace viz
