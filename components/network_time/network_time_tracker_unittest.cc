// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/network_time/network_time_tracker.h"

#include <memory>

#include "base/compiler_specific.h"
#include "base/test/simple_test_clock.h"
#include "base/test/simple_test_tick_clock.h"
#include "components/network_time/network_time_pref_names.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network_time {

class NetworkTimeTrackerTest : public testing::Test {
 public:
  ~NetworkTimeTrackerTest() override {}

  void SetUp() override {
    NetworkTimeTracker::RegisterPrefs(pref_service_.registry());

    clock_ = new base::SimpleTestClock();
    tick_clock_ = new base::SimpleTestTickClock();
    // Do this to be sure that |is_null| returns false.
    clock_->Advance(base::TimeDelta::FromDays(111));
    tick_clock_->Advance(base::TimeDelta::FromDays(222));

    tracker_.reset(new NetworkTimeTracker(
        std::unique_ptr<base::Clock>(clock_),
        std::unique_ptr<base::TickClock>(tick_clock_), &pref_service_));

    // Can not be smaller than 15, it's the NowFromSystemTime() resolution.
    resolution_ = base::TimeDelta::FromMilliseconds(17);
    latency_ = base::TimeDelta::FromMilliseconds(50);
    adjustment_ = 7 * base::TimeDelta::FromMilliseconds(kTicksResolutionMs);
  }

  // Replaces |tracker_| with a new object, while preserving the
  // testing clocks.
  void Reset() {
    base::SimpleTestClock* new_clock = new base::SimpleTestClock();
    new_clock->SetNow(clock_->Now());
    base::SimpleTestTickClock* new_tick_clock = new base::SimpleTestTickClock();
    new_tick_clock->SetNowTicks(tick_clock_->NowTicks());
    clock_ = new_clock;
    tick_clock_= new_tick_clock;
    tracker_.reset(new NetworkTimeTracker(
        std::unique_ptr<base::Clock>(clock_),
        std::unique_ptr<base::TickClock>(tick_clock_), &pref_service_));
  }

  // Updates the notifier's time with the specified parameters.
  void UpdateNetworkTime(const base::Time& network_time,
                         const base::TimeDelta& resolution,
                         const base::TimeDelta& latency,
                         const base::TimeTicks& post_time) {
    tracker_->UpdateNetworkTime(
        network_time, resolution, latency, post_time);
  }

  // Advances both the system clock and the tick clock.  This should be used for
  // the normal passage of time, i.e. when neither clock is doing anything odd.
  void AdvanceBoth(const base::TimeDelta& delta) {
    tick_clock_->Advance(delta);
    clock_->Advance(delta);
  }

 protected:
  base::TimeDelta resolution_;
  base::TimeDelta latency_;
  base::TimeDelta adjustment_;
  base::SimpleTestClock* clock_;
  base::SimpleTestTickClock* tick_clock_;
  TestingPrefServiceSimple pref_service_;
  std::unique_ptr<NetworkTimeTracker> tracker_;
};

TEST_F(NetworkTimeTrackerTest, Uninitialized) {
  base::Time network_time;
  base::TimeDelta uncertainty;
  EXPECT_FALSE(tracker_->GetNetworkTime(&network_time, &uncertainty));
}

TEST_F(NetworkTimeTrackerTest, LongPostingDelay) {
  // The request arrives at the server, which records the time.  Advance the
  // clock to simulate the latency of sending the reply, which we'll say for
  // convenience is half the total latency.
  base::Time in_network_time = clock_->Now();
  AdvanceBoth(latency_ / 2);

  // Record the tick counter at the time the reply is received.  At this point,
  // we would post UpdateNetworkTime to be run on the browser thread.
  base::TimeTicks posting_time = tick_clock_->NowTicks();

  // Simulate that it look a long time (1888us) for the browser thread to get
  // around to executing the update.
  AdvanceBoth(base::TimeDelta::FromMicroseconds(1888));
  UpdateNetworkTime(in_network_time, resolution_, latency_, posting_time);

  base::Time out_network_time;
  base::TimeDelta uncertainty;
  EXPECT_TRUE(tracker_->GetNetworkTime(&out_network_time,  &uncertainty));
  EXPECT_EQ(resolution_ + latency_ + adjustment_, uncertainty);
  EXPECT_EQ(clock_->Now(), out_network_time);
}

TEST_F(NetworkTimeTrackerTest, LopsidedLatency) {
  // Simulate that the server received the request instantaneously, and that all
  // of the latency was in sending the reply.  (This contradicts the assumption
  // in the code.)
  base::Time in_network_time = clock_->Now();
  AdvanceBoth(latency_);
  UpdateNetworkTime(in_network_time, resolution_, latency_,
                    tick_clock_->NowTicks());

  // But, the answer is still within the uncertainty bounds!
  base::Time out_network_time;
  base::TimeDelta uncertainty;
  EXPECT_TRUE(tracker_->GetNetworkTime(&out_network_time,  &uncertainty));
  EXPECT_LT(out_network_time - uncertainty / 2, clock_->Now());
  EXPECT_GT(out_network_time + uncertainty / 2, clock_->Now());
}

TEST_F(NetworkTimeTrackerTest, ClockIsWack) {
  // Now let's assume the system clock is completely wrong.
  base::Time in_network_time = clock_->Now() - base::TimeDelta::FromDays(90);
  UpdateNetworkTime(in_network_time - latency_ / 2, resolution_, latency_,
                    tick_clock_->NowTicks());

  base::Time out_network_time;
  EXPECT_TRUE(tracker_->GetNetworkTime(&out_network_time, NULL));
  EXPECT_EQ(in_network_time, out_network_time);
}

TEST_F(NetworkTimeTrackerTest, ClocksDivergeSlightly) {
  // The two clocks are allowed to diverge a little bit.
  base::Time in_network_time = clock_->Now();
  UpdateNetworkTime(in_network_time - latency_ / 2, resolution_, latency_,
                    tick_clock_->NowTicks());

  base::TimeDelta small = base::TimeDelta::FromSeconds(30);
  tick_clock_->Advance(small);
  base::Time out_network_time;
  base::TimeDelta out_uncertainty;
  EXPECT_TRUE(tracker_->GetNetworkTime(&out_network_time, &out_uncertainty));
  EXPECT_EQ(in_network_time + small, out_network_time);
  // The clock divergence should show up in the uncertainty.
  EXPECT_EQ(resolution_ + latency_ + adjustment_ + small, out_uncertainty);
}

TEST_F(NetworkTimeTrackerTest, NetworkTimeUpdates) {
  // Verify that the the tracker receives and properly handles updates to the
  // network time.
  base::Time out_network_time;
  base::TimeDelta uncertainty;

  UpdateNetworkTime(clock_->Now() - latency_ / 2, resolution_, latency_,
                    tick_clock_->NowTicks());
  EXPECT_TRUE(tracker_->GetNetworkTime(&out_network_time,  &uncertainty));
  EXPECT_EQ(clock_->Now(), out_network_time);
  EXPECT_EQ(resolution_ + latency_ + adjustment_, uncertainty);

  // Fake a wait to make sure we keep tracking.
  AdvanceBoth(base::TimeDelta::FromSeconds(1));
  EXPECT_TRUE(tracker_->GetNetworkTime(&out_network_time,  &uncertainty));
  EXPECT_EQ(clock_->Now(), out_network_time);
  EXPECT_EQ(resolution_ + latency_ + adjustment_, uncertainty);

  // And one more time.
  UpdateNetworkTime(clock_->Now() - latency_ / 2, resolution_, latency_,
                    tick_clock_->NowTicks());
  AdvanceBoth(base::TimeDelta::FromSeconds(1));
  EXPECT_TRUE(tracker_->GetNetworkTime(&out_network_time,  &uncertainty));
  EXPECT_EQ(clock_->Now(), out_network_time);
  EXPECT_EQ(resolution_ + latency_ + adjustment_, uncertainty);
}

TEST_F(NetworkTimeTrackerTest, SpringForward) {
  // Simulate the wall clock advancing faster than the tick clock.
  UpdateNetworkTime(clock_->Now(), resolution_, latency_,
                    tick_clock_->NowTicks());
  tick_clock_->Advance(base::TimeDelta::FromSeconds(1));
  clock_->Advance(base::TimeDelta::FromDays(1));
  base::Time out_network_time;
  EXPECT_FALSE(tracker_->GetNetworkTime(&out_network_time, NULL));
}

TEST_F(NetworkTimeTrackerTest, FallBack) {
  // Simulate the wall clock running backward.
  UpdateNetworkTime(clock_->Now(), resolution_, latency_,
                    tick_clock_->NowTicks());
  tick_clock_->Advance(base::TimeDelta::FromSeconds(1));
  clock_->Advance(base::TimeDelta::FromDays(-1));
  base::Time out_network_time;
  EXPECT_FALSE(tracker_->GetNetworkTime(&out_network_time, NULL));
}

TEST_F(NetworkTimeTrackerTest, SuspendAndResume) {
  // Simulate the wall clock advancing while the tick clock stands still, as
  // would happen in a suspend+resume cycle.
  UpdateNetworkTime(clock_->Now(), resolution_, latency_,
                    tick_clock_->NowTicks());
  clock_->Advance(base::TimeDelta::FromHours(1));
  base::Time out_network_time;
  EXPECT_FALSE(tracker_->GetNetworkTime(&out_network_time, NULL));
}

TEST_F(NetworkTimeTrackerTest, Serialize) {
  // Test that we can serialize and deserialize state and get consistent
  // results.
  base::Time in_network_time = clock_->Now() - base::TimeDelta::FromDays(90);
  UpdateNetworkTime(in_network_time - latency_ / 2, resolution_, latency_,
                    tick_clock_->NowTicks());
  base::Time out_network_time;
  base::TimeDelta out_uncertainty;
  EXPECT_TRUE(tracker_->GetNetworkTime(&out_network_time, &out_uncertainty));
  EXPECT_EQ(in_network_time, out_network_time);
  EXPECT_EQ(resolution_ + latency_ + adjustment_, out_uncertainty);

  // 6 days is just under the threshold for discarding data.
  base::TimeDelta delta = base::TimeDelta::FromDays(6);
  AdvanceBoth(delta);
  Reset();
  EXPECT_TRUE(tracker_->GetNetworkTime(&out_network_time, &out_uncertainty));
  EXPECT_EQ(in_network_time + delta, out_network_time);
  EXPECT_EQ(resolution_ + latency_ + adjustment_, out_uncertainty);
}

TEST_F(NetworkTimeTrackerTest, DeserializeOldFormat) {
  // Test that deserializing old data (which do not record the uncertainty and
  // tick clock) causes the serialized data to be ignored.
  base::Time in_network_time = clock_->Now() - base::TimeDelta::FromDays(90);
  UpdateNetworkTime(in_network_time - latency_ / 2, resolution_, latency_,
                    tick_clock_->NowTicks());

  base::Time out_network_time;
  EXPECT_TRUE(tracker_->GetNetworkTime(&out_network_time, NULL));
  double local, network;
  const base::DictionaryValue* saved_prefs =
      pref_service_.GetDictionary(prefs::kNetworkTimeMapping);
  saved_prefs->GetDouble("local", &local);
  saved_prefs->GetDouble("network", &network);
  base::DictionaryValue prefs;
  prefs.SetDouble("local", local);
  prefs.SetDouble("network", network);
  pref_service_.Set(prefs::kNetworkTimeMapping, prefs);
  Reset();
  EXPECT_FALSE(tracker_->GetNetworkTime(&out_network_time, NULL));
}

TEST_F(NetworkTimeTrackerTest, SerializeWithLongDelay) {
  // Test that if the serialized data are more than a week old, they are
  // discarded.
  base::Time in_network_time = clock_->Now() - base::TimeDelta::FromDays(90);
  UpdateNetworkTime(in_network_time - latency_ / 2, resolution_, latency_,
                    tick_clock_->NowTicks());
  base::Time out_network_time;
  EXPECT_TRUE(tracker_->GetNetworkTime(&out_network_time, NULL));
  AdvanceBoth(base::TimeDelta::FromDays(8));
  Reset();
  EXPECT_FALSE(tracker_->GetNetworkTime(&out_network_time, NULL));
}

TEST_F(NetworkTimeTrackerTest, SerializeWithTickClockAdvance) {
  // Test that serialized data are discarded if the wall clock and tick clock
  // have not advanced consistently since data were serialized.
  base::Time in_network_time = clock_->Now() - base::TimeDelta::FromDays(90);
  UpdateNetworkTime(in_network_time - latency_ / 2, resolution_, latency_,
                    tick_clock_->NowTicks());
  base::Time out_network_time;
  EXPECT_TRUE(tracker_->GetNetworkTime(&out_network_time, NULL));
  tick_clock_->Advance(base::TimeDelta::FromDays(1));
  Reset();
  EXPECT_FALSE(tracker_->GetNetworkTime(&out_network_time, NULL));
}

TEST_F(NetworkTimeTrackerTest, SerializeWithWallClockAdvance) {
  // Test that serialized data are discarded if the wall clock and tick clock
  // have not advanced consistently since data were serialized.
  base::Time in_network_time = clock_->Now() - base::TimeDelta::FromDays(90);
  UpdateNetworkTime(in_network_time - latency_ / 2, resolution_, latency_,
                    tick_clock_->NowTicks());

  base::Time out_network_time;
  EXPECT_TRUE(tracker_->GetNetworkTime(&out_network_time, NULL));
  clock_->Advance(base::TimeDelta::FromDays(1));
  Reset();
  EXPECT_FALSE(tracker_->GetNetworkTime(&out_network_time, NULL));
}

}  // namespace network_time
