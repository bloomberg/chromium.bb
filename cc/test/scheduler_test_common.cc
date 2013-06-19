// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/scheduler_test_common.h"

#include "base/logging.h"

namespace cc {

void FakeTimeSourceClient::OnTimerTick() {
  tick_called_ = true;
}

base::TimeTicks FakeDelayBasedTimeSource::Now() const { return now_; }

}  // namespace cc
