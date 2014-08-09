// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/audio_stream_monitor.h"

#include <map>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/scoped_ptr.h"
#include "base/test/simple_test_tick_clock.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/invalidate_type.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "media/audio/audio_power_monitor.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::InvokeWithoutArgs;

namespace {

const int kStreamId = 3;
const int kAnotherStreamId = 6;

// Used to confirm audio indicator state changes occur at the correct times.
class MockWebContentsDelegate : public content::WebContentsDelegate {
 public:
  MOCK_METHOD2(NavigationStateChanged,
               void(const content::WebContents* source,
                    content::InvalidateTypes changed_flags));
};

}  // namespace

class AudioStreamMonitorTest : public testing::Test {
 public:
  AudioStreamMonitorTest() {
    // Start |clock_| at non-zero.
    clock_.Advance(base::TimeDelta::FromSeconds(1000000));

    // Create a WebContents instance and set it to use our mock delegate.
    web_contents_.reset(content::WebContents::Create(
        content::WebContents::CreateParams(&profile_, NULL)));
    web_contents_->SetDelegate(&mock_web_contents_delegate_);

    // Create an AudioStreamMonitor instance whose lifecycle is tied to that of
    // |web_contents_|, and override its clock with the test clock.
    AudioStreamMonitor::CreateForWebContents(web_contents_.get());
    CHECK(audio_stream_monitor());
    const_cast<base::TickClock*&>(audio_stream_monitor()->clock_) = &clock_;
  }

  AudioStreamMonitor* audio_stream_monitor() {
    return AudioStreamMonitor::FromWebContents(web_contents_.get());
  }

  base::TimeTicks GetTestClockTime() { return clock_.NowTicks(); }

  void AdvanceClock(const base::TimeDelta& delta) { clock_.Advance(delta); }

  AudioStreamMonitor::ReadPowerAndClipCallback CreatePollCallback(
      int stream_id) {
    return base::Bind(
        &AudioStreamMonitorTest::ReadPower, base::Unretained(this), stream_id);
  }

  void SetStreamPower(int stream_id, float power) {
    current_power_[stream_id] = power;
  }

  void SimulatePollTimerFired() { audio_stream_monitor()->Poll(); }

  void SimulateOffTimerFired() { audio_stream_monitor()->MaybeToggle(); }

  void ExpectIsPolling(int stream_id, bool is_polling) {
    AudioStreamMonitor* const monitor = audio_stream_monitor();
    EXPECT_EQ(is_polling,
              monitor->poll_callbacks_.find(stream_id) !=
                  monitor->poll_callbacks_.end());
    EXPECT_EQ(!monitor->poll_callbacks_.empty(),
              monitor->poll_timer_.IsRunning());
  }

  void ExpectTabWasRecentlyAudible(bool was_audible,
                                   const base::TimeTicks& last_blurt_time) {
    AudioStreamMonitor* const monitor = audio_stream_monitor();
    EXPECT_EQ(was_audible, monitor->was_recently_audible_);
    EXPECT_EQ(last_blurt_time, monitor->last_blurt_time_);
    EXPECT_EQ(monitor->was_recently_audible_, monitor->off_timer_.IsRunning());
  }

  void ExpectWebContentsWillBeNotifiedOnce(bool should_be_audible) {
    EXPECT_CALL(mock_web_contents_delegate_,
                NavigationStateChanged(web_contents_.get(),
                                       content::INVALIDATE_TYPE_TAB))
        .WillOnce(InvokeWithoutArgs(
            this,
            should_be_audible
                ? &AudioStreamMonitorTest::ExpectIsNotifyingForToggleOn
                : &AudioStreamMonitorTest::ExpectIsNotifyingForToggleOff))
        .RetiresOnSaturation();
  }

  static base::TimeDelta one_polling_interval() {
    return base::TimeDelta::FromSeconds(1) /
           AudioStreamMonitor::kPowerMeasurementsPerSecond;
  }

  static base::TimeDelta holding_period() {
    return base::TimeDelta::FromMilliseconds(
        AudioStreamMonitor::kHoldOnMilliseconds);
  }

 private:
  std::pair<float, bool> ReadPower(int stream_id) {
    return std::make_pair(current_power_[stream_id], false);
  }

  void ExpectIsNotifyingForToggleOn() {
    EXPECT_TRUE(audio_stream_monitor()->WasRecentlyAudible());
  }

  void ExpectIsNotifyingForToggleOff() {
    EXPECT_FALSE(audio_stream_monitor()->WasRecentlyAudible());
  }

  content::TestBrowserThreadBundle browser_thread_bundle_;
  TestingProfile profile_;
  MockWebContentsDelegate mock_web_contents_delegate_;
  base::SimpleTestTickClock clock_;
  std::map<int, float> current_power_;
  scoped_ptr<content::WebContents> web_contents_;

  DISALLOW_COPY_AND_ASSIGN(AudioStreamMonitorTest);
};

// Tests that AudioStreamMonitor is polling while it has a
// ReadPowerAndClipCallback, and is not polling at other times.
TEST_F(AudioStreamMonitorTest, PollsWhenProvidedACallback) {
  EXPECT_FALSE(audio_stream_monitor()->WasRecentlyAudible());
  ExpectIsPolling(kStreamId, false);

  audio_stream_monitor()->StartMonitoringStream(kStreamId,
                                                CreatePollCallback(kStreamId));
  EXPECT_FALSE(audio_stream_monitor()->WasRecentlyAudible());
  ExpectIsPolling(kStreamId, true);

  audio_stream_monitor()->StopMonitoringStream(kStreamId);
  EXPECT_FALSE(audio_stream_monitor()->WasRecentlyAudible());
  ExpectIsPolling(kStreamId, false);
}

// Tests that AudioStreamMonitor debounces the power level readings it's taking,
// which could be fluctuating rapidly between the audible versus silence
// threshold.  See comments in audio_stream_monitor.h for expected behavior.
TEST_F(AudioStreamMonitorTest,
       ImpulsesKeepIndicatorOnUntilHoldingPeriodHasPassed) {
  audio_stream_monitor()->StartMonitoringStream(kStreamId,
                                                CreatePollCallback(kStreamId));

  // Expect WebContents will get one call form AudioStreamMonitor to toggle the
  // indicator on upon the very first poll.
  ExpectWebContentsWillBeNotifiedOnce(true);

  // Loop, each time testing a slightly longer period of polled silence.  The
  // indicator should remain on throughout.
  int num_silence_polls = 0;
  base::TimeTicks last_blurt_time;
  do {
    // Poll an audible signal, and expect tab indicator state is on.
    SetStreamPower(kStreamId, media::AudioPowerMonitor::max_power());
    last_blurt_time = GetTestClockTime();
    SimulatePollTimerFired();
    ExpectTabWasRecentlyAudible(true, last_blurt_time);
    AdvanceClock(one_polling_interval());

    // Poll a silent signal repeatedly, ensuring that the indicator is being
    // held on during the holding period.
    SetStreamPower(kStreamId, media::AudioPowerMonitor::zero_power());
    for (int i = 0; i < num_silence_polls; ++i) {
      SimulatePollTimerFired();
      ExpectTabWasRecentlyAudible(true, last_blurt_time);
      // Note: Redundant off timer firings should not have any effect.
      SimulateOffTimerFired();
      ExpectTabWasRecentlyAudible(true, last_blurt_time);
      AdvanceClock(one_polling_interval());
    }

    ++num_silence_polls;
  } while (GetTestClockTime() < last_blurt_time + holding_period());

  // At this point, the clock has just advanced to beyond the holding period, so
  // the next firing of the off timer should turn off the tab indicator.  Also,
  // make sure it stays off for several cycles thereafter.
  ExpectWebContentsWillBeNotifiedOnce(false);
  for (int i = 0; i < 10; ++i) {
    SimulateOffTimerFired();
    ExpectTabWasRecentlyAudible(false, last_blurt_time);
    AdvanceClock(one_polling_interval());
  }
}

// Tests that the AudioStreamMonitor correctly processes the blurts from two
// different streams in the same tab.
TEST_F(AudioStreamMonitorTest, HandlesMultipleStreamsBlurting) {
  audio_stream_monitor()->StartMonitoringStream(kStreamId,
                                                CreatePollCallback(kStreamId));
  audio_stream_monitor()->StartMonitoringStream(
      kAnotherStreamId,
      CreatePollCallback(kAnotherStreamId));

  base::TimeTicks last_blurt_time;
  ExpectTabWasRecentlyAudible(false, last_blurt_time);

  // Measure audible sound from the first stream and silence from the second.
  // The indicator turns on (i.e., tab was recently audible).
  ExpectWebContentsWillBeNotifiedOnce(true);
  SetStreamPower(kStreamId, media::AudioPowerMonitor::max_power());
  SetStreamPower(kAnotherStreamId, media::AudioPowerMonitor::zero_power());
  last_blurt_time = GetTestClockTime();
  SimulatePollTimerFired();
  ExpectTabWasRecentlyAudible(true, last_blurt_time);

  // Halfway through the holding period, the second stream joins in.  The
  // indicator stays on.
  AdvanceClock(holding_period() / 2);
  SimulateOffTimerFired();
  SetStreamPower(kAnotherStreamId, media::AudioPowerMonitor::max_power());
  last_blurt_time = GetTestClockTime();
  SimulatePollTimerFired();  // Restarts holding period.
  ExpectTabWasRecentlyAudible(true, last_blurt_time);

  // Now, measure silence from both streams.  After an entire holding period
  // has passed (since the second stream joined in), the indicator should turn
  // off.
  ExpectWebContentsWillBeNotifiedOnce(false);
  AdvanceClock(holding_period());
  SimulateOffTimerFired();
  SetStreamPower(kStreamId, media::AudioPowerMonitor::zero_power());
  SetStreamPower(kAnotherStreamId, media::AudioPowerMonitor::zero_power());
  SimulatePollTimerFired();
  ExpectTabWasRecentlyAudible(false, last_blurt_time);

  // Now, measure silence from the first stream and audible sound from the
  // second.  The indicator turns back on.
  ExpectWebContentsWillBeNotifiedOnce(true);
  SetStreamPower(kAnotherStreamId, media::AudioPowerMonitor::max_power());
  last_blurt_time = GetTestClockTime();
  SimulatePollTimerFired();
  ExpectTabWasRecentlyAudible(true, last_blurt_time);

  // From here onwards, both streams are silent.  Halfway through the holding
  // period, the indicator should not have changed.
  SetStreamPower(kAnotherStreamId, media::AudioPowerMonitor::zero_power());
  AdvanceClock(holding_period() / 2);
  SimulatePollTimerFired();
  SimulateOffTimerFired();
  ExpectTabWasRecentlyAudible(true, last_blurt_time);

  // Just past the holding period, the indicator should be turned off.
  ExpectWebContentsWillBeNotifiedOnce(false);
  AdvanceClock(holding_period() - (GetTestClockTime() - last_blurt_time));
  SimulateOffTimerFired();
  ExpectTabWasRecentlyAudible(false, last_blurt_time);

  // Polling should not turn the indicator back while both streams are remaining
  // silent.
  for (int i = 0; i < 100; ++i) {
    AdvanceClock(one_polling_interval());
    SimulatePollTimerFired();
    ExpectTabWasRecentlyAudible(false, last_blurt_time);
  }
}
