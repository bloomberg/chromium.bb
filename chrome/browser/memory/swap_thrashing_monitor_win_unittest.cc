// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/memory/swap_thrashing_monitor_win.h"

#include "base/memory/ptr_util.h"
#include "base/test/scoped_task_environment.h"
#include "base/timer/mock_timer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace memory {

namespace {

using SwapThrashingLevel = SwapThrashingMonitorWin::SwapThrashingLevel;

class MockPageFaultAverageRate;

class TestSwapThrashingMonitorWin : public SwapThrashingMonitorWin {
 public:
  using PageFaultAverageRate = SwapThrashingMonitorWin::PageFaultAverageRate;
  using PageFaultObservation = SwapThrashingMonitorWin::PageFaultObservation;

  TestSwapThrashingMonitorWin()
      : SwapThrashingMonitorWin::SwapThrashingMonitorWin() {}
  ~TestSwapThrashingMonitorWin() override {}

  bool RecordHardFaultCountForChromeProcesses() override {
    PageFaultObservation observation = {0, base::TimeTicks::Now()};

    if (swap_thrashing_escalation_window_)
      swap_thrashing_escalation_window_->OnObservation(observation);
    if (swap_thrashing_cooldown_window_)
      swap_thrashing_cooldown_window_->OnObservation(observation);

    return true;
  }

  // Override these functions to make this detector use mock detection windows.
  void ResetEscalationWindow(base::TimeDelta window_length) override;
  void ResetCooldownWindow(base::TimeDelta window_length) override;

  MockPageFaultAverageRate* escalation_window() {
    EXPECT_NE(nullptr, escalation_window_);
    return escalation_window_;
  }
  MockPageFaultAverageRate* cooldown_window() {
    EXPECT_NE(nullptr, cooldown_window_);
    return cooldown_window_;
  }

  // Returns a page-fault value that will set the hard page-fault rate during
  // the observation to a value that will cause a state change.
  uint64_t ComputeHighThrashingRate(base::TimeTicks window_beginning,
                                    base::TimeTicks next_timestamp);

 private:
  MockPageFaultAverageRate* escalation_window_;
  MockPageFaultAverageRate* cooldown_window_;
};

// A mock for the PageFaultAverageRate class.
//
// Used to induce state transitions in TestSwapThrashingMonitorWin.
class MockPageFaultAverageRate
    : public TestSwapThrashingMonitorWin::PageFaultAverageRate {
 public:
  using PageFaultObservation =
      TestSwapThrashingMonitorWin::PageFaultObservation;

  MockPageFaultAverageRate()
      : TestSwapThrashingMonitorWin::PageFaultAverageRate(
            base::TimeDelta::FromMilliseconds(1000)),
        observation_override_({0, base::TimeTicks::Now()}),
        invalidated_(false) {}
  ~MockPageFaultAverageRate() override {}

  // Override this function so we can use the mocked expectations rather than
  // the real ones.
  void OnObservation(PageFaultObservation observation) override {
    TestSwapThrashingMonitorWin::PageFaultAverageRate::OnObservation(
        observation_override_);
  }

  void Invalidate() { invalidated_ = true; }

  base::Optional<double> AveragePageFaultRate() const override {
    if (invalidated_)
      return base::nullopt;
    return TestSwapThrashingMonitorWin::PageFaultAverageRate::
        AveragePageFaultRate();
  }

  // Override the next observations that this window will receive.
  void OverrideNextObservation(PageFaultObservation observation_override) {
    observation_override_ = observation_override;
  }

  void AddObservation(PageFaultObservation observation) {
    OverrideNextObservation(observation);
    OnObservation({0, base::TimeTicks::Now()});
  }

  base::TimeDelta window_length() const { return window_length_; }

  base::TimeTicks window_beginning() {
    return observations_.begin()->timestamp;
  }

  // Compute the timestamp that correspond to the end of this window.
  base::TimeTicks GetEndOfWindowTimestamp() {
    return window_beginning() + window_length();
  }

  const base::circular_deque<PageFaultObservation>& observations() {
    return observations_;
  }

 private:
  PageFaultObservation observation_override_;
  bool invalidated_;
};

void TestSwapThrashingMonitorWin::ResetEscalationWindow(
    base::TimeDelta window_length) {
  std::unique_ptr<MockPageFaultAverageRate> escalation_window =
      base::MakeUnique<MockPageFaultAverageRate>();
  escalation_window_ = escalation_window.get();
  swap_thrashing_escalation_window_.reset(escalation_window.release());
}

void TestSwapThrashingMonitorWin::ResetCooldownWindow(
    base::TimeDelta window_length) {
  std::unique_ptr<MockPageFaultAverageRate> cooldown_window =
      base::MakeUnique<MockPageFaultAverageRate>();
  cooldown_window_ = cooldown_window.get();
  swap_thrashing_cooldown_window_.reset(cooldown_window.release());
}

uint64_t TestSwapThrashingMonitorWin::ComputeHighThrashingRate(
    base::TimeTicks window_beginning,
    base::TimeTicks next_timestamp) {
  return SwapThrashingMonitorWin::kPageFaultEscalationThreshold *
         (next_timestamp - window_beginning).InSeconds();
}

}  // namespace

class SwapThrashingMonitorWinTest : public testing::Test {
 public:
  using PageFaultObservation =
      TestSwapThrashingMonitorWin::PageFaultObservation;

  void SetUp() override {
    mock_escalation_window_ = nullptr;
    mock_cooldown_window_ = nullptr;
  }

  void TearDown() override {}

  void ResetObservationWindows(bool enable_escalation_window,
                               bool enable_cooldown_window) {
    // Reset the windows to turn it into mock ones.
    monitor_.ResetEscalationWindow(base::TimeDelta());
    mock_escalation_window_ = monitor_.escalation_window();
    monitor_.ResetCooldownWindow(base::TimeDelta());
    mock_cooldown_window_ = monitor_.cooldown_window();
    if (enable_escalation_window) {
      mock_escalation_window_->AddObservation({0, base::TimeTicks::Now()});
    } else {
      mock_escalation_window_->Invalidate();
    }
    if (enable_cooldown_window) {
      mock_cooldown_window_->AddObservation({0, base::TimeTicks::Now()});
    } else {
      mock_cooldown_window_->Invalidate();
    }
  }

 protected:
  TestSwapThrashingMonitorWin monitor_;

  MockPageFaultAverageRate* mock_escalation_window_;
  MockPageFaultAverageRate* mock_cooldown_window_;
};

TEST_F(SwapThrashingMonitorWinTest, StateTransition) {
  // This state test the transitions from the SWAP_THRASHING_LEVEL_NONE state
  // to the SWAP_THRASHING_LEVEL_CONFIRMED one and then ensure that the cooldown
  // mechanism works.

  // Only enable the detection window
  ResetObservationWindows(true, false);

  // We expect the system to initially be in the SWAP_THRASHING_LEVEL_NONE
  // state.
  EXPECT_EQ(SwapThrashingLevel::SWAP_THRASHING_LEVEL_NONE,
            monitor_.SampleAndCalculateSwapThrashingLevel());

  // Override the next observation with a timestamp that is inferior to the
  // window length.
  PageFaultObservation next_observation = {
      0, mock_escalation_window_->GetEndOfWindowTimestamp() -
             base::TimeDelta::FromMilliseconds(1)};
  mock_escalation_window_->OverrideNextObservation(next_observation);
  // There should be no state transition as the window hasn't expired yet.
  EXPECT_EQ(SwapThrashingLevel::SWAP_THRASHING_LEVEL_NONE,
            monitor_.SampleAndCalculateSwapThrashingLevel());

  // Use a timestamp equal to the window length.
  next_observation.timestamp =
      mock_escalation_window_->GetEndOfWindowTimestamp();
  // Set the thrashing level to a rate that is inferior to the state change
  // threshold.
  next_observation.page_fault_count =
      monitor_.ComputeHighThrashingRate(
          mock_escalation_window_->window_beginning(),
          next_observation.timestamp) -
      1;
  mock_escalation_window_->OverrideNextObservation(next_observation);
  EXPECT_EQ(SwapThrashingLevel::SWAP_THRASHING_LEVEL_NONE,
            monitor_.SampleAndCalculateSwapThrashingLevel());

  // Set the thrashing rate to the threshold, this should cause a state change.
  next_observation.page_fault_count++;
  mock_escalation_window_->OverrideNextObservation(next_observation);
  EXPECT_EQ(SwapThrashingLevel::SWAP_THRASHING_LEVEL_SUSPECTED,
            monitor_.SampleAndCalculateSwapThrashingLevel());

  // Reset the detection window as it has been invalidated by the state change,
  // invalidate the cooldown window to make sure the system doesn't switch to a
  // lower state.
  ResetObservationWindows(true, false);

  // Use a timestamp just before the end of the window.
  next_observation.timestamp =
      mock_escalation_window_->GetEndOfWindowTimestamp() -
      base::TimeDelta::FromMilliseconds(1);
  next_observation.page_fault_count = 0;
  mock_escalation_window_->OverrideNextObservation(next_observation);
  EXPECT_EQ(SwapThrashingLevel::SWAP_THRASHING_LEVEL_SUSPECTED,
            monitor_.SampleAndCalculateSwapThrashingLevel());

  // Make the window expire but keep the page-fault rate low, this shouldn't
  // cause a state change.
  next_observation.timestamp =
      mock_escalation_window_->GetEndOfWindowTimestamp();
  mock_escalation_window_->OverrideNextObservation(next_observation);
  EXPECT_EQ(SwapThrashingLevel::SWAP_THRASHING_LEVEL_SUSPECTED,
            monitor_.SampleAndCalculateSwapThrashingLevel());

  ResetObservationWindows(true, false);

  next_observation.timestamp =
      mock_escalation_window_->GetEndOfWindowTimestamp();

  // Increase the page-fault rate to induce a change to the confirmed state.
  next_observation.page_fault_count = monitor_.ComputeHighThrashingRate(
      mock_escalation_window_->window_beginning(), next_observation.timestamp);
  mock_escalation_window_->OverrideNextObservation(next_observation);
  EXPECT_EQ(SwapThrashingLevel::SWAP_THRASHING_LEVEL_CONFIRMED,
            monitor_.SampleAndCalculateSwapThrashingLevel());

  // Reset the detection windows as they have been invalidated by the state
  // change.
  ResetObservationWindows(false, true);

  // Keep the hard page-fault rate high, the state shouldn't change at the end
  // of the cooldown window.
  next_observation.timestamp = mock_cooldown_window_->GetEndOfWindowTimestamp();
  next_observation.page_fault_count = monitor_.ComputeHighThrashingRate(
      mock_cooldown_window_->window_beginning(), next_observation.timestamp);
  mock_cooldown_window_->OverrideNextObservation(next_observation);
  EXPECT_EQ(SwapThrashingLevel::SWAP_THRASHING_LEVEL_CONFIRMED,
            monitor_.SampleAndCalculateSwapThrashingLevel());

  // Set the hard page-fault rate to 0, this will induce a transition to a lower
  // state.
  ResetObservationWindows(false, true);
  next_observation.timestamp = mock_cooldown_window_->GetEndOfWindowTimestamp();
  next_observation.page_fault_count = 0;
  mock_cooldown_window_->OverrideNextObservation(next_observation);
  EXPECT_EQ(SwapThrashingLevel::SWAP_THRASHING_LEVEL_SUSPECTED,
            monitor_.SampleAndCalculateSwapThrashingLevel());

  // And finally return to the NONE state.
  ResetObservationWindows(false, true);
  next_observation.timestamp = mock_cooldown_window_->GetEndOfWindowTimestamp();
  mock_cooldown_window_->OverrideNextObservation(next_observation);
  EXPECT_EQ(SwapThrashingLevel::SWAP_THRASHING_LEVEL_NONE,
            monitor_.SampleAndCalculateSwapThrashingLevel());
}

TEST_F(SwapThrashingMonitorWinTest, PageFaultAverageRateTests) {
  constexpr base::TimeDelta kObservationWindowLength =
      base::TimeDelta::FromSeconds(8);
  MockPageFaultAverageRate escalation_window;

  base::TimeTicks reference_tick = base::TimeTicks::Now();
  PageFaultObservation first_observation = {0, reference_tick};
  escalation_window.Reset(kObservationWindowLength);
  escalation_window.OnObservation(first_observation);

  EXPECT_EQ(1U, escalation_window.observations().size());

  // Add an observation with a timestamp inferior to the window length.
  PageFaultObservation second_observation = {
      400, reference_tick + base::TimeDelta::FromSeconds(2)};

  escalation_window.AddObservation(second_observation);
  // It shouldn't be able to compute the average as the window hasn't expired
  // yet.
  base::Optional<double> average = escalation_window.AveragePageFaultRate();
  EXPECT_EQ(base::nullopt, average);

  EXPECT_EQ(2U, escalation_window.observations().size());

  // Add another observation that correspond to the end of the observation
  // window.
  PageFaultObservation third_observation = {
      800, reference_tick + kObservationWindowLength};
  escalation_window.AddObservation(third_observation);
  average = escalation_window.AveragePageFaultRate();
  EXPECT_NE(base::nullopt, average);
  double expected_rate =
      (third_observation.page_fault_count -
       first_observation.page_fault_count) /
      (third_observation.timestamp - first_observation.timestamp).InSecondsF();
  EXPECT_NEAR(expected_rate, average.value(), 0.01);

  EXPECT_EQ(3U, escalation_window.observations().size());

  // Adding an observation that should force the removal of the first one.
  PageFaultObservation fourth_observation = {
      1200, third_observation.timestamp +
                (second_observation.timestamp - first_observation.timestamp)};
  escalation_window.AddObservation(fourth_observation);
  average = escalation_window.AveragePageFaultRate();
  EXPECT_NE(base::nullopt, average);
  expected_rate = (fourth_observation.page_fault_count -
                   second_observation.page_fault_count) /
                  (fourth_observation.timestamp - second_observation.timestamp)
                      .InSecondsF();
  EXPECT_NEAR(expected_rate, average.value(), 0.01);
  EXPECT_EQ(3U, escalation_window.observations().size());

  // And finally add an observation that should force the removal of all the
  // previous ones expect the last 2.
  PageFaultObservation fifth_observation = {
      fourth_observation.page_fault_count,
      fourth_observation.timestamp + kObservationWindowLength * 2};
  escalation_window.AddObservation(fifth_observation);
  average = escalation_window.AveragePageFaultRate();
  EXPECT_NE(base::nullopt, average);
  expected_rate =
      (fifth_observation.page_fault_count -
       fourth_observation.page_fault_count) /
      (fifth_observation.timestamp - fourth_observation.timestamp).InSecondsF();
  EXPECT_NEAR(expected_rate, average.value(), 0.01);
  EXPECT_EQ(2U, escalation_window.observations().size());
}

}  // namespace memory
