// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/network_time/network_time_tracker.h"

#include <math.h>
#include <stdint.h>

#include "base/compiler_specific.h"
#include "base/time/tick_clock.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network_time {

namespace {

// These are all in milliseconds.
const int64_t kLatency1 = 50;
const int64_t kLatency2 = 500;

// Can not be smaller than 15, it's the NowFromSystemTime() resolution.
const int64_t kResolution1 = 17;
const int64_t kResolution2 = 177;

const int64_t kPseudoSleepTime1 = 500000001;
const int64_t kPseudoSleepTime2 = 1888;

// A custom tick clock that will return an arbitrary time.
class TestTickClock : public base::TickClock {
 public:
  explicit TestTickClock(base::TimeTicks* ticks_now) : ticks_now_(ticks_now) {}
  ~TestTickClock() override {}

  base::TimeTicks NowTicks() override { return *ticks_now_; }

 private:
  base::TimeTicks* ticks_now_;
};

}  // namespace

class NetworkTimeTrackerTest : public testing::Test {
 public:
  ~NetworkTimeTrackerTest() override {}

  void SetUp() override {
    NetworkTimeTracker::RegisterPrefs(pref_service_.registry());

    now_ = base::Time::NowFromSystemTime();
    network_time_tracker_.reset(new NetworkTimeTracker(
        scoped_ptr<base::TickClock>(new TestTickClock(&ticks_now_)),
        &pref_service_));
  }

  base::Time Now() const {
    return now_ + (ticks_now_ - base::TimeTicks());
  }

  base::TimeTicks TicksNow() const {
    return ticks_now_;
  }

  void AddToTicksNow(int64_t ms) {
    ticks_now_ += base::TimeDelta::FromMilliseconds(ms);
  }

  // Updates the notifier's time with the specified parameters.
  void UpdateNetworkTime(const base::Time& network_time,
                         const base::TimeDelta& resolution,
                         const base::TimeDelta& latency,
                         const base::TimeTicks& post_time) {
    network_time_tracker_->UpdateNetworkTime(
        network_time, resolution, latency, post_time);
  }

  // Ensures the network time tracker has a network time and that the
  // disparity between the network time version of |ticks_now_| and the actual
  // |ticks_now_| value is within the uncertainty (should always be true
  // because the network time notifier uses |ticks_now_| for the tick clock).
  testing::AssertionResult ValidateExpectedTime() const {
    base::Time network_time;
    base::TimeDelta uncertainty;
    if (!network_time_tracker_->GetNetworkTime(TicksNow(),
                                               &network_time,
                                               &uncertainty))
      return testing::AssertionFailure() << "Failed to get network time.";
    if (fabs(static_cast<double>(Now().ToInternalValue() -
                                 network_time.ToInternalValue())) >
             static_cast<double>(uncertainty.ToInternalValue())) {
      return testing::AssertionFailure()
          << "Expected network time not within uncertainty.";
    }
    return testing::AssertionSuccess();
  }

  NetworkTimeTracker* network_time_tracker() {
    return network_time_tracker_.get();
  }

 private:
  // Used in building the current time that TestTickClock reports. See Now()
  // for details.
  base::Time now_;
  base::TimeTicks ticks_now_;

  TestingPrefServiceSimple pref_service_;

  // The network time tracker being tested.
  scoped_ptr<NetworkTimeTracker> network_time_tracker_;
};

// Should not return a value before UpdateNetworkTime gets called.
TEST_F(NetworkTimeTrackerTest, Uninitialized) {
  base::Time network_time;
  base::TimeDelta uncertainty;
  EXPECT_FALSE(network_time_tracker()->GetNetworkTime(base::TimeTicks(),
                                                      &network_time,
                                                      &uncertainty));
}

// Verify that the the tracker receives and properly handles updates to the
// network time.
TEST_F(NetworkTimeTrackerTest, NetworkTimeUpdates) {
  UpdateNetworkTime(
      Now(),
      base::TimeDelta::FromMilliseconds(kResolution1),
      base::TimeDelta::FromMilliseconds(kLatency1),
      TicksNow());
  EXPECT_TRUE(ValidateExpectedTime());

  // Fake a wait for kPseudoSleepTime1 to make sure we keep tracking.
  AddToTicksNow(kPseudoSleepTime1);
  EXPECT_TRUE(ValidateExpectedTime());

  // Update the time with a new now value and kLatency2.
  UpdateNetworkTime(
      Now(),
      base::TimeDelta::FromMilliseconds(kResolution2),
      base::TimeDelta::FromMilliseconds(kLatency2),
      TicksNow());

  // Fake a wait for kPseudoSleepTime2 to make sure we keep tracking still.
  AddToTicksNow(kPseudoSleepTime2);
  EXPECT_TRUE(ValidateExpectedTime());

  // Fake a long delay between update task post time and the network notifier
  // updating its network time. The uncertainty should account for the
  // disparity.
  base::Time old_now = Now();
  base::TimeTicks old_ticks = TicksNow();
  AddToTicksNow(kPseudoSleepTime2);
  UpdateNetworkTime(
      old_now,
      base::TimeDelta::FromMilliseconds(kResolution2),
      base::TimeDelta::FromMilliseconds(kLatency2),
      old_ticks);
  EXPECT_TRUE(ValidateExpectedTime());
}

}  // namespace network_time
