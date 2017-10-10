// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <sstream>
#include <string>

#include "base/memory/ptr_util.h"
#include "media/mojo/services/video_decode_perf_history.h"
#include "media/mojo/services/video_decode_stats_db.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

class FakeVideoDecodeStatsDB : public VideoDecodeStatsDB {
 public:
  FakeVideoDecodeStatsDB() = default;
  ~FakeVideoDecodeStatsDB() override {}

  void AppendDecodeStats(const VideoDescKey& key,
                         const DecodeStatsEntry& entry) override {
    std::string key_str = MakeKeyString(key);
    if (entries_.find(key_str) == entries_.end()) {
      entries_.emplace(std::make_pair(key_str, entry));
    } else {
      const DecodeStatsEntry& known_entry = entries_.at(key_str);
      uint32_t frames_decoded =
          known_entry.frames_decoded + known_entry.frames_decoded;
      uint32_t frames_dropped =
          known_entry.frames_dropped + known_entry.frames_dropped;
      entries_.at(key_str) = DecodeStatsEntry(frames_decoded, frames_dropped);
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
    std::stringstream ss;
    ss << key.codec_profile << "|" << key.size.ToString() << "|"
       << key.frame_rate;
    return ss.str();
  }

  std::map<std::string, DecodeStatsEntry> entries_;
};

class VideoDecodePerfHistoryTest : public ::testing::Test {
 public:
  void SetUp() override {
    // Sniff the database pointer so tests can inject entries.
    database_ = base::MakeUnique<FakeVideoDecodeStatsDB>();
    VideoDecodePerfHistory::Initialize(database_.get());

    // Make tests hermetic by creating a new instance. Cannot use unique_ptr
    // here because the constructor/destructor are private and only accessible
    // to this test via friending.
    perf_history_ = new VideoDecodePerfHistory();
  }

  void TearDown() override {
    // Avoid leaking between tests.
    delete perf_history_;
    perf_history_ = nullptr;
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

  // The VideoDecodeStatsReporter being tested.
  VideoDecodePerfHistory* perf_history_;

  // The database |perf_history_| uses to store/query performance stats.
  std::unique_ptr<FakeVideoDecodeStatsDB> database_;
};

TEST_F(VideoDecodePerfHistoryTest, GetPerfInfo_Smooth) {
  // Prepare database with 2 entries. The second entry has a higher framerate
  // and a higher number of dropped frames such that it is "not smooth".
  const VideoCodecProfile kKnownProfile = VP9PROFILE_PROFILE0;
  const gfx::Size kKownSize(100, 200);
  const int kSmoothFrameRate = 30;
  const int kNotSmoothFrameRate = 90;
  const int kFramesDecoded = 1000;
  // Sets the ratio of dropped frames to barely qualify as smooth.
  const int kSmoothFramesDropped =
      kFramesDecoded * kMaxSmoothDroppedFramesPercent;
  // Set the ratio of dropped frames to barely qualify as NOT smooth.
  const int kNotSmoothFramesDropped =
      kFramesDecoded * kMaxSmoothDroppedFramesPercent + 1;

  // Add the entries.
  database_->AppendDecodeStats(
      VideoDescKey(kKnownProfile, kKownSize, kSmoothFrameRate),
      DecodeStatsEntry(kFramesDecoded, kSmoothFramesDropped));
  database_->AppendDecodeStats(
      VideoDescKey(kKnownProfile, kKownSize, kNotSmoothFrameRate),
      DecodeStatsEntry(kFramesDecoded, kNotSmoothFramesDropped));

  // Verify perf history returns is_smooth = true for the smooth entry.
  bool is_smooth = true;
  bool is_power_efficient = true;
  EXPECT_CALL(*this, MockGetPerfInfoCB(is_smooth, is_power_efficient));
  perf_history_->GetPerfInfo(
      kKnownProfile, kKownSize, kSmoothFrameRate,
      base::BindOnce(&VideoDecodePerfHistoryTest::MockGetPerfInfoCB,
                     base::Unretained(this)));

  // Verify perf history returns is_smooth = false for the NOT smooth entry.
  is_smooth = false;
  EXPECT_CALL(*this, MockGetPerfInfoCB(is_smooth, is_power_efficient));
  perf_history_->GetPerfInfo(
      kKnownProfile, kKownSize, kNotSmoothFrameRate,
      base::BindOnce(&VideoDecodePerfHistoryTest::MockGetPerfInfoCB,
                     base::Unretained(this)));

  // Verify perf history optimistically returns is_smooth = true when no entry
  // can be found with the given configuration.
  const VideoCodecProfile kUnknownProfile = VP9PROFILE_PROFILE2;
  is_smooth = true;
  EXPECT_CALL(*this, MockGetPerfInfoCB(is_smooth, is_power_efficient));
  perf_history_->GetPerfInfo(
      kUnknownProfile, kKownSize, kNotSmoothFrameRate,
      base::BindOnce(&VideoDecodePerfHistoryTest::MockGetPerfInfoCB,
                     base::Unretained(this)));
}

}  // namespace media
