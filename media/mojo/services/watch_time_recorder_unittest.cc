// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/watch_time_recorder.h"

#include <stddef.h>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/test/histogram_tester.h"
#include "base/test/test_message_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/ukm/test_ukm_recorder.h"
#include "media/base/watch_time_keys.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using UkmEntry = ukm::builders::Media_BasicPlayback;

namespace media {

constexpr char kTestOrigin[] = "https://test.google.com/";

class WatchTimeRecorderTest : public testing::Test {
 public:
  WatchTimeRecorderTest()
      : computation_keys_(
            {WatchTimeKey::kAudioSrc, WatchTimeKey::kAudioMse,
             WatchTimeKey::kAudioEme, WatchTimeKey::kAudioVideoSrc,
             WatchTimeKey::kAudioVideoMse, WatchTimeKey::kAudioVideoEme}),
        mtbr_keys_({kMeanTimeBetweenRebuffersAudioSrc,
                    kMeanTimeBetweenRebuffersAudioMse,
                    kMeanTimeBetweenRebuffersAudioEme,
                    kMeanTimeBetweenRebuffersAudioVideoSrc,
                    kMeanTimeBetweenRebuffersAudioVideoMse,
                    kMeanTimeBetweenRebuffersAudioVideoEme}),
        smooth_keys_({kRebuffersCountAudioSrc, kRebuffersCountAudioMse,
                      kRebuffersCountAudioEme, kRebuffersCountAudioVideoSrc,
                      kRebuffersCountAudioVideoMse,
                      kRebuffersCountAudioVideoEme}) {
    ResetMetricRecorders();
    WatchTimeRecorder::CreateWatchTimeRecorderProvider(
        mojo::MakeRequest(&provider_));
  }

  ~WatchTimeRecorderTest() override { base::RunLoop().RunUntilIdle(); }

  void Initialize(bool has_audio,
                  bool has_video,
                  bool is_mse,
                  bool is_encrypted) {
    provider_->AcquireWatchTimeRecorder(
        mojom::PlaybackProperties::New(
            kUnknownAudioCodec, kUnknownVideoCodec, has_audio, has_video,
            is_mse, is_encrypted, false, gfx::Size(800, 600),
            url::Origin::Create(GURL(kTestOrigin)), true /* is_top_frame */),
        mojo::MakeRequest(&wtr_));
  }

  void ExpectWatchTime(const std::vector<base::StringPiece>& keys,
                       base::TimeDelta value) {
    for (int i = 0; i <= static_cast<int>(WatchTimeKey::kWatchTimeKeyMax);
         ++i) {
      const base::StringPiece test_key =
          WatchTimeKeyToString(static_cast<WatchTimeKey>(i));
      auto it = std::find(keys.begin(), keys.end(), test_key);
      if (it == keys.end()) {
        histogram_tester_->ExpectTotalCount(test_key.as_string(), 0);
      } else {
        histogram_tester_->ExpectUniqueSample(test_key.as_string(),
                                              value.InMilliseconds(), 1);
      }
    }
  }

  void ExpectHelper(const std::vector<base::StringPiece>& full_key_list,
                    const std::vector<base::StringPiece>& keys,
                    int64_t value) {
    for (auto key : full_key_list) {
      auto it = std::find(keys.begin(), keys.end(), key);
      if (it == keys.end())
        histogram_tester_->ExpectTotalCount(key.as_string(), 0);
      else
        histogram_tester_->ExpectUniqueSample(key.as_string(), value, 1);
    }
  }

  void ExpectMtbrTime(const std::vector<base::StringPiece>& keys,
                      base::TimeDelta value) {
    ExpectHelper(mtbr_keys_, keys, value.InMilliseconds());
  }

  void ExpectZeroRebuffers(const std::vector<base::StringPiece>& keys) {
    ExpectHelper(smooth_keys_, keys, 0);
  }

  void ExpectRebuffers(const std::vector<base::StringPiece>& keys, int count) {
    ExpectHelper(smooth_keys_, keys, count);
  }

  void ExpectUkmWatchTime(const std::vector<base::StringPiece>& keys,
                          base::TimeDelta value) {
    if (keys.empty()) {
      ASSERT_EQ(0u, test_recorder_->sources_count());
      ASSERT_EQ(0u, test_recorder_->entries_count());
      return;
    }

    ASSERT_EQ(1u, test_recorder_->sources_count());

    const auto* source = test_recorder_->GetSourceForUrl(kTestOrigin);
    ASSERT_TRUE(source);

    for (auto key : keys) {
      test_recorder_->ExpectMetric(*source, UkmEntry::kEntryName, key.data(),
                                   value.InMilliseconds());
    }
  }

  void ResetMetricRecorders() {
    histogram_tester_.reset(new base::HistogramTester());
    // Ensure cleared global before attempting to create a new TestUkmReporter.
    test_recorder_.reset();
    test_recorder_.reset(new ukm::TestAutoSetUkmRecorder());
  }

  MOCK_METHOD0(GetCurrentMediaTime, base::TimeDelta());

 protected:
  base::MessageLoop message_loop_;
  mojom::WatchTimeRecorderProviderPtr provider_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> test_recorder_;
  mojom::WatchTimeRecorderPtr wtr_;
  const std::vector<WatchTimeKey> computation_keys_;
  const std::vector<base::StringPiece> mtbr_keys_;
  const std::vector<base::StringPiece> smooth_keys_;

  DISALLOW_COPY_AND_ASSIGN(WatchTimeRecorderTest);
};

TEST_F(WatchTimeRecorderTest, TestBasicReporting) {
  constexpr base::TimeDelta kWatchTime1 = base::TimeDelta::FromSeconds(25);
  constexpr base::TimeDelta kWatchTime2 = base::TimeDelta::FromSeconds(50);

  // Don't include kWatchTimeKeyMax, since we'll use that as a placeholder to
  // ensure only the keys requested for finalize are finalized.
  const base::StringPiece kLastKey =
      WatchTimeKeyToString(WatchTimeKey::kWatchTimeKeyMax);
  for (int i = 0; i < static_cast<int>(WatchTimeKey::kWatchTimeKeyMax); ++i) {
    const WatchTimeKey key = static_cast<WatchTimeKey>(i);
    SCOPED_TRACE(WatchTimeKeyToString(key));

    Initialize(true, false, true, true);
    wtr_->RecordWatchTime(WatchTimeKey::kWatchTimeKeyMax, kWatchTime1);
    wtr_->RecordWatchTime(key, kWatchTime1);
    wtr_->RecordWatchTime(key, kWatchTime2);
    base::RunLoop().RunUntilIdle();

    // Nothing should be recorded yet since we haven't finalized.
    ExpectWatchTime({}, base::TimeDelta());

    // Only the requested key should be finalized.
    wtr_->FinalizeWatchTime({key});
    base::RunLoop().RunUntilIdle();

    const base::StringPiece key_str = WatchTimeKeyToString(key);
    ExpectWatchTime({key_str}, kWatchTime2);

    // These keys are only reported for a full finalize.
    ExpectMtbrTime({}, base::TimeDelta());
    ExpectZeroRebuffers({});
    ExpectUkmWatchTime({}, base::TimeDelta());

    // Verify our sentinel key is recorded properly at player destruction.
    ResetMetricRecorders();
    wtr_.reset();
    base::RunLoop().RunUntilIdle();

    ExpectWatchTime({kLastKey}, kWatchTime1);
    ExpectMtbrTime({}, base::TimeDelta());
    ExpectZeroRebuffers({});

    if (key == WatchTimeKey::kAudioAll || key == WatchTimeKey::kAudioVideoAll ||
        key == WatchTimeKey::kAudioVideoBackgroundAll) {
      ExpectUkmWatchTime({UkmEntry::kWatchTimeName}, kWatchTime2);
    } else {
      ExpectUkmWatchTime({}, base::TimeDelta());
    }

    ResetMetricRecorders();
  }
}

TEST_F(WatchTimeRecorderTest, FinalizeWithoutWatchTime) {
  Initialize(true, true, false, true);
  wtr_.reset();
  base::RunLoop().RunUntilIdle();

  // No watch time should have been recorded even though a finalize event will
  // be sent.
  ExpectWatchTime({}, base::TimeDelta());
  ExpectMtbrTime({}, base::TimeDelta());
  ExpectZeroRebuffers({});
  ExpectUkmWatchTime({}, base::TimeDelta());
  ASSERT_EQ(0U, test_recorder_->sources_count());
}

TEST_F(WatchTimeRecorderTest, TestRebufferingMetrics) {
  Initialize(true, false, true, true);

  constexpr base::TimeDelta kWatchTime = base::TimeDelta::FromSeconds(50);
  for (auto key : computation_keys_)
    wtr_->RecordWatchTime(key, kWatchTime);
  wtr_->UpdateUnderflowCount(1);
  wtr_->UpdateUnderflowCount(2);

  // Trigger finalization of everything.
  wtr_->FinalizeWatchTime({});
  base::RunLoop().RunUntilIdle();

  ExpectMtbrTime(mtbr_keys_, kWatchTime / 2);
  ExpectRebuffers(smooth_keys_, 2);

  // Now rerun the test without any rebuffering.
  ResetMetricRecorders();
  for (auto key : computation_keys_)
    wtr_->RecordWatchTime(key, kWatchTime);
  wtr_->FinalizeWatchTime({});
  base::RunLoop().RunUntilIdle();

  ExpectMtbrTime({}, base::TimeDelta());
  ExpectZeroRebuffers(smooth_keys_);
}

#define EXPECT_UKM(name, value) \
  test_recorder_->ExpectMetric(*source, UkmEntry::kEntryName, name, value)
#define EXPECT_NO_UKM(name) \
  EXPECT_FALSE(test_recorder_->HasMetric(*source, UkmEntry::kEntryName, name))

TEST_F(WatchTimeRecorderTest, BasicUkmAudioVideo) {
  mojom::PlaybackPropertiesPtr properties = mojom::PlaybackProperties::New(
      kCodecAAC, kCodecH264, true, true, false, false, false,
      gfx::Size(800, 600), url::Origin::Create(GURL(kTestOrigin)), true);
  provider_->AcquireWatchTimeRecorder(properties.Clone(),
                                      mojo::MakeRequest(&wtr_));

  constexpr base::TimeDelta kWatchTime = base::TimeDelta::FromSeconds(54);
  wtr_->RecordWatchTime(WatchTimeKey::kAudioVideoAll, kWatchTime);
  wtr_.reset();
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(1u, test_recorder_->sources_count());
  auto* source = test_recorder_->GetSourceForUrl(kTestOrigin);
  ASSERT_TRUE(source);

  EXPECT_UKM(UkmEntry::kWatchTimeName, kWatchTime.InMilliseconds());
  EXPECT_UKM(UkmEntry::kIsBackgroundName, false);
  EXPECT_UKM(UkmEntry::kAudioCodecName, properties->audio_codec);
  EXPECT_UKM(UkmEntry::kVideoCodecName, properties->video_codec);
  EXPECT_UKM(UkmEntry::kHasAudioName, properties->has_audio);
  EXPECT_UKM(UkmEntry::kHasVideoName, properties->has_video);
  EXPECT_UKM(UkmEntry::kIsEMEName, properties->is_eme);
  EXPECT_UKM(UkmEntry::kIsMSEName, properties->is_mse);
  EXPECT_UKM(UkmEntry::kLastPipelineStatusName, PIPELINE_OK);
  EXPECT_UKM(UkmEntry::kRebuffersCountName, 0);
  EXPECT_UKM(UkmEntry::kVideoNaturalWidthName,
             properties->natural_size.width());
  EXPECT_UKM(UkmEntry::kVideoNaturalHeightName,
             properties->natural_size.height());

  EXPECT_NO_UKM(UkmEntry::kMeanTimeBetweenRebuffersName);
  EXPECT_NO_UKM(UkmEntry::kWatchTime_ACName);
  EXPECT_NO_UKM(UkmEntry::kWatchTime_BatteryName);
  EXPECT_NO_UKM(UkmEntry::kWatchTime_NativeControlsOnName);
  EXPECT_NO_UKM(UkmEntry::kWatchTime_NativeControlsOffName);
  EXPECT_NO_UKM(UkmEntry::kWatchTime_DisplayFullscreenName);
  EXPECT_NO_UKM(UkmEntry::kWatchTime_DisplayInlineName);
  EXPECT_NO_UKM(UkmEntry::kWatchTime_DisplayPictureInPictureName);
}

TEST_F(WatchTimeRecorderTest, BasicUkmAudioVideoWithExtras) {
  mojom::PlaybackPropertiesPtr properties = mojom::PlaybackProperties::New(
      kCodecOpus, kCodecVP9, true, true, true, true, false, gfx::Size(800, 600),
      url::Origin::Create(GURL(kTestOrigin)), true);
  provider_->AcquireWatchTimeRecorder(properties.Clone(),
                                      mojo::MakeRequest(&wtr_));

  constexpr base::TimeDelta kWatchTime = base::TimeDelta::FromSeconds(54);
  const base::TimeDelta kWatchTime2 = kWatchTime * 2;
  const base::TimeDelta kWatchTime3 = kWatchTime / 3;
  wtr_->RecordWatchTime(WatchTimeKey::kAudioVideoAll, kWatchTime2);
  wtr_->RecordWatchTime(WatchTimeKey::kAudioVideoAc, kWatchTime);

  // Ensure partial finalize does not affect final report.
  wtr_->FinalizeWatchTime({WatchTimeKey::kAudioVideoAc});
  wtr_->RecordWatchTime(WatchTimeKey::kAudioVideoBattery, kWatchTime);
  wtr_->RecordWatchTime(WatchTimeKey::kAudioVideoNativeControlsOn, kWatchTime);
  wtr_->FinalizeWatchTime({WatchTimeKey::kAudioVideoNativeControlsOn});
  wtr_->RecordWatchTime(WatchTimeKey::kAudioVideoNativeControlsOff, kWatchTime);
  wtr_->RecordWatchTime(WatchTimeKey::kAudioVideoDisplayFullscreen,
                        kWatchTime3);
  wtr_->FinalizeWatchTime({WatchTimeKey::kAudioVideoDisplayFullscreen});
  wtr_->RecordWatchTime(WatchTimeKey::kAudioVideoDisplayInline, kWatchTime3);
  wtr_->FinalizeWatchTime({WatchTimeKey::kAudioVideoDisplayInline});
  wtr_->RecordWatchTime(WatchTimeKey::kAudioVideoDisplayPictureInPicture,
                        kWatchTime3);
  wtr_->UpdateUnderflowCount(3);
  wtr_->OnError(PIPELINE_ERROR_DECODE);

  wtr_.reset();
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(1u, test_recorder_->sources_count());
  auto* source = test_recorder_->GetSourceForUrl(kTestOrigin);
  ASSERT_TRUE(source);

  EXPECT_UKM(UkmEntry::kWatchTimeName, kWatchTime2.InMilliseconds());
  EXPECT_UKM(UkmEntry::kWatchTime_ACName, kWatchTime.InMilliseconds());
  EXPECT_UKM(UkmEntry::kWatchTime_BatteryName, kWatchTime.InMilliseconds());
  EXPECT_UKM(UkmEntry::kWatchTime_NativeControlsOnName,
             kWatchTime.InMilliseconds());
  EXPECT_UKM(UkmEntry::kWatchTime_NativeControlsOffName,
             kWatchTime.InMilliseconds());
  EXPECT_UKM(UkmEntry::kWatchTime_DisplayFullscreenName,
             kWatchTime3.InMilliseconds());
  EXPECT_UKM(UkmEntry::kWatchTime_DisplayInlineName,
             kWatchTime3.InMilliseconds());
  EXPECT_UKM(UkmEntry::kWatchTime_DisplayPictureInPictureName,
             kWatchTime3.InMilliseconds());
  EXPECT_UKM(UkmEntry::kMeanTimeBetweenRebuffersName,
             kWatchTime2.InMilliseconds() / 3);

  EXPECT_UKM(UkmEntry::kIsBackgroundName, false);
  EXPECT_UKM(UkmEntry::kAudioCodecName, properties->audio_codec);
  EXPECT_UKM(UkmEntry::kVideoCodecName, properties->video_codec);
  EXPECT_UKM(UkmEntry::kHasAudioName, properties->has_audio);
  EXPECT_UKM(UkmEntry::kHasVideoName, properties->has_video);
  EXPECT_UKM(UkmEntry::kIsEMEName, properties->is_eme);
  EXPECT_UKM(UkmEntry::kIsMSEName, properties->is_mse);
  EXPECT_UKM(UkmEntry::kLastPipelineStatusName, PIPELINE_ERROR_DECODE);
  EXPECT_UKM(UkmEntry::kRebuffersCountName, 3);
  EXPECT_UKM(UkmEntry::kVideoNaturalWidthName,
             properties->natural_size.width());
  EXPECT_UKM(UkmEntry::kVideoNaturalHeightName,
             properties->natural_size.height());
}

TEST_F(WatchTimeRecorderTest, BasicUkmAudioVideoBackground) {
  mojom::PlaybackPropertiesPtr properties = mojom::PlaybackProperties::New(
      kCodecAAC, kCodecH264, true, true, false, false, false,
      gfx::Size(800, 600), url::Origin::Create(GURL(kTestOrigin)), true);
  provider_->AcquireWatchTimeRecorder(properties.Clone(),
                                      mojo::MakeRequest(&wtr_));

  constexpr base::TimeDelta kWatchTime = base::TimeDelta::FromSeconds(54);
  wtr_->RecordWatchTime(WatchTimeKey::kAudioVideoBackgroundAll, kWatchTime);
  wtr_.reset();
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(1u, test_recorder_->sources_count());
  auto* source = test_recorder_->GetSourceForUrl(kTestOrigin);
  ASSERT_TRUE(source);

  EXPECT_UKM(UkmEntry::kWatchTimeName, kWatchTime.InMilliseconds());
  EXPECT_UKM(UkmEntry::kIsBackgroundName, true);
  EXPECT_UKM(UkmEntry::kAudioCodecName, properties->audio_codec);
  EXPECT_UKM(UkmEntry::kVideoCodecName, properties->video_codec);
  EXPECT_UKM(UkmEntry::kHasAudioName, properties->has_audio);
  EXPECT_UKM(UkmEntry::kHasVideoName, properties->has_video);
  EXPECT_UKM(UkmEntry::kIsEMEName, properties->is_eme);
  EXPECT_UKM(UkmEntry::kIsMSEName, properties->is_mse);
  EXPECT_UKM(UkmEntry::kLastPipelineStatusName, PIPELINE_OK);
  EXPECT_UKM(UkmEntry::kRebuffersCountName, 0);
  EXPECT_UKM(UkmEntry::kVideoNaturalWidthName,
             properties->natural_size.width());
  EXPECT_UKM(UkmEntry::kVideoNaturalHeightName,
             properties->natural_size.height());

  EXPECT_NO_UKM(UkmEntry::kMeanTimeBetweenRebuffersName);
  EXPECT_NO_UKM(UkmEntry::kWatchTime_ACName);
  EXPECT_NO_UKM(UkmEntry::kWatchTime_BatteryName);
  EXPECT_NO_UKM(UkmEntry::kWatchTime_NativeControlsOnName);
  EXPECT_NO_UKM(UkmEntry::kWatchTime_NativeControlsOffName);
  EXPECT_NO_UKM(UkmEntry::kWatchTime_DisplayFullscreenName);
  EXPECT_NO_UKM(UkmEntry::kWatchTime_DisplayInlineName);
  EXPECT_NO_UKM(UkmEntry::kWatchTime_DisplayPictureInPictureName);
}
#undef EXPECT_UKM
#undef EXPECT_NO_UKM

}  // namespace media
