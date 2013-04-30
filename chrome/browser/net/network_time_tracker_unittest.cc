// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <math.h>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/compiler_specific.h"
#include "base/message_loop.h"
#include "chrome/browser/net/network_time_tracker.h"
#include "content/public/test/test_browser_thread.h"
#include "net/base/network_time_notifier.h"
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

// A custom tick clock that will return an arbitrary time.
class TestTickClock : public base::TickClock {
 public:
  explicit TestTickClock(base::TimeTicks* ticks_now) : ticks_now_(ticks_now) {}
  virtual ~TestTickClock() {}

  virtual base::TimeTicks NowTicks() OVERRIDE {
    return *ticks_now_;
  }

 private:
  base::TimeTicks* ticks_now_;
};

}  // namespace

class NetworkTimeTrackerTest : public testing::Test {
 public:
  NetworkTimeTrackerTest()
      : ui_thread(content::BrowserThread::UI, &message_loop_),
        io_thread(content::BrowserThread::IO, &message_loop_),
        now_(base::Time::NowFromSystemTime()),
        tick_clock_(new TestTickClock(&ticks_now_)),
        network_time_notifier_(
            new net::NetworkTimeNotifier(
                tick_clock_.PassAs<base::TickClock>())) {}
  virtual ~NetworkTimeTrackerTest() {}

  virtual void TearDown() OVERRIDE {
    message_loop_.RunUntilIdle();
  }

  base::Time Now() const {
    return now_ + (ticks_now_ - base::TimeTicks());
  }

  base::TimeTicks TicksNow() const {
    return ticks_now_;
  }

  void AddToTicksNow(int64 ms) {
    ticks_now_ += base::TimeDelta::FromMilliseconds(ms);
  }

  void StartTracker() {
    network_time_tracker_.reset(new NetworkTimeTracker());
    network_time_notifier_->AddObserver(
        network_time_tracker_->BuildObserverCallback());
    message_loop_.RunUntilIdle();
  }

  void StopTracker() {
    network_time_tracker_.reset();
  }

  // Updates the notifier's time with the specified parameters and waits until
  // the observers have been updated.
  void UpdateNetworkTime(const base::Time& network_time,
                         const base::TimeDelta& resolution,
                         const base::TimeDelta& latency,
                         const base::TimeTicks& post_time) {
    message_loop_.PostTask(
        FROM_HERE,
        base::Bind(&net::NetworkTimeNotifier::UpdateNetworkTime,
                   base::Unretained(network_time_notifier_.get()),
                   network_time,
                   resolution,
                   latency,
                   post_time));
    message_loop_.RunUntilIdle();
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
  // Message loop and threads for the tracker's internal logic.
  MessageLoop message_loop_;
  content::TestBrowserThread ui_thread;
  content::TestBrowserThread io_thread;

  // Used in building the current time that |tick_clock_| reports. See Now()
  // for details.
  base::Time now_;
  base::TimeTicks ticks_now_;

  // A custom clock that allows arbitrary time delays.
  scoped_ptr<TestTickClock> tick_clock_;

  // The network time notifier that receives time updates and posts them to
  // the tracker.
  scoped_ptr<net::NetworkTimeNotifier> network_time_notifier_;

  // The network time tracker being tested.
  scoped_ptr<NetworkTimeTracker> network_time_tracker_;
};

// Should not return a value before UpdateNetworkTime gets called.
TEST_F(NetworkTimeTrackerTest, Uninitialized) {
  base::Time network_time;
  base::TimeDelta uncertainty;
  StartTracker();
  EXPECT_FALSE(network_time_tracker()->GetNetworkTime(base::TimeTicks(),
                                                      &network_time,
                                                      &uncertainty));
}

// Verify that the the tracker receives and properly handles updates to the
// network time.
TEST_F(NetworkTimeTrackerTest, NetworkTimeUpdates) {
  StartTracker();
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

// Starting the tracker after the network time has been set with the notifier
// should update the tracker's time as well.
TEST_F(NetworkTimeTrackerTest, UpdateThenStartTracker) {
  UpdateNetworkTime(
      Now(),
      base::TimeDelta::FromMilliseconds(kResolution1),
      base::TimeDelta::FromMilliseconds(kLatency1),
      TicksNow());
  StartTracker();
  EXPECT_TRUE(ValidateExpectedTime());
}

// Time updates after the tracker has been destroyed should not attempt to
// dereference the destroyed tracker.
TEST_F(NetworkTimeTrackerTest, UpdateAfterTrackerDestroyed) {
  StartTracker();
  StopTracker();
  UpdateNetworkTime(
      Now(),
      base::TimeDelta::FromMilliseconds(kResolution1),
      base::TimeDelta::FromMilliseconds(kLatency1),
      TicksNow());
}
