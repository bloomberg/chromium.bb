// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <string>

#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/task_scheduler/post_task.h"
#include "base/test/scoped_task_environment.h"
#include "media/mojo/services/video_decode_perf_history.h"
#include "media/mojo/services/video_decode_stats_db.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

// Aliases for readability.
const bool kIsSmooth = true;
const bool kIsNotSmooth = false;
const bool kIsPowerEfficient = true;
const bool kIsNotPowerEfficient = false;

class FakeVideoDecodeStatsDB : public VideoDecodeStatsDB {
 public:
  FakeVideoDecodeStatsDB() = default;
  ~FakeVideoDecodeStatsDB() override {}

  // Call CompleteInitialize(...) to run |init_cb| callback.
  void Initialize(base::OnceCallback<void(bool)> init_cb) override {
    pendnding_init_cb_ = std::move(init_cb);
  }

  // Completes fake initialization, running |init_cb| with the supplied value
  // for success.
  void CompleteInitialize(bool success) {
    std::move(pendnding_init_cb_).Run(success);
  }

  void AppendDecodeStats(const VideoDescKey& key,
                         const DecodeStatsEntry& new_entry) override {
    std::string key_str = MakeKeyString(key);
    if (entries_.find(key_str) == entries_.end()) {
      entries_.emplace(std::make_pair(key_str, new_entry));
    } else {
      const DecodeStatsEntry& known_entry = entries_.at(key_str);
      entries_.at(key_str) = DecodeStatsEntry(
          known_entry.frames_decoded + new_entry.frames_decoded,
          known_entry.frames_dropped + new_entry.frames_dropped,
          known_entry.frames_decoded_power_efficient +
              new_entry.frames_decoded_power_efficient);
    }
  }

  void GetDecodeStats(const VideoDescKey& key,
                      GetDecodeStatsCB callback) override {
    auto entry_it = entries_.find(MakeKeyString(key));
    if (entry_it == entries_.end()) {
      std::move(callback).Run(true, nullptr);
    } else {
      std::move(callback).Run(
          true, base::MakeUnique<DecodeStatsEntry>(entry_it->second));
    }
  }

 private:
  static std::string MakeKeyString(const VideoDescKey& key) {
    return base::StringPrintf("%d|%s|%d", static_cast<int>(key.codec_profile),
                              key.size.ToString().c_str(), key.frame_rate);
  }

  std::map<std::string, DecodeStatsEntry> entries_;

  base::OnceCallback<void(bool)> pendnding_init_cb_;
};

// Simple factory that always returns the pointer its given. The lifetime of
// |db| is managed by the CreateDB() caller.
class FakeVideoDecodeStatsDBFactory : public VideoDecodeStatsDBFactory {
 public:
  FakeVideoDecodeStatsDBFactory(FakeVideoDecodeStatsDB* db) : db_(db) {}
  ~FakeVideoDecodeStatsDBFactory() override = default;

  std::unique_ptr<VideoDecodeStatsDB> CreateDB() override {
    return std::unique_ptr<VideoDecodeStatsDB>(db_);
  }

 private:
  FakeVideoDecodeStatsDB* db_;
};

// When bool param is true, tests should wait until the end to run
// database_->CompleteInitialize(). Otherwise run CompleteInitialize() at the
// test start.
class VideoDecodePerfHistoryTest : public testing::TestWithParam<bool> {
 public:
  void SetUp() override {
    // Sniff the database pointer so tests can inject entries. The factory will
    // return this pointer when requested by VideoDecodePerfHistory.
    database_ = new FakeVideoDecodeStatsDB();

    // Make tests hermetic by creating a new instance. Cannot use unique_ptr
    // here because the constructor/destructor are private and only accessible
    // to this test via friending.
    perf_history_ = new VideoDecodePerfHistory(
        std::make_unique<FakeVideoDecodeStatsDBFactory>(database_));
  }

  void TearDown() override {
    // Avoid leaking between tests.
    delete perf_history_;
    perf_history_ = nullptr;

    // |perf_history_| owns the |database_| and will delete it.
    database_ = nullptr;
  }

  void PreInitializeDB(bool initialize_success) {
    // Invoke private method to start initialization. Usually invoked by first
    // API call requiring DB access.
    perf_history_->InitDatabase();
    // Complete initialization by firing callback from our fake DB.
    database_->CompleteInitialize(initialize_success);
  }

  // Tests may set this as the callback for VideoDecodePerfHistory::GetPerfInfo
  // to check the results of the call.
  MOCK_METHOD2(MockGetPerfInfoCB,
               void(bool is_smooth, bool is_power_efficient));

 protected:
  using VideoDescKey = VideoDecodeStatsDB::VideoDescKey;
  using DecodeStatsEntry = VideoDecodeStatsDB::DecodeStatsEntry;

  static constexpr double kMaxSmoothDroppedFramesPercent =
      VideoDecodePerfHistory::kMaxSmoothDroppedFramesPercent;
  static constexpr double kMinPowerEfficientDecodedFramePercent =
      VideoDecodePerfHistory::kMinPowerEfficientDecodedFramePercent;

  base::test::ScopedTaskEnvironment scoped_task_environment_;

  // The VideoDecodeStatsReporter being tested.
  VideoDecodePerfHistory* perf_history_;

  // The database |perf_history_| uses to store/query performance stats.
  FakeVideoDecodeStatsDB* database_;
};

TEST_P(VideoDecodePerfHistoryTest, GetPerfInfo_Smooth) {
  // NOTE: The when the DB initialization is deferred, All EXPECT_CALLs are then
  // delayed until we db_->CompleteInitialize(). testing::InSequence enforces
  // that EXPECT_CALLs arrive in top-to-bottom order.
  bool defer_initialize = GetParam();
  testing::InSequence dummy;

  // Complete initialization in advance of API calls when not asked to defer.
  if (!defer_initialize)
    PreInitializeDB(/* success */ true);

  // First add 2 records to the history. The second record has a higher frame
  // rate and a higher number of dropped frames such that it is "not smooth".
  const VideoCodecProfile kKnownProfile = VP9PROFILE_PROFILE0;
  const gfx::Size kKownSize(100, 200);
  const int kSmoothFrameRate = 30;
  const int kNotSmoothFrameRate = 90;
  const int kFramesDecoded = 1000;
  const int kNotPowerEfficientFramesDecoded = 0;
  // Sets the ratio of dropped frames to barely qualify as smooth.
  const int kSmoothFramesDropped =
      kFramesDecoded * kMaxSmoothDroppedFramesPercent;
  // Set the ratio of dropped frames to barely qualify as NOT smooth.
  const int kNotSmoothFramesDropped =
      kFramesDecoded * kMaxSmoothDroppedFramesPercent + 1;

  // Add the entries.
  perf_history_->SavePerfRecord(kKnownProfile, kKownSize, kSmoothFrameRate,
                                kFramesDecoded, kSmoothFramesDropped,
                                kNotPowerEfficientFramesDecoded);
  perf_history_->SavePerfRecord(kKnownProfile, kKownSize, kNotSmoothFrameRate,
                                kFramesDecoded, kNotSmoothFramesDropped,
                                kNotPowerEfficientFramesDecoded);

  // Verify perf history returns is_smooth = true for the smooth entry.
  EXPECT_CALL(*this, MockGetPerfInfoCB(kIsSmooth, kIsNotPowerEfficient));
  perf_history_->GetPerfInfo(
      kKnownProfile, kKownSize, kSmoothFrameRate,
      base::BindOnce(&VideoDecodePerfHistoryTest::MockGetPerfInfoCB,
                     base::Unretained(this)));

  // Verify perf history returns is_smooth = false for the NOT smooth entry.
  EXPECT_CALL(*this, MockGetPerfInfoCB(kIsNotSmooth, kIsNotPowerEfficient));
  perf_history_->GetPerfInfo(
      kKnownProfile, kKownSize, kNotSmoothFrameRate,
      base::BindOnce(&VideoDecodePerfHistoryTest::MockGetPerfInfoCB,
                     base::Unretained(this)));

  // Verify perf history optimistically returns is_smooth = true when no entry
  // can be found with the given configuration.
  const VideoCodecProfile kUnknownProfile = VP9PROFILE_PROFILE2;
  EXPECT_CALL(*this, MockGetPerfInfoCB(kIsSmooth, kIsPowerEfficient));
  perf_history_->GetPerfInfo(
      kUnknownProfile, kKownSize, kNotSmoothFrameRate,
      base::BindOnce(&VideoDecodePerfHistoryTest::MockGetPerfInfoCB,
                     base::Unretained(this)));

  // Complete successful deferred DB initialization (see comment at top of test)
  if (defer_initialize) {
    database_->CompleteInitialize(true);

    // Allow initialize-deferred API calls to complete.
    base::RunLoop run_loop;
    base::PostTask(FROM_HERE, base::BindOnce(run_loop.QuitWhenIdleClosure()));
    run_loop.Run();
  }
}

TEST_P(VideoDecodePerfHistoryTest, GetPerfInfo_PowerEfficient) {
  // NOTE: The when the DB initialization is deferred, All EXPECT_CALLs are then
  // delayed until we db_->CompleteInitialize(). testing::InSequence enforces
  // that EXPECT_CALLs arrive in top-to-bottom order.
  bool defer_initialize = GetParam();
  testing::InSequence dummy;

  // Complete initialization in advance of API calls when not asked to defer.
  if (!defer_initialize)
    PreInitializeDB(/* success */ true);

  // First add 3 records to the history:
  // - the first has a high number of power efficiently decoded frames;
  // - the second has a low number of power efficiently decoded frames;
  // - the third is similar to the first with a high number of dropped frames.
  const VideoCodecProfile kPowerEfficientProfile = VP9PROFILE_PROFILE0;
  const VideoCodecProfile kNotPowerEfficientProfile = VP8PROFILE_ANY;
  const gfx::Size kKownSize(100, 200);
  const int kSmoothFrameRate = 30;
  const int kNotSmoothFrameRate = 90;
  const int kFramesDecoded = 1000;
  const int kPowerEfficientFramesDecoded =
      kFramesDecoded * kMinPowerEfficientDecodedFramePercent;
  const int kNotPowerEfficientFramesDecoded =
      kFramesDecoded * kMinPowerEfficientDecodedFramePercent - 1;
  // Sets the ratio of dropped frames to barely qualify as smooth.
  const int kSmoothFramesDropped =
      kFramesDecoded * kMaxSmoothDroppedFramesPercent;
  // Set the ratio of dropped frames to barely qualify as NOT smooth.
  const int kNotSmoothFramesDropped =
      kFramesDecoded * kMaxSmoothDroppedFramesPercent + 1;

  // Add the entries.
  perf_history_->SavePerfRecord(
      kPowerEfficientProfile, kKownSize, kSmoothFrameRate, kFramesDecoded,
      kSmoothFramesDropped, kPowerEfficientFramesDecoded);
  perf_history_->SavePerfRecord(
      kNotPowerEfficientProfile, kKownSize, kSmoothFrameRate, kFramesDecoded,
      kSmoothFramesDropped, kNotPowerEfficientFramesDecoded);
  perf_history_->SavePerfRecord(
      kPowerEfficientProfile, kKownSize, kNotSmoothFrameRate, kFramesDecoded,
      kNotSmoothFramesDropped, kPowerEfficientFramesDecoded);

  // Verify perf history returns is_smooth = true, is_power_efficient = true.
  EXPECT_CALL(*this, MockGetPerfInfoCB(kIsSmooth, kIsPowerEfficient));
  perf_history_->GetPerfInfo(
      kPowerEfficientProfile, kKownSize, kSmoothFrameRate,
      base::BindOnce(&VideoDecodePerfHistoryTest::MockGetPerfInfoCB,
                     base::Unretained(this)));

  // Verify perf history returns is_smooth = true, is_power_efficient = false.
  EXPECT_CALL(*this, MockGetPerfInfoCB(kIsSmooth, kIsNotPowerEfficient));
  perf_history_->GetPerfInfo(
      kNotPowerEfficientProfile, kKownSize, kSmoothFrameRate,
      base::BindOnce(&VideoDecodePerfHistoryTest::MockGetPerfInfoCB,
                     base::Unretained(this)));

  // Verify perf history returns is_smooth = false, is_power_efficient = true.
  EXPECT_CALL(*this, MockGetPerfInfoCB(kIsNotSmooth, kIsPowerEfficient));
  perf_history_->GetPerfInfo(
      kPowerEfficientProfile, kKownSize, kNotSmoothFrameRate,
      base::BindOnce(&VideoDecodePerfHistoryTest::MockGetPerfInfoCB,
                     base::Unretained(this)));

  // Verify perf history optimistically returns is_smooth = true and
  // is_power_efficient = true when no entry can be found with the given
  // configuration.
  const VideoCodecProfile kUnknownProfile = VP9PROFILE_PROFILE2;
  EXPECT_CALL(*this, MockGetPerfInfoCB(kIsSmooth, kIsPowerEfficient));
  perf_history_->GetPerfInfo(
      kUnknownProfile, kKownSize, kNotSmoothFrameRate,
      base::BindOnce(&VideoDecodePerfHistoryTest::MockGetPerfInfoCB,
                     base::Unretained(this)));

  // Complete successful deferred DB initialization (see comment at top of test)
  if (defer_initialize) {
    database_->CompleteInitialize(true);

    // Allow initialize-deferred API calls to complete.
    base::RunLoop run_loop;
    base::PostTask(FROM_HERE, base::BindOnce(run_loop.QuitWhenIdleClosure()));
    run_loop.Run();
  }
}

TEST_P(VideoDecodePerfHistoryTest, GetPerfInfo_FailedInitialize) {
  bool defer_initialize = GetParam();
  // Fail initialization in advance of API calls when not asked to defer.
  if (!defer_initialize)
    PreInitializeDB(/* success */ false);

  const VideoCodecProfile kProfile = VP9PROFILE_PROFILE0;
  const gfx::Size kSize(100, 200);
  const int kFrameRate = 30;

  // When initialization fails, callback should optimistically claim both smooth
  // and power efficient performance.
  EXPECT_CALL(*this, MockGetPerfInfoCB(kIsSmooth, kIsPowerEfficient));
  perf_history_->GetPerfInfo(
      kProfile, kSize, kFrameRate,
      base::BindOnce(&VideoDecodePerfHistoryTest::MockGetPerfInfoCB,
                     base::Unretained(this)));

  // Fail deferred DB initialization (see comment at top of test).
  if (defer_initialize) {
    database_->CompleteInitialize(false);

    // Allow initialize-deferred API calls to complete.
    base::RunLoop run_loop;
    base::PostTask(FROM_HERE, base::BindOnce(run_loop.QuitWhenIdleClosure()));
    run_loop.Run();
  }
}

INSTANTIATE_TEST_CASE_P(VaryDBInitTiming,
                        VideoDecodePerfHistoryTest,
                        ::testing::Values(true, false));

}  // namespace media
