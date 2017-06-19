// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/run_loop.h"
#include "base/test/test_message_loop.h"
#include "media/base/mock_media_log.h"
#include "media/base/watch_time_keys.h"
#include "media/blink/watch_time_reporter.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

constexpr gfx::Size kSizeJustRight = gfx::Size(201, 201);

using blink::WebMediaPlayer;

#define EXPECT_WATCH_TIME(key, value)                                    \
  do {                                                                   \
    EXPECT_CALL(media_log_,                                              \
                OnWatchTimeUpdate(has_video_ ? kWatchTimeAudioVideo##key \
                                             : kWatchTimeAudio##key,     \
                                  value))                                \
        .RetiresOnSaturation();                                          \
  } while (0)

#define EXPECT_WATCH_TIME_IF_VIDEO(key, value) \
  do {                                         \
    if (!has_video_)                           \
      break;                                   \
    EXPECT_WATCH_TIME(key, value);             \
  } while (0)

#define EXPECT_BACKGROUND_WATCH_TIME(key, value)                               \
  do {                                                                         \
    DCHECK(has_video_);                                                        \
    EXPECT_CALL(media_log_,                                                    \
                OnWatchTimeUpdate(kWatchTimeAudioVideoBackground##key, value)) \
        .RetiresOnSaturation();                                                \
  } while (0)

#define EXPECT_WATCH_TIME_FINALIZED() \
  EXPECT_CALL(media_log_, OnWatchTimeFinalized()).RetiresOnSaturation();

#define EXPECT_POWER_WATCH_TIME_FINALIZED() \
  EXPECT_CALL(media_log_, OnPowerWatchTimeFinalized()).RetiresOnSaturation();

#define EXPECT_CONTROLS_WATCH_TIME_FINALIZED() \
  EXPECT_CALL(media_log_, OnControlsWatchTimeFinalized()).RetiresOnSaturation();

#define EXPECT_DISPLAY_WATCH_TIME_FINALIZED() \
  EXPECT_CALL(media_log_, OnDisplayWatchTimeFinalized()).RetiresOnSaturation();

class WatchTimeReporterTest : public testing::TestWithParam<bool> {
 public:
  WatchTimeReporterTest() : has_video_(GetParam()) {}
  ~WatchTimeReporterTest() override {}

 protected:
  class WatchTimeLogMonitor : public MediaLog {
   public:
    WatchTimeLogMonitor() = default;

    void AddEvent(std::unique_ptr<MediaLogEvent> event) override {
      ASSERT_EQ(event->type, MediaLogEvent::Type::WATCH_TIME_UPDATE);

      for (base::DictionaryValue::Iterator it(event->params); !it.IsAtEnd();
           it.Advance()) {
        bool finalize;
        if (it.value().GetAsBoolean(&finalize)) {
          if (it.key() == kWatchTimeFinalize)
            OnWatchTimeFinalized();
          else if (it.key() == kWatchTimeFinalizePower)
            OnPowerWatchTimeFinalized();
          else if (it.key() == kWatchTimeFinalizeControls)
            OnControlsWatchTimeFinalized();
          else if (it.key() == kWatchTimeFinalizeDisplay)
            OnDisplayWatchTimeFinalized();
          else
            NOTREACHED();
          continue;
        }

        int underflow_count;
        if (it.value().GetAsInteger(&underflow_count)) {
          OnUnderflowUpdate(underflow_count);
          continue;
        }

        double in_seconds;
        ASSERT_TRUE(it.value().GetAsDouble(&in_seconds));
        OnWatchTimeUpdate(it.key(), base::TimeDelta::FromSecondsD(in_seconds));
      }
    }

    MOCK_METHOD0(OnWatchTimeFinalized, void(void));
    MOCK_METHOD0(OnPowerWatchTimeFinalized, void(void));
    MOCK_METHOD0(OnControlsWatchTimeFinalized, void(void));
    MOCK_METHOD0(OnDisplayWatchTimeFinalized, void(void));
    MOCK_METHOD2(OnWatchTimeUpdate, void(const std::string&, base::TimeDelta));
    MOCK_METHOD1(OnUnderflowUpdate, void(int));

   protected:
    ~WatchTimeLogMonitor() override = default;

   private:
    DISALLOW_COPY_AND_ASSIGN(WatchTimeLogMonitor);
  };

  void Initialize(bool has_audio,
                  bool is_mse,
                  bool is_encrypted,
                  const gfx::Size& initial_video_size) {
    if (wtr_ && IsMonitoring())
      EXPECT_WATCH_TIME_FINALIZED();

    wtr_.reset(new WatchTimeReporter(
        has_audio, has_video_, is_mse, is_encrypted, false, &media_log_,
        initial_video_size,
        base::Bind(&WatchTimeReporterTest::GetCurrentMediaTime,
                   base::Unretained(this))));

    // Setup the reporting interval to be immediate to avoid spinning real time
    // within the unit test.
    wtr_->reporting_interval_ = base::TimeDelta();
    if (wtr_->background_reporter_)
      wtr_->background_reporter_->reporting_interval_ = base::TimeDelta();
  }

  void CycleReportingTimer() {
    base::RunLoop run_loop;
    message_loop_.task_runner()->PostTask(FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }

  bool IsMonitoring() const { return wtr_->reporting_timer_.IsRunning(); }

  bool IsBackgroundMonitoring() const {
    return wtr_->background_reporter_->reporting_timer_.IsRunning();
  }

  // We call directly into the reporter for this instead of using an actual
  // PowerMonitorTestSource since that results in a posted tasks which interfere
  // with our ability to test the timer.
  void SetOnBatteryPower(bool on_battery_power) {
    wtr_->is_on_battery_power_ = on_battery_power;
  }

  void OnPowerStateChange(bool on_battery_power) {
    wtr_->OnPowerStateChange(on_battery_power);
    if (wtr_->background_reporter_)
      wtr_->background_reporter_->OnPowerStateChange(on_battery_power);
  }

  void OnNativeControlsEnabled(bool enabled) {
    enabled ? wtr_->OnNativeControlsEnabled()
            : wtr_->OnNativeControlsDisabled();
  }

  void OnDisplayTypeChanged(WebMediaPlayer::DisplayType display_type) {
    wtr_->OnDisplayTypeChanged(display_type);
  }

  enum {
    // After |test_callback_func| is executed, should watch time continue to
    // accumulate?
    kAccumulationContinuesAfterTest = 1,

    // |test_callback_func| for hysteresis tests enters and exits finalize mode
    // for watch time, not all exits require a new current time update.
    kFinalizeExitDoesNotRequireCurrentTime = 2,

    // During finalize the watch time should not continue on the starting power
    // metric. By default this means the AC metric will be finalized, but if
    // used with |kStartOnBattery| it will be the battery metric.
    kFinalizePowerWatchTime = 4,

    // During finalize the power watch time should continue on the metric
    // opposite the starting metric (by default it's AC, it's battery if
    // |kStartOnBattery| is specified.
    kTransitionPowerWatchTime = 8,

    // Indicates that power watch time should be reported to the battery metric.
    kStartOnBattery = 16,

    // Indicates an extra start event may be generated during test execution.
    kFinalizeInterleavedStartEvent = 32,

    // During finalize the watch time should not continue on the starting
    // controls metric. By default this means the NativeControsOff metric will
    // be finalized, but if used with |kStartWithNativeControls| it will be the
    // NativeControlsOn metric.
    kFinalizeControlsWatchTime = 64,

    // During finalize the controls watch time should continue on the metric
    // opposite the starting metric (by default it's non-native controls, it's
    // native controls if |kStartWithNativeControls| is specified.
    kTransitionControlsWatchTime = 128,

    // Indicates that controls watch time should be reported to the native
    // controls metric.
    kStartWithNativeControls = 256,

    // During finalize the watch time should not continue on the starting
    // display metric. By default this means the DisplayInline metric will be
    // finalized, but if used with |kStartWithDisplayFullscreen| it will be the
    // DisplayFullscreen metric.
    kFinalizeDisplayWatchTime = 1024,

    // During finalize the display watch time should continue on the metric
    // opposite the starting metric (by default it's inline, it's fullscreen if
    // |kStartWithDisplayFullscreen| is specified.
    kTransitionDisplayWatchTime = 2048,

    // Indicates that the watch time should be reporter to the fullscreen
    // display metric.
    kStartWithDisplayFullscreen = 4096,
  };

  template <int TestFlags = 0, typename HysteresisTestCallback>
  void RunHysteresisTest(HysteresisTestCallback test_callback_func) {
    Initialize(true, false, false, kSizeJustRight);

    // Disable background reporting for the hysteresis tests.
    wtr_->background_reporter_.reset();

    if (TestFlags & kStartWithNativeControls)
      OnNativeControlsEnabled(true);
    if (TestFlags & kStartWithDisplayFullscreen)
      OnDisplayTypeChanged(WebMediaPlayer::DisplayType::kFullscreen);

    // Setup all current time expectations first since they need to use the
    // InSequence macro for ease of use, but we don't want the watch time
    // expectations to be in sequence (or expectations would depend on sorted
    // order of histogram names).
    constexpr base::TimeDelta kWatchTime1 = base::TimeDelta::FromSeconds(10);
    constexpr base::TimeDelta kWatchTime2 = base::TimeDelta::FromSeconds(12);
    constexpr base::TimeDelta kWatchTime3 = base::TimeDelta::FromSeconds(15);
    constexpr base::TimeDelta kWatchTime4 = base::TimeDelta::FromSeconds(30);
    {
      testing::InSequence s;

      EXPECT_CALL(*this, GetCurrentMediaTime())
          .WillOnce(testing::Return(base::TimeDelta()))
          .WillOnce(testing::Return(kWatchTime1));

      // Setup conditions depending on if the test will not resume watch time
      // accumulation or not; i.e. the finalize criteria will not be undone
      // within the hysteresis time.
      if (TestFlags & kAccumulationContinuesAfterTest) {
        EXPECT_CALL(*this, GetCurrentMediaTime())
            .Times(TestFlags & (kFinalizeExitDoesNotRequireCurrentTime |
                                kFinalizePowerWatchTime |
                                kFinalizeControlsWatchTime |
                                kFinalizeDisplayWatchTime)
                       ? 1
                       : 2)
            .WillRepeatedly(testing::Return(kWatchTime2));
        EXPECT_CALL(*this, GetCurrentMediaTime())
            .WillOnce(testing::Return(kWatchTime3));
      } else {
        // Current time should be requested when entering the finalize state.
        EXPECT_CALL(*this, GetCurrentMediaTime())
            .Times(TestFlags & kFinalizeInterleavedStartEvent ? 2 : 1)
            .WillRepeatedly(testing::Return(kWatchTime2));
      }

      if (TestFlags & kTransitionPowerWatchTime) {
        EXPECT_CALL(*this, GetCurrentMediaTime())
            .WillOnce(testing::Return(kWatchTime4));
      }

      if (TestFlags & kTransitionControlsWatchTime) {
        EXPECT_CALL(*this, GetCurrentMediaTime())
            .WillOnce(testing::Return(kWatchTime4));
      }

      if (TestFlags & kTransitionDisplayWatchTime) {
        EXPECT_CALL(*this, GetCurrentMediaTime())
            .WillOnce(testing::Return(kWatchTime4));
      }
    }

    wtr_->OnPlaying();
    EXPECT_TRUE(IsMonitoring());
    if (TestFlags & kStartOnBattery)
      SetOnBatteryPower(true);
    else
      ASSERT_FALSE(wtr_->is_on_battery_power_);

    EXPECT_WATCH_TIME(All, kWatchTime1);
    EXPECT_WATCH_TIME(Src, kWatchTime1);
    if (TestFlags & kStartOnBattery)
      EXPECT_WATCH_TIME(Battery, kWatchTime1);
    else
      EXPECT_WATCH_TIME(Ac, kWatchTime1);
    if (TestFlags & kStartWithNativeControls)
      EXPECT_WATCH_TIME(NativeControlsOn, kWatchTime1);
    else
      EXPECT_WATCH_TIME(NativeControlsOff, kWatchTime1);
    if (TestFlags & kStartWithDisplayFullscreen)
      EXPECT_WATCH_TIME_IF_VIDEO(DisplayFullscreen, kWatchTime1);
    else
      EXPECT_WATCH_TIME_IF_VIDEO(DisplayInline, kWatchTime1);

    CycleReportingTimer();

    // Invoke the test.
    test_callback_func();

    const base::TimeDelta kExpectedWatchTime =
        TestFlags & kAccumulationContinuesAfterTest ? kWatchTime3 : kWatchTime2;

    EXPECT_WATCH_TIME(All, kExpectedWatchTime);
    EXPECT_WATCH_TIME(Src, kExpectedWatchTime);
    const base::TimeDelta kExpectedPowerWatchTime =
        TestFlags & kFinalizePowerWatchTime ? kWatchTime2 : kExpectedWatchTime;
    const base::TimeDelta kExpectedContolsWatchTime =
        TestFlags & kFinalizeControlsWatchTime ? kWatchTime2
                                               : kExpectedWatchTime;
    const base::TimeDelta kExpectedDisplayWatchTime =
        TestFlags & kFinalizeDisplayWatchTime ? kWatchTime2
                                              : kExpectedWatchTime;

    if (TestFlags & kStartOnBattery)
      EXPECT_WATCH_TIME(Battery, kExpectedPowerWatchTime);
    else
      EXPECT_WATCH_TIME(Ac, kExpectedPowerWatchTime);

    if (TestFlags & kStartWithNativeControls)
      EXPECT_WATCH_TIME(NativeControlsOn, kExpectedContolsWatchTime);
    else
      EXPECT_WATCH_TIME(NativeControlsOff, kExpectedContolsWatchTime);

    if (TestFlags & kStartWithDisplayFullscreen)
      EXPECT_WATCH_TIME_IF_VIDEO(DisplayFullscreen, kExpectedDisplayWatchTime);
    else
      EXPECT_WATCH_TIME_IF_VIDEO(DisplayInline, kExpectedDisplayWatchTime);

    // Special case when testing battery watch time.
    if (TestFlags & kTransitionPowerWatchTime) {
      ASSERT_TRUE(TestFlags & kAccumulationContinuesAfterTest)
          << "kTransitionPowerWatchTime tests must be done with "
             "kAccumulationContinuesAfterTest";

      EXPECT_POWER_WATCH_TIME_FINALIZED();
      CycleReportingTimer();

      // Run one last cycle that is long enough to trigger a new watch time
      // entry on the opposite of the current power watch time graph; i.e. if we
      // started on battery we'll now record one for ac and vice versa.
      EXPECT_WATCH_TIME(All, kWatchTime4);
      EXPECT_WATCH_TIME(Src, kWatchTime4);
      EXPECT_WATCH_TIME_IF_VIDEO(DisplayInline, kWatchTime4);
      EXPECT_WATCH_TIME(NativeControlsOff, kWatchTime4);
      if (TestFlags & kStartOnBattery)
        EXPECT_WATCH_TIME(Ac, kWatchTime4 - kWatchTime2);
      else
        EXPECT_WATCH_TIME(Battery, kWatchTime4 - kWatchTime2);
    } else if (TestFlags & kTransitionControlsWatchTime) {
      ASSERT_TRUE(TestFlags & kAccumulationContinuesAfterTest)
          << "kTransitionControlsWatchTime tests must be done with "
             "kAccumulationContinuesAfterTest";

      EXPECT_CONTROLS_WATCH_TIME_FINALIZED();
      CycleReportingTimer();

      // Run one last cycle that is long enough to trigger a new watch time
      // entry on the opposite of the current power watch time graph; i.e. if we
      // started on battery we'll now record one for ac and vice versa.
      EXPECT_WATCH_TIME(All, kWatchTime4);
      EXPECT_WATCH_TIME(Src, kWatchTime4);
      EXPECT_WATCH_TIME(Ac, kWatchTime4);
      EXPECT_WATCH_TIME_IF_VIDEO(DisplayInline, kWatchTime4);
      if (TestFlags & kStartWithNativeControls)
        EXPECT_WATCH_TIME(NativeControlsOff, kWatchTime4 - kWatchTime2);
      else
        EXPECT_WATCH_TIME(NativeControlsOn, kWatchTime4 - kWatchTime2);
    } else if (TestFlags & kTransitionDisplayWatchTime) {
      ASSERT_TRUE(TestFlags & kAccumulationContinuesAfterTest)
          << "kTransitionDisplayWatchTime tests must be done with "
             "kAccumulationContinuesAfterTest";

      EXPECT_DISPLAY_WATCH_TIME_FINALIZED();
      CycleReportingTimer();

      // Run one last cycle that is long enough to trigger a new watch time
      // entry on the opposite of the current power watch time graph; i.e. if we
      // started on battery we'll now record one for ac and vice versa.
      EXPECT_WATCH_TIME(All, kWatchTime4);
      EXPECT_WATCH_TIME(Src, kWatchTime4);
      EXPECT_WATCH_TIME(Ac, kWatchTime4);
      EXPECT_WATCH_TIME(NativeControlsOff, kWatchTime4);
      if (TestFlags & kStartWithDisplayFullscreen)
        EXPECT_WATCH_TIME_IF_VIDEO(DisplayInline, kWatchTime4 - kWatchTime2);
      else
        EXPECT_WATCH_TIME_IF_VIDEO(DisplayFullscreen,
                                   kWatchTime4 - kWatchTime2);
    }

    EXPECT_WATCH_TIME_FINALIZED();
    wtr_.reset();
  }

  MOCK_METHOD0(GetCurrentMediaTime, base::TimeDelta());

  const bool has_video_;
  testing::StrictMock<WatchTimeLogMonitor> media_log_;
  base::TestMessageLoop message_loop_;
  std::unique_ptr<WatchTimeReporter> wtr_;

 private:
  DISALLOW_COPY_AND_ASSIGN(WatchTimeReporterTest);
};

// Tests that watch time reporting is appropriately enabled or disabled.
TEST_P(WatchTimeReporterTest, WatchTimeReporter) {
  EXPECT_CALL(*this, GetCurrentMediaTime())
      .WillRepeatedly(testing::Return(base::TimeDelta()));

  Initialize(!has_video_, true, true, gfx::Size());
  wtr_->OnPlaying();
  EXPECT_EQ(!has_video_, IsMonitoring());
  EXPECT_FALSE(wtr_->IsSizeLargeEnoughToReportWatchTime());

  Initialize(true, true, true, gfx::Size());
  wtr_->OnPlaying();
  EXPECT_EQ(!has_video_, IsMonitoring());
  EXPECT_FALSE(wtr_->IsSizeLargeEnoughToReportWatchTime());

  constexpr gfx::Size kSizeTooSmall = gfx::Size(100, 100);
  Initialize(!has_video_, true, true, kSizeTooSmall);
  wtr_->OnPlaying();
  EXPECT_EQ(!has_video_, IsMonitoring());
  EXPECT_FALSE(wtr_->IsSizeLargeEnoughToReportWatchTime());

  Initialize(true, true, true, kSizeJustRight);
  wtr_->OnPlaying();
  EXPECT_TRUE(IsMonitoring());
  EXPECT_TRUE(wtr_->IsSizeLargeEnoughToReportWatchTime());

  Initialize(true, false, false, kSizeJustRight);
  wtr_->OnPlaying();
  EXPECT_TRUE(IsMonitoring());
  EXPECT_TRUE(wtr_->IsSizeLargeEnoughToReportWatchTime());

  Initialize(true, true, false, kSizeJustRight);
  wtr_->OnPlaying();
  EXPECT_TRUE(IsMonitoring());
  EXPECT_TRUE(wtr_->IsSizeLargeEnoughToReportWatchTime());

  Initialize(true, true, true, gfx::Size());
  wtr_->OnPlaying();
  EXPECT_EQ(!has_video_, IsMonitoring());
  EXPECT_FALSE(wtr_->IsSizeLargeEnoughToReportWatchTime());

  Initialize(true, false, false, gfx::Size());
  wtr_->OnPlaying();
  EXPECT_EQ(!has_video_, IsMonitoring());
  EXPECT_FALSE(wtr_->IsSizeLargeEnoughToReportWatchTime());

  Initialize(true, true, false, gfx::Size());
  wtr_->OnPlaying();
  EXPECT_EQ(!has_video_, IsMonitoring());
  EXPECT_FALSE(wtr_->IsSizeLargeEnoughToReportWatchTime());

  if (!has_video_)
    EXPECT_WATCH_TIME_FINALIZED();
  wtr_.reset();
}

TEST_P(WatchTimeReporterTest, WatchTimeReporterBasic) {
  constexpr base::TimeDelta kWatchTimeEarly = base::TimeDelta::FromSeconds(5);
  constexpr base::TimeDelta kWatchTimeLate = base::TimeDelta::FromSeconds(10);
  EXPECT_CALL(*this, GetCurrentMediaTime())
      .WillOnce(testing::Return(base::TimeDelta()))
      .WillOnce(testing::Return(kWatchTimeEarly))
      .WillRepeatedly(testing::Return(kWatchTimeLate));
  Initialize(true, true, true, kSizeJustRight);
  wtr_->OnPlaying();
  EXPECT_TRUE(IsMonitoring());

  // No log should have been generated yet since the message loop has not had
  // any chance to pump.
  CycleReportingTimer();

  wtr_->OnUnderflow();
  wtr_->OnUnderflow();
  EXPECT_WATCH_TIME(Ac, kWatchTimeLate);
  EXPECT_WATCH_TIME(All, kWatchTimeLate);
  EXPECT_WATCH_TIME(Eme, kWatchTimeLate);
  EXPECT_WATCH_TIME(Mse, kWatchTimeLate);
  EXPECT_WATCH_TIME(NativeControlsOff, kWatchTimeLate);
  EXPECT_WATCH_TIME_IF_VIDEO(DisplayInline, kWatchTimeLate);
  EXPECT_CALL(media_log_, OnUnderflowUpdate(2));
  CycleReportingTimer();

  EXPECT_WATCH_TIME_FINALIZED();
  wtr_.reset();
}

TEST_P(WatchTimeReporterTest, WatchTimeReporterUnderflow) {
  constexpr base::TimeDelta kWatchTimeEarly = base::TimeDelta::FromSeconds(10);
  constexpr base::TimeDelta kWatchTimeLate = base::TimeDelta::FromSeconds(15);
  EXPECT_CALL(*this, GetCurrentMediaTime())
      .WillOnce(testing::Return(base::TimeDelta()))
      .WillOnce(testing::Return(base::TimeDelta::FromSeconds(5)))
      .WillOnce(testing::Return(kWatchTimeEarly))
      .WillOnce(testing::Return(kWatchTimeEarly))
      .WillRepeatedly(testing::Return(kWatchTimeLate));
  Initialize(true, true, true, kSizeJustRight);
  wtr_->OnPlaying();
  EXPECT_TRUE(IsMonitoring());

  // No log should have been generated yet since the message loop has not had
  // any chance to pump.
  CycleReportingTimer();

  wtr_->OnUnderflow();
  wtr_->OnVolumeChange(0);

  // This underflow call should be ignored since it happens after the finalize.
  // Note: We use a muted call above to trigger finalize instead of say a pause
  // since media time will be the same in the event of a pause and no underflow
  // should trigger after a pause in any case.
  wtr_->OnUnderflow();

  EXPECT_WATCH_TIME(Ac, kWatchTimeEarly);
  EXPECT_WATCH_TIME(All, kWatchTimeEarly);
  EXPECT_WATCH_TIME(Eme, kWatchTimeEarly);
  EXPECT_WATCH_TIME(Mse, kWatchTimeEarly);
  EXPECT_WATCH_TIME(NativeControlsOff, kWatchTimeEarly);
  EXPECT_WATCH_TIME_IF_VIDEO(DisplayInline, kWatchTimeEarly);
  EXPECT_CALL(media_log_, OnUnderflowUpdate(1));
  EXPECT_WATCH_TIME_FINALIZED();
  CycleReportingTimer();
  wtr_.reset();
}

TEST_P(WatchTimeReporterTest, WatchTimeReporterShownHidden) {
  constexpr base::TimeDelta kWatchTimeEarly = base::TimeDelta::FromSeconds(8);
  constexpr base::TimeDelta kWatchTimeLate = base::TimeDelta::FromSeconds(25);
  EXPECT_CALL(*this, GetCurrentMediaTime())
      .WillOnce(testing::Return(base::TimeDelta()))
      .WillOnce(testing::Return(kWatchTimeEarly))
      .WillOnce(testing::Return(kWatchTimeEarly))
      .WillRepeatedly(testing::Return(kWatchTimeLate));
  Initialize(true, true, true, kSizeJustRight);
  wtr_->OnPlaying();
  EXPECT_TRUE(IsMonitoring());

  // If we have video, this will halt watch time collection, if only audio it
  // will do nothing. Consume the expectations if audio only.
  wtr_->OnHidden();
  if (!has_video_) {
    GetCurrentMediaTime();
    GetCurrentMediaTime();
  } else {
    const base::TimeDelta kExpectedWatchTime = kWatchTimeLate - kWatchTimeEarly;
    EXPECT_BACKGROUND_WATCH_TIME(Ac, kExpectedWatchTime);
    EXPECT_BACKGROUND_WATCH_TIME(All, kExpectedWatchTime);
    EXPECT_BACKGROUND_WATCH_TIME(Eme, kExpectedWatchTime);
    EXPECT_BACKGROUND_WATCH_TIME(Mse, kExpectedWatchTime);
    EXPECT_WATCH_TIME_FINALIZED();
  }

  const base::TimeDelta kExpectedWatchTime =
      has_video_ ? kWatchTimeEarly : kWatchTimeLate;
  EXPECT_WATCH_TIME(Ac, kExpectedWatchTime);
  EXPECT_WATCH_TIME(All, kExpectedWatchTime);
  EXPECT_WATCH_TIME(Eme, kExpectedWatchTime);
  EXPECT_WATCH_TIME(Mse, kExpectedWatchTime);
  EXPECT_WATCH_TIME(NativeControlsOff, kExpectedWatchTime);
  EXPECT_WATCH_TIME_IF_VIDEO(DisplayInline, kExpectedWatchTime);
  EXPECT_WATCH_TIME_FINALIZED();
  wtr_.reset();
}

TEST_P(WatchTimeReporterTest, WatchTimeReporterBackgroundHysteresis) {
  // Only run these background tests when video is present.
  if (!has_video_)
    return;

  constexpr base::TimeDelta kWatchTimeEarly = base::TimeDelta::FromSeconds(8);
  constexpr base::TimeDelta kWatchTimeLate = base::TimeDelta::FromSeconds(10);
  EXPECT_CALL(*this, GetCurrentMediaTime())
      .WillOnce(testing::Return(base::TimeDelta()))  // 2x for playing
      .WillOnce(testing::Return(base::TimeDelta()))
      .WillOnce(testing::Return(kWatchTimeEarly))  // 2x for shown
      .WillOnce(testing::Return(kWatchTimeEarly))
      .WillOnce(testing::Return(kWatchTimeEarly))  // 2x for hidden
      .WillOnce(testing::Return(kWatchTimeEarly))
      .WillOnce(testing::Return(kWatchTimeEarly))  // 1x for timer cycle.
      .WillRepeatedly(testing::Return(kWatchTimeLate));
  Initialize(true, true, true, kSizeJustRight);
  wtr_->OnHidden();
  wtr_->OnPlaying();
  EXPECT_TRUE(IsBackgroundMonitoring());
  EXPECT_FALSE(IsMonitoring());

  wtr_->OnShown();
  wtr_->OnHidden();
  EXPECT_BACKGROUND_WATCH_TIME(Ac, kWatchTimeEarly);
  EXPECT_BACKGROUND_WATCH_TIME(All, kWatchTimeEarly);
  EXPECT_BACKGROUND_WATCH_TIME(Eme, kWatchTimeEarly);
  EXPECT_BACKGROUND_WATCH_TIME(Mse, kWatchTimeEarly);
  EXPECT_TRUE(IsBackgroundMonitoring());
  EXPECT_TRUE(IsMonitoring());
  EXPECT_WATCH_TIME_FINALIZED();
  CycleReportingTimer();

  EXPECT_TRUE(IsBackgroundMonitoring());
  EXPECT_FALSE(IsMonitoring());

  EXPECT_BACKGROUND_WATCH_TIME(Ac, kWatchTimeLate);
  EXPECT_BACKGROUND_WATCH_TIME(All, kWatchTimeLate);
  EXPECT_BACKGROUND_WATCH_TIME(Eme, kWatchTimeLate);
  EXPECT_BACKGROUND_WATCH_TIME(Mse, kWatchTimeLate);
  EXPECT_WATCH_TIME_FINALIZED();
  wtr_.reset();
}

TEST_P(WatchTimeReporterTest, WatchTimeReporterShownHiddenBackground) {
  // Only run these background tests when video is present.
  if (!has_video_)
    return;

  constexpr base::TimeDelta kWatchTimeEarly = base::TimeDelta::FromSeconds(8);
  constexpr base::TimeDelta kWatchTimeLate = base::TimeDelta::FromSeconds(10);
  EXPECT_CALL(*this, GetCurrentMediaTime())
      .WillOnce(testing::Return(base::TimeDelta()))
      .WillOnce(testing::Return(base::TimeDelta()))
      .WillOnce(testing::Return(kWatchTimeEarly))
      .WillOnce(testing::Return(kWatchTimeEarly))
      .WillRepeatedly(testing::Return(kWatchTimeLate));

  Initialize(true, true, true, kSizeJustRight);
  wtr_->OnHidden();
  wtr_->OnPlaying();
  EXPECT_TRUE(IsBackgroundMonitoring());
  EXPECT_FALSE(IsMonitoring());

  wtr_->OnShown();
  EXPECT_BACKGROUND_WATCH_TIME(Ac, kWatchTimeEarly);
  EXPECT_BACKGROUND_WATCH_TIME(All, kWatchTimeEarly);
  EXPECT_BACKGROUND_WATCH_TIME(Eme, kWatchTimeEarly);
  EXPECT_BACKGROUND_WATCH_TIME(Mse, kWatchTimeEarly);
  EXPECT_WATCH_TIME_FINALIZED();
  CycleReportingTimer();

  EXPECT_FALSE(IsBackgroundMonitoring());
  EXPECT_TRUE(IsMonitoring());

  EXPECT_WATCH_TIME_FINALIZED();
  wtr_.reset();
}

TEST_P(WatchTimeReporterTest, WatchTimeReporterHiddenPausedBackground) {
  // Only run these background tests when video is present.
  if (!has_video_)
    return;

  constexpr base::TimeDelta kWatchTime = base::TimeDelta::FromSeconds(8);
  EXPECT_CALL(*this, GetCurrentMediaTime())
      .WillOnce(testing::Return(base::TimeDelta()))
      .WillOnce(testing::Return(base::TimeDelta()))
      .WillRepeatedly(testing::Return(kWatchTime));
  Initialize(true, true, true, kSizeJustRight);
  wtr_->OnHidden();
  wtr_->OnPlaying();
  EXPECT_TRUE(IsBackgroundMonitoring());
  EXPECT_FALSE(IsMonitoring());

  wtr_->OnPaused();
  EXPECT_BACKGROUND_WATCH_TIME(Ac, kWatchTime);
  EXPECT_BACKGROUND_WATCH_TIME(All, kWatchTime);
  EXPECT_BACKGROUND_WATCH_TIME(Eme, kWatchTime);
  EXPECT_BACKGROUND_WATCH_TIME(Mse, kWatchTime);
  EXPECT_WATCH_TIME_FINALIZED();
  CycleReportingTimer();

  EXPECT_FALSE(IsBackgroundMonitoring());
  EXPECT_FALSE(IsMonitoring());
  wtr_.reset();
}

TEST_P(WatchTimeReporterTest, WatchTimeReporterHiddenSeekedBackground) {
  // Only run these background tests when video is present.
  if (!has_video_)
    return;

  constexpr base::TimeDelta kWatchTime = base::TimeDelta::FromSeconds(8);
  EXPECT_CALL(*this, GetCurrentMediaTime())
      .WillOnce(testing::Return(base::TimeDelta()))
      .WillOnce(testing::Return(base::TimeDelta()))
      .WillRepeatedly(testing::Return(kWatchTime));
  Initialize(true, false, true, kSizeJustRight);
  wtr_->OnHidden();
  wtr_->OnPlaying();
  EXPECT_TRUE(IsBackgroundMonitoring());
  EXPECT_FALSE(IsMonitoring());

  EXPECT_BACKGROUND_WATCH_TIME(Ac, kWatchTime);
  EXPECT_BACKGROUND_WATCH_TIME(All, kWatchTime);
  EXPECT_BACKGROUND_WATCH_TIME(Eme, kWatchTime);
  EXPECT_BACKGROUND_WATCH_TIME(Src, kWatchTime);
  EXPECT_WATCH_TIME_FINALIZED();
  wtr_->OnSeeking();

  EXPECT_FALSE(IsBackgroundMonitoring());
  EXPECT_FALSE(IsMonitoring());
  wtr_.reset();
}

TEST_P(WatchTimeReporterTest, WatchTimeReporterHiddenPowerBackground) {
  // Only run these background tests when video is present.
  if (!has_video_)
    return;

  constexpr base::TimeDelta kWatchTime1 = base::TimeDelta::FromSeconds(8);
  constexpr base::TimeDelta kWatchTime2 = base::TimeDelta::FromSeconds(16);
  EXPECT_CALL(*this, GetCurrentMediaTime())
      .WillOnce(testing::Return(base::TimeDelta()))
      .WillOnce(testing::Return(base::TimeDelta()))
      .WillOnce(testing::Return(kWatchTime1))
      .WillOnce(testing::Return(kWatchTime1))
      .WillRepeatedly(testing::Return(kWatchTime2));
  Initialize(true, true, true, kSizeJustRight);
  wtr_->OnHidden();
  wtr_->OnPlaying();
  EXPECT_TRUE(IsBackgroundMonitoring());
  EXPECT_FALSE(IsMonitoring());

  OnPowerStateChange(true);
  EXPECT_BACKGROUND_WATCH_TIME(Ac, kWatchTime1);
  EXPECT_BACKGROUND_WATCH_TIME(All, kWatchTime1);
  EXPECT_BACKGROUND_WATCH_TIME(Eme, kWatchTime1);
  EXPECT_BACKGROUND_WATCH_TIME(Mse, kWatchTime1);
  EXPECT_POWER_WATCH_TIME_FINALIZED();
  CycleReportingTimer();

  wtr_->OnPaused();
  EXPECT_BACKGROUND_WATCH_TIME(Battery, kWatchTime2 - kWatchTime1);
  EXPECT_BACKGROUND_WATCH_TIME(All, kWatchTime2);
  EXPECT_BACKGROUND_WATCH_TIME(Eme, kWatchTime2);
  EXPECT_BACKGROUND_WATCH_TIME(Mse, kWatchTime2);
  EXPECT_WATCH_TIME_FINALIZED();
  CycleReportingTimer();

  EXPECT_FALSE(IsBackgroundMonitoring());
  EXPECT_FALSE(IsMonitoring());
  wtr_.reset();
}

TEST_P(WatchTimeReporterTest, WatchTimeReporterHiddenControlsBackground) {
  // Only run these background tests when video is present.
  if (!has_video_)
    return;

  constexpr base::TimeDelta kWatchTime1 = base::TimeDelta::FromSeconds(8);
  constexpr base::TimeDelta kWatchTime2 = base::TimeDelta::FromSeconds(16);
  EXPECT_CALL(*this, GetCurrentMediaTime())
      .WillOnce(testing::Return(base::TimeDelta()))
      .WillOnce(testing::Return(base::TimeDelta()))
      .WillOnce(testing::Return(kWatchTime1))
      .WillOnce(testing::Return(kWatchTime2));
  Initialize(true, true, true, kSizeJustRight);
  wtr_->OnHidden();
  wtr_->OnPlaying();
  EXPECT_TRUE(IsBackgroundMonitoring());
  EXPECT_FALSE(IsMonitoring());

  OnNativeControlsEnabled(true);

  EXPECT_BACKGROUND_WATCH_TIME(Ac, kWatchTime1);
  EXPECT_BACKGROUND_WATCH_TIME(All, kWatchTime1);
  EXPECT_BACKGROUND_WATCH_TIME(Eme, kWatchTime1);
  EXPECT_BACKGROUND_WATCH_TIME(Mse, kWatchTime1);
  CycleReportingTimer();

  wtr_->OnPaused();
  EXPECT_BACKGROUND_WATCH_TIME(Ac, kWatchTime2);
  EXPECT_BACKGROUND_WATCH_TIME(All, kWatchTime2);
  EXPECT_BACKGROUND_WATCH_TIME(Eme, kWatchTime2);
  EXPECT_BACKGROUND_WATCH_TIME(Mse, kWatchTime2);
  EXPECT_WATCH_TIME_FINALIZED();
  CycleReportingTimer();

  EXPECT_FALSE(IsBackgroundMonitoring());
  EXPECT_FALSE(IsMonitoring());
  wtr_.reset();
}

TEST_P(WatchTimeReporterTest, WatchTimeReporterHiddenDisplayTypeBackground) {
  // Only run these background tests when video is present.
  if (!has_video_)
    return;

  constexpr base::TimeDelta kWatchTime1 = base::TimeDelta::FromSeconds(8);
  constexpr base::TimeDelta kWatchTime2 = base::TimeDelta::FromSeconds(16);
  EXPECT_CALL(*this, GetCurrentMediaTime())
      .WillOnce(testing::Return(base::TimeDelta()))
      .WillOnce(testing::Return(base::TimeDelta()))
      .WillOnce(testing::Return(kWatchTime1))
      .WillOnce(testing::Return(kWatchTime2));
  Initialize(true, true, true, kSizeJustRight);
  wtr_->OnHidden();
  wtr_->OnPlaying();
  EXPECT_TRUE(IsBackgroundMonitoring());
  EXPECT_FALSE(IsMonitoring());

  OnDisplayTypeChanged(WebMediaPlayer::DisplayType::kFullscreen);

  EXPECT_BACKGROUND_WATCH_TIME(Ac, kWatchTime1);
  EXPECT_BACKGROUND_WATCH_TIME(All, kWatchTime1);
  EXPECT_BACKGROUND_WATCH_TIME(Eme, kWatchTime1);
  EXPECT_BACKGROUND_WATCH_TIME(Mse, kWatchTime1);
  CycleReportingTimer();

  wtr_->OnPaused();
  EXPECT_BACKGROUND_WATCH_TIME(Ac, kWatchTime2);
  EXPECT_BACKGROUND_WATCH_TIME(All, kWatchTime2);
  EXPECT_BACKGROUND_WATCH_TIME(Eme, kWatchTime2);
  EXPECT_BACKGROUND_WATCH_TIME(Mse, kWatchTime2);
  EXPECT_WATCH_TIME_FINALIZED();
  CycleReportingTimer();

  EXPECT_FALSE(IsBackgroundMonitoring());
  EXPECT_FALSE(IsMonitoring());
  wtr_.reset();
}

TEST_P(WatchTimeReporterTest, WatchTimeReporterMultiplePartialFinalize) {
  // Only run these background tests when video is present.
  if (!has_video_)
    return;

  constexpr base::TimeDelta kWatchTime1 = base::TimeDelta::FromSeconds(8);
  constexpr base::TimeDelta kWatchTime2 = base::TimeDelta::FromSeconds(16);

  // Transition controls and battery.
  {
    EXPECT_CALL(*this, GetCurrentMediaTime())
        .WillOnce(testing::Return(base::TimeDelta()))
        .WillOnce(testing::Return(kWatchTime1))
        .WillOnce(testing::Return(kWatchTime1))
        .WillOnce(testing::Return(kWatchTime1))
        .WillOnce(testing::Return(kWatchTime2));
    Initialize(true, true, true, kSizeJustRight);
    wtr_->OnPlaying();
    EXPECT_TRUE(IsMonitoring());

    OnNativeControlsEnabled(true);
    OnPowerStateChange(true);

    EXPECT_WATCH_TIME(Ac, kWatchTime1);
    EXPECT_WATCH_TIME(All, kWatchTime1);
    EXPECT_WATCH_TIME(Eme, kWatchTime1);
    EXPECT_WATCH_TIME(Mse, kWatchTime1);
    EXPECT_WATCH_TIME(NativeControlsOff, kWatchTime1);
    EXPECT_WATCH_TIME_IF_VIDEO(DisplayInline, kWatchTime1);
    EXPECT_CONTROLS_WATCH_TIME_FINALIZED();
    EXPECT_POWER_WATCH_TIME_FINALIZED();
    CycleReportingTimer();

    wtr_->OnPaused();
    EXPECT_WATCH_TIME(All, kWatchTime2);
    EXPECT_WATCH_TIME(Eme, kWatchTime2);
    EXPECT_WATCH_TIME(Mse, kWatchTime2);
    EXPECT_WATCH_TIME_IF_VIDEO(DisplayInline, kWatchTime2);
    EXPECT_WATCH_TIME(NativeControlsOn, kWatchTime2 - kWatchTime1);
    EXPECT_WATCH_TIME(Battery, kWatchTime2 - kWatchTime1);
    EXPECT_WATCH_TIME_FINALIZED();
    CycleReportingTimer();

    EXPECT_FALSE(IsMonitoring());
    wtr_.reset();
  }

  // Transition display type and battery.
  {
    EXPECT_CALL(*this, GetCurrentMediaTime())
        .WillOnce(testing::Return(base::TimeDelta()))
        .WillOnce(testing::Return(kWatchTime1))
        .WillOnce(testing::Return(kWatchTime1))
        .WillOnce(testing::Return(kWatchTime1))
        .WillOnce(testing::Return(kWatchTime2));
    Initialize(true, true, true, kSizeJustRight);
    wtr_->OnPlaying();
    EXPECT_TRUE(IsMonitoring());

    OnDisplayTypeChanged(WebMediaPlayer::DisplayType::kFullscreen);
    OnPowerStateChange(true);

    EXPECT_WATCH_TIME(Ac, kWatchTime1);
    EXPECT_WATCH_TIME(All, kWatchTime1);
    EXPECT_WATCH_TIME(Eme, kWatchTime1);
    EXPECT_WATCH_TIME(Mse, kWatchTime1);
    EXPECT_WATCH_TIME(NativeControlsOff, kWatchTime1);
    EXPECT_WATCH_TIME_IF_VIDEO(DisplayInline, kWatchTime1);
    EXPECT_DISPLAY_WATCH_TIME_FINALIZED();
    EXPECT_POWER_WATCH_TIME_FINALIZED();
    CycleReportingTimer();

    wtr_->OnPaused();
    EXPECT_WATCH_TIME(All, kWatchTime2);
    EXPECT_WATCH_TIME(Eme, kWatchTime2);
    EXPECT_WATCH_TIME(Mse, kWatchTime2);
    EXPECT_WATCH_TIME(NativeControlsOff, kWatchTime2);
    EXPECT_WATCH_TIME_IF_VIDEO(DisplayFullscreen, kWatchTime2 - kWatchTime1);
    EXPECT_WATCH_TIME(Battery, kWatchTime2 - kWatchTime1);
    EXPECT_WATCH_TIME_FINALIZED();
    CycleReportingTimer();

    EXPECT_FALSE(IsMonitoring());
    wtr_.reset();
  }

  // Transition controls, battery and display type.
  {
    EXPECT_CALL(*this, GetCurrentMediaTime())
        .WillOnce(testing::Return(base::TimeDelta()))
        .WillOnce(testing::Return(kWatchTime1))
        .WillOnce(testing::Return(kWatchTime1))
        .WillOnce(testing::Return(kWatchTime1))
        .WillOnce(testing::Return(kWatchTime1))
        .WillOnce(testing::Return(kWatchTime2));
    Initialize(true, true, true, kSizeJustRight);
    wtr_->OnPlaying();
    EXPECT_TRUE(IsMonitoring());

    OnNativeControlsEnabled(true);
    OnPowerStateChange(true);
    OnDisplayTypeChanged(WebMediaPlayer::DisplayType::kPictureInPicture);

    EXPECT_WATCH_TIME(Ac, kWatchTime1);
    EXPECT_WATCH_TIME(All, kWatchTime1);
    EXPECT_WATCH_TIME(Eme, kWatchTime1);
    EXPECT_WATCH_TIME(Mse, kWatchTime1);
    EXPECT_WATCH_TIME(NativeControlsOff, kWatchTime1);
    EXPECT_WATCH_TIME_IF_VIDEO(DisplayInline, kWatchTime1);
    EXPECT_CONTROLS_WATCH_TIME_FINALIZED();
    EXPECT_POWER_WATCH_TIME_FINALIZED();
    EXPECT_DISPLAY_WATCH_TIME_FINALIZED();
    CycleReportingTimer();

    wtr_->OnPaused();
    EXPECT_WATCH_TIME(All, kWatchTime2);
    EXPECT_WATCH_TIME(Eme, kWatchTime2);
    EXPECT_WATCH_TIME(Mse, kWatchTime2);
    EXPECT_WATCH_TIME_IF_VIDEO(DisplayPictureInPicture,
                               kWatchTime2 - kWatchTime1);
    EXPECT_WATCH_TIME(NativeControlsOn, kWatchTime2 - kWatchTime1);
    EXPECT_WATCH_TIME(Battery, kWatchTime2 - kWatchTime1);
    EXPECT_WATCH_TIME_FINALIZED();
    CycleReportingTimer();

    EXPECT_FALSE(IsMonitoring());
    wtr_.reset();
  }
}

// Tests that starting from a non-zero base works.
TEST_P(WatchTimeReporterTest, WatchTimeReporterNonZeroStart) {
  constexpr base::TimeDelta kWatchTime1 = base::TimeDelta::FromSeconds(5);
  constexpr base::TimeDelta kWatchTime2 = base::TimeDelta::FromSeconds(15);
  EXPECT_CALL(*this, GetCurrentMediaTime())
      .WillOnce(testing::Return(kWatchTime1))
      .WillRepeatedly(testing::Return(kWatchTime2));
  Initialize(true, true, true, kSizeJustRight);
  wtr_->OnPlaying();
  EXPECT_TRUE(IsMonitoring());

  const base::TimeDelta kWatchTime = kWatchTime2 - kWatchTime1;
  EXPECT_WATCH_TIME(Ac, kWatchTime);
  EXPECT_WATCH_TIME(All, kWatchTime);
  EXPECT_WATCH_TIME(Eme, kWatchTime);
  EXPECT_WATCH_TIME(Mse, kWatchTime);
  EXPECT_WATCH_TIME(NativeControlsOff, kWatchTime);
  EXPECT_WATCH_TIME_IF_VIDEO(DisplayInline, kWatchTime);
  CycleReportingTimer();

  EXPECT_WATCH_TIME_FINALIZED();
  wtr_.reset();
}

// Tests that seeking causes an immediate finalization.
TEST_P(WatchTimeReporterTest, SeekFinalizes) {
  constexpr base::TimeDelta kWatchTime = base::TimeDelta::FromSeconds(10);
  EXPECT_CALL(*this, GetCurrentMediaTime())
      .WillOnce(testing::Return(base::TimeDelta()))
      .WillOnce(testing::Return(kWatchTime));
  Initialize(true, true, true, kSizeJustRight);
  wtr_->OnPlaying();
  EXPECT_TRUE(IsMonitoring());

  EXPECT_WATCH_TIME(Ac, kWatchTime);
  EXPECT_WATCH_TIME(All, kWatchTime);
  EXPECT_WATCH_TIME(Eme, kWatchTime);
  EXPECT_WATCH_TIME(Mse, kWatchTime);
  EXPECT_WATCH_TIME(NativeControlsOff, kWatchTime);
  EXPECT_WATCH_TIME_IF_VIDEO(DisplayInline, kWatchTime);
  EXPECT_WATCH_TIME_FINALIZED();
  wtr_->OnSeeking();
}

// Tests that seeking causes an immediate finalization, but does not trample a
// previously set finalize time.
TEST_P(WatchTimeReporterTest, SeekFinalizeDoesNotTramplePreviousFinalize) {
  constexpr base::TimeDelta kWatchTime = base::TimeDelta::FromSeconds(10);
  EXPECT_CALL(*this, GetCurrentMediaTime())
      .WillOnce(testing::Return(base::TimeDelta()))
      .WillOnce(testing::Return(kWatchTime));
  Initialize(true, true, true, kSizeJustRight);
  wtr_->OnPlaying();
  EXPECT_TRUE(IsMonitoring());

  EXPECT_WATCH_TIME(Ac, kWatchTime);
  EXPECT_WATCH_TIME(All, kWatchTime);
  EXPECT_WATCH_TIME(Eme, kWatchTime);
  EXPECT_WATCH_TIME(Mse, kWatchTime);
  EXPECT_WATCH_TIME(NativeControlsOff, kWatchTime);
  EXPECT_WATCH_TIME_IF_VIDEO(DisplayInline, kWatchTime);
  EXPECT_WATCH_TIME_FINALIZED();
  wtr_->OnPaused();
  wtr_->OnSeeking();
}

// Tests that watch time is finalized upon destruction.
TEST_P(WatchTimeReporterTest, WatchTimeReporterFinalizeOnDestruction) {
  constexpr base::TimeDelta kWatchTime = base::TimeDelta::FromSeconds(10);
  EXPECT_CALL(*this, GetCurrentMediaTime())
      .WillOnce(testing::Return(base::TimeDelta()))
      .WillOnce(testing::Return(kWatchTime));
  Initialize(true, true, true, kSizeJustRight);
  wtr_->OnPlaying();
  EXPECT_TRUE(IsMonitoring());

  // Finalize the histogram before any cycles of the timer have run.
  EXPECT_WATCH_TIME(Ac, kWatchTime);
  EXPECT_WATCH_TIME(All, kWatchTime);
  EXPECT_WATCH_TIME(Eme, kWatchTime);
  EXPECT_WATCH_TIME(Mse, kWatchTime);
  EXPECT_WATCH_TIME(NativeControlsOff, kWatchTime);
  EXPECT_WATCH_TIME_IF_VIDEO(DisplayInline, kWatchTime);
  EXPECT_WATCH_TIME_FINALIZED();
  wtr_.reset();
}

// Tests that watch time categories are mapped correctly.
TEST_P(WatchTimeReporterTest, WatchTimeCategoryMapping) {
  constexpr base::TimeDelta kWatchTime = base::TimeDelta::FromSeconds(10);

  // Verify ac, all, src, non-native controls
  EXPECT_CALL(*this, GetCurrentMediaTime())
      .WillOnce(testing::Return(base::TimeDelta()))
      .WillOnce(testing::Return(kWatchTime));
  Initialize(true, false, false, kSizeJustRight);
  wtr_->OnPlaying();
  EXPECT_TRUE(IsMonitoring());
  EXPECT_WATCH_TIME(Ac, kWatchTime);
  EXPECT_WATCH_TIME(All, kWatchTime);
  EXPECT_WATCH_TIME(Src, kWatchTime);
  EXPECT_WATCH_TIME(NativeControlsOff, kWatchTime);
  EXPECT_WATCH_TIME_IF_VIDEO(DisplayInline, kWatchTime);
  EXPECT_WATCH_TIME_FINALIZED();
  wtr_.reset();

  // Verify ac, all, mse, non-native controls
  EXPECT_CALL(*this, GetCurrentMediaTime())
      .WillOnce(testing::Return(base::TimeDelta()))
      .WillOnce(testing::Return(kWatchTime));
  Initialize(true, true, false, kSizeJustRight);
  wtr_->OnPlaying();
  EXPECT_TRUE(IsMonitoring());
  EXPECT_WATCH_TIME(Ac, kWatchTime);
  EXPECT_WATCH_TIME(All, kWatchTime);
  EXPECT_WATCH_TIME(Mse, kWatchTime);
  EXPECT_WATCH_TIME(NativeControlsOff, kWatchTime);
  EXPECT_WATCH_TIME_IF_VIDEO(DisplayInline, kWatchTime);
  EXPECT_WATCH_TIME_FINALIZED();
  wtr_.reset();

  // Verify ac, all, eme, src, non-native controls
  EXPECT_CALL(*this, GetCurrentMediaTime())
      .WillOnce(testing::Return(base::TimeDelta()))
      .WillOnce(testing::Return(kWatchTime));
  Initialize(true, false, true, kSizeJustRight);
  wtr_->OnPlaying();
  EXPECT_TRUE(IsMonitoring());
  EXPECT_WATCH_TIME(Ac, kWatchTime);
  EXPECT_WATCH_TIME(All, kWatchTime);
  EXPECT_WATCH_TIME(Eme, kWatchTime);
  EXPECT_WATCH_TIME(Src, kWatchTime);
  EXPECT_WATCH_TIME(NativeControlsOff, kWatchTime);
  EXPECT_WATCH_TIME_IF_VIDEO(DisplayInline, kWatchTime);
  EXPECT_WATCH_TIME_FINALIZED();
  wtr_.reset();

  // Verify all, battery, src, non-native controls
  EXPECT_CALL(*this, GetCurrentMediaTime())
      .WillOnce(testing::Return(base::TimeDelta()))
      .WillOnce(testing::Return(kWatchTime));
  Initialize(true, false, false, kSizeJustRight);
  wtr_->OnPlaying();
  SetOnBatteryPower(true);
  EXPECT_TRUE(IsMonitoring());
  EXPECT_WATCH_TIME(All, kWatchTime);
  EXPECT_WATCH_TIME(Battery, kWatchTime);
  EXPECT_WATCH_TIME(Src, kWatchTime);
  EXPECT_WATCH_TIME(NativeControlsOff, kWatchTime);
  EXPECT_WATCH_TIME_IF_VIDEO(DisplayInline, kWatchTime);
  EXPECT_WATCH_TIME_FINALIZED();
  wtr_.reset();

  // Verify ac, all, src, native controls
  EXPECT_CALL(*this, GetCurrentMediaTime())
      .WillOnce(testing::Return(base::TimeDelta()))
      .WillOnce(testing::Return(kWatchTime));
  Initialize(true, false, false, kSizeJustRight);
  OnNativeControlsEnabled(true);
  wtr_->OnPlaying();
  EXPECT_TRUE(IsMonitoring());
  EXPECT_WATCH_TIME(Ac, kWatchTime);
  EXPECT_WATCH_TIME(All, kWatchTime);
  EXPECT_WATCH_TIME(Src, kWatchTime);
  EXPECT_WATCH_TIME(NativeControlsOn, kWatchTime);
  EXPECT_WATCH_TIME_IF_VIDEO(DisplayInline, kWatchTime);
  EXPECT_WATCH_TIME_FINALIZED();
  wtr_.reset();

  // Verify all, battery, src, non-native controls, display fullscreen
  EXPECT_CALL(*this, GetCurrentMediaTime())
      .WillOnce(testing::Return(base::TimeDelta()))
      .WillOnce(testing::Return(kWatchTime));
  Initialize(true, false, false, kSizeJustRight);
  OnDisplayTypeChanged(WebMediaPlayer::DisplayType::kFullscreen);
  wtr_->OnPlaying();
  SetOnBatteryPower(true);
  EXPECT_TRUE(IsMonitoring());
  EXPECT_WATCH_TIME(All, kWatchTime);
  EXPECT_WATCH_TIME(Battery, kWatchTime);
  EXPECT_WATCH_TIME(Src, kWatchTime);
  EXPECT_WATCH_TIME(NativeControlsOff, kWatchTime);
  EXPECT_WATCH_TIME_IF_VIDEO(DisplayFullscreen, kWatchTime);
  EXPECT_WATCH_TIME_FINALIZED();
  wtr_.reset();

  // Verify ac, all, src, native controls, display picture-in-picture
  EXPECT_CALL(*this, GetCurrentMediaTime())
      .WillOnce(testing::Return(base::TimeDelta()))
      .WillOnce(testing::Return(kWatchTime));
  Initialize(true, false, false, kSizeJustRight);
  OnNativeControlsEnabled(true);
  OnDisplayTypeChanged(WebMediaPlayer::DisplayType::kPictureInPicture);
  wtr_->OnPlaying();
  EXPECT_TRUE(IsMonitoring());
  EXPECT_WATCH_TIME(Ac, kWatchTime);
  EXPECT_WATCH_TIME(All, kWatchTime);
  EXPECT_WATCH_TIME(Src, kWatchTime);
  EXPECT_WATCH_TIME(NativeControlsOn, kWatchTime);
  EXPECT_WATCH_TIME_IF_VIDEO(DisplayPictureInPicture, kWatchTime);
  EXPECT_WATCH_TIME_FINALIZED();
  wtr_.reset();
}

TEST_P(WatchTimeReporterTest, PlayPauseHysteresisContinuation) {
  RunHysteresisTest<kAccumulationContinuesAfterTest>([this]() {
    wtr_->OnPaused();
    wtr_->OnPlaying();
  });
}

TEST_P(WatchTimeReporterTest, PlayPauseHysteresisFinalized) {
  RunHysteresisTest([this]() { wtr_->OnPaused(); });
}

TEST_P(WatchTimeReporterTest, OnVolumeChangeHysteresisContinuation) {
  RunHysteresisTest<kAccumulationContinuesAfterTest>([this]() {
    wtr_->OnVolumeChange(0);
    wtr_->OnVolumeChange(1);
  });
}

TEST_P(WatchTimeReporterTest, OnVolumeChangeHysteresisFinalized) {
  RunHysteresisTest([this]() { wtr_->OnVolumeChange(0); });
}

TEST_P(WatchTimeReporterTest, OnShownHiddenHysteresisContinuation) {
  if (!has_video_)
    return;
  RunHysteresisTest<kAccumulationContinuesAfterTest>([this]() {
    wtr_->OnHidden();
    wtr_->OnShown();
  });
}

TEST_P(WatchTimeReporterTest, OnShownHiddenHysteresisFinalized) {
  if (!has_video_)
    return;
  RunHysteresisTest([this]() { wtr_->OnHidden(); });
}

TEST_P(WatchTimeReporterTest, OnPowerStateChangeHysteresisBatteryContinuation) {
  RunHysteresisTest<kAccumulationContinuesAfterTest |
                    kFinalizeExitDoesNotRequireCurrentTime | kStartOnBattery>(
      [this]() {
        OnPowerStateChange(false);
        OnPowerStateChange(true);
      });
}

TEST_P(WatchTimeReporterTest, OnPowerStateChangeHysteresisBatteryFinalized) {
  RunHysteresisTest<kAccumulationContinuesAfterTest | kFinalizePowerWatchTime |
                    kStartOnBattery>([this]() { OnPowerStateChange(false); });
}

TEST_P(WatchTimeReporterTest, OnPowerStateChangeHysteresisAcContinuation) {
  RunHysteresisTest<kAccumulationContinuesAfterTest |
                    kFinalizeExitDoesNotRequireCurrentTime>([this]() {
    OnPowerStateChange(true);
    OnPowerStateChange(false);
  });
}

TEST_P(WatchTimeReporterTest, OnPowerStateChangeHysteresisAcFinalized) {
  RunHysteresisTest<kAccumulationContinuesAfterTest | kFinalizePowerWatchTime>(
      [this]() { OnPowerStateChange(true); });
}

TEST_P(WatchTimeReporterTest, OnPowerStateChangeBatteryTransitions) {
  RunHysteresisTest<kAccumulationContinuesAfterTest | kFinalizePowerWatchTime |
                    kStartOnBattery | kTransitionPowerWatchTime>(
      [this]() { OnPowerStateChange(false); });
}

TEST_P(WatchTimeReporterTest, OnPowerStateChangeAcTransitions) {
  RunHysteresisTest<kAccumulationContinuesAfterTest | kFinalizePowerWatchTime |
                    kTransitionPowerWatchTime>(
      [this]() { OnPowerStateChange(true); });
}

TEST_P(WatchTimeReporterTest, OnControlsChangeHysteresisNativeContinuation) {
  RunHysteresisTest<kAccumulationContinuesAfterTest |
                    kFinalizeExitDoesNotRequireCurrentTime |
                    kStartWithNativeControls>([this]() {
    OnNativeControlsEnabled(false);
    OnNativeControlsEnabled(true);
  });
}

TEST_P(WatchTimeReporterTest, OnControlsChangeHysteresisNativeFinalized) {
  RunHysteresisTest<kAccumulationContinuesAfterTest |
                    kFinalizeControlsWatchTime | kStartWithNativeControls>(
      [this]() { OnNativeControlsEnabled(false); });
}

TEST_P(WatchTimeReporterTest, OnControlsChangeHysteresisNativeOffContinuation) {
  RunHysteresisTest<kAccumulationContinuesAfterTest |
                    kFinalizeExitDoesNotRequireCurrentTime>([this]() {
    OnNativeControlsEnabled(true);
    OnNativeControlsEnabled(false);
  });
}

TEST_P(WatchTimeReporterTest, OnControlsChangeHysteresisNativeOffFinalized) {
  RunHysteresisTest<kAccumulationContinuesAfterTest |
                    kFinalizeControlsWatchTime>(
      [this]() { OnNativeControlsEnabled(true); });
}

TEST_P(WatchTimeReporterTest, OnControlsChangeToNativeOff) {
  RunHysteresisTest<kAccumulationContinuesAfterTest |
                    kFinalizeControlsWatchTime | kStartWithNativeControls |
                    kTransitionControlsWatchTime>(
      [this]() { OnNativeControlsEnabled(false); });
}

TEST_P(WatchTimeReporterTest, OnControlsChangeToNative) {
  RunHysteresisTest<kAccumulationContinuesAfterTest |
                    kFinalizeControlsWatchTime | kTransitionControlsWatchTime>(
      [this]() { OnNativeControlsEnabled(true); });
}

TEST_P(WatchTimeReporterTest,
       OnDisplayTypeChangeHysteresisFullscreenContinuation) {
  RunHysteresisTest<kAccumulationContinuesAfterTest |
                    kFinalizeExitDoesNotRequireCurrentTime |
                    kStartWithDisplayFullscreen>([this]() {
    OnDisplayTypeChanged(WebMediaPlayer::DisplayType::kInline);
    OnDisplayTypeChanged(WebMediaPlayer::DisplayType::kFullscreen);
  });
}

TEST_P(WatchTimeReporterTest, OnDisplayTypeChangeHysteresisNativeFinalized) {
  RunHysteresisTest<kAccumulationContinuesAfterTest |
                    kFinalizeDisplayWatchTime | kStartWithDisplayFullscreen>(
      [this]() { OnDisplayTypeChanged(WebMediaPlayer::DisplayType::kInline); });
}

TEST_P(WatchTimeReporterTest, OnDisplayTypeChangeHysteresisInlineContinuation) {
  RunHysteresisTest<kAccumulationContinuesAfterTest |
                    kFinalizeExitDoesNotRequireCurrentTime>([this]() {
    OnDisplayTypeChanged(WebMediaPlayer::DisplayType::kFullscreen);
    OnDisplayTypeChanged(WebMediaPlayer::DisplayType::kInline);
  });
}

TEST_P(WatchTimeReporterTest, OnDisplayTypeChangeHysteresisNativeOffFinalized) {
  RunHysteresisTest<kAccumulationContinuesAfterTest |
                    kFinalizeDisplayWatchTime>([this]() {
    OnDisplayTypeChanged(WebMediaPlayer::DisplayType::kFullscreen);
  });
}

TEST_P(WatchTimeReporterTest, OnDisplayTypeChangeInlineToFullscreen) {
  RunHysteresisTest<kAccumulationContinuesAfterTest |
                    kFinalizeDisplayWatchTime | kStartWithDisplayFullscreen |
                    kTransitionDisplayWatchTime>(
      [this]() { OnDisplayTypeChanged(WebMediaPlayer::DisplayType::kInline); });
}

TEST_P(WatchTimeReporterTest, OnDisplayTypeChangeFullscreenToInline) {
  RunHysteresisTest<kAccumulationContinuesAfterTest |
                    kFinalizeDisplayWatchTime | kTransitionDisplayWatchTime>(
      [this]() {
        OnDisplayTypeChanged(WebMediaPlayer::DisplayType::kFullscreen);
      });
}

// Tests that the first finalize is the only one that matters.
TEST_P(WatchTimeReporterTest, HysteresisFinalizedWithEarliest) {
  RunHysteresisTest([this]() {
    wtr_->OnPaused();

    // These subsequent "stop events" should do nothing since a finalize time
    // has already been selected.
    wtr_->OnHidden();
    wtr_->OnVolumeChange(0);
  });
}

// Tests that if a stop, stop, start sequence occurs, the middle stop is not
// undone and thus finalize still occurs.
TEST_P(WatchTimeReporterTest, HysteresisPartialExitStillFinalizes) {
  auto stop_event = [this](size_t i) {
    if (i == 0) {
      wtr_->OnPaused();
    } else if (i == 1) {
      wtr_->OnVolumeChange(0);
    } else {
      ASSERT_TRUE(has_video_);
      wtr_->OnHidden();
    }
  };

  auto start_event = [this](size_t i) {
    if (i == 0) {
      wtr_->OnPlaying();
    } else if (i == 1) {
      wtr_->OnVolumeChange(1);
    } else {
      ASSERT_TRUE(has_video_);
      wtr_->OnShown();
    }
  };

  const size_t kTestSize = has_video_ ? 3 : 2;
  for (size_t i = 0; i < kTestSize; ++i) {
    for (size_t j = 0; j < kTestSize; ++j) {
      if (i == j)
        continue;

      RunHysteresisTest<kFinalizeInterleavedStartEvent>(
          [i, j, start_event, stop_event]() {
            stop_event(i);
            stop_event(j);
            start_event(i);
          });
    }
  }
}

INSTANTIATE_TEST_CASE_P(WatchTimeReporterTest,
                        WatchTimeReporterTest,
                        testing::Values(true, false));

}  // namespace media
