// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/copresence/handlers/audio/tick_clock_ref_counted.h"

#include "base/time/tick_clock.h"

namespace copresence {

TickClockRefCounted::TickClockRefCounted(scoped_ptr<base::TickClock> clock)
    : clock_(clock.Pass()) {
}

base::TimeTicks TickClockRefCounted::NowTicks() {
  return clock_->NowTicks();
}

TickClockRefCounted::~TickClockRefCounted() {
}

}  // namespace copresence
