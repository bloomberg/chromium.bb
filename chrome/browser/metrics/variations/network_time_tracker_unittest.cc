// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <math.h>

#include "base/compiler_specific.h"
#include "chrome/browser/metrics/variations/network_time_tracker.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// These are all in milliseconds.
const int64 kLatency1 = 50;
const int64 kLatency2 = 500;

// Can not be smaller than 15, it's the NowFromSystemTime() resolution.
const int64 kResolution1 = 17;
const int64 kResolution2 = 177;

const int64 kPseudoSleepTime1 = 500000001;
const int64 kPseudoSleepTime2 = 1888;

class TestNetworkTimeTracker : public NetworkTimeTracker {
 public:
  TestNetworkTimeTracker() : now_(base::Time::NowFromSystemTime()) {
  }

  void AddToTicksNow(int64 ms) {
    ticks_now_ += base::TimeDelta::FromMilliseconds(ms);
  }

  base::Time now() const {
    return now_ + (ticks_now_ - base::TimeTicks());
  }

  void ValidateExpectedTime() {
    base::Time network_time;
    base::TimeDelta uncertainty;
    EXPECT_TRUE(GetNetworkTime(&network_time, &uncertainty));
    EXPECT_LE(fabs(static_cast<double>(now().ToInternalValue() -
                                       network_time.ToInternalValue())),
              static_cast<double>(uncertainty.ToInternalValue()));
  }

 protected:
  virtual base::TimeTicks GetTicksNow() const OVERRIDE {
    return ticks_now_;
  }

  base::TimeTicks ticks_now_;
  base::Time now_;
};

}  // namespace.

TEST(NetworkTimeTrackerTest, ProperTimeTracking) {
  TestNetworkTimeTracker network_time_tracker;

  // Should not return a value before UpdateNetworkTime gets called.
  base::Time network_time;
  base::TimeDelta uncertainty;
  EXPECT_FALSE(network_time_tracker.GetNetworkTime(&network_time,
                                                   &uncertainty));

  network_time_tracker.UpdateNetworkTime(
      network_time_tracker.now(),
      base::TimeDelta::FromMilliseconds(kResolution1),
      base::TimeDelta::FromMilliseconds(kLatency1));
  network_time_tracker.ValidateExpectedTime();

  // Fake a wait for kPseudoSleepTime1 to make sure we keep tracking.
  network_time_tracker.AddToTicksNow(kPseudoSleepTime1);
  network_time_tracker.ValidateExpectedTime();

  // Update the time with a new now value and kLatency2.
  network_time_tracker.UpdateNetworkTime(
      network_time_tracker.now(),
      base::TimeDelta::FromMilliseconds(kResolution2),
      base::TimeDelta::FromMilliseconds(kLatency2));

  // Fake a wait for kPseudoSleepTime2 to make sure we keep tracking still.
  network_time_tracker.AddToTicksNow(kPseudoSleepTime2);
  network_time_tracker.ValidateExpectedTime();
}
