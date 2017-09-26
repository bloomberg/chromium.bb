// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <sstream>
#include <string>

#include "base/memory/ptr_util.h"
#include "media/mojo/services/media_capabilities_database.h"
#include "media/mojo/services/video_decode_perf_history.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

class FakeCapabilitiesDatabase : public MediaCapabilitiesDatabase {
 public:
  FakeCapabilitiesDatabase() = default;
  ~FakeCapabilitiesDatabase() override {}

  void AppendInfoToEntry(const Entry& entry, const Info& info) override {
    std::string key = MakeEntryKey(entry);
    if (entries_.find(key) == entries_.end()) {
      entries_.emplace(std::make_pair(key, info));
    } else {
      const Info& known_info = entries_.at(key);
      uint32_t frames_decoded = known_info.frames_decoded + info.frames_decoded;
      uint32_t frames_dropped = known_info.frames_dropped + info.frames_dropped;
      entries_.at(key) = Info(frames_decoded, frames_dropped);
    }
  }

  void GetInfo(const Entry& entry, GetInfoCallback callback) override {
    auto entry_it = entries_.find(MakeEntryKey(entry));
    if (entry_it == entries_.end()) {
      std::move(callback).Run(true, nullptr);
    } else {
      std::move(callback).Run(true, base::MakeUnique<Info>(entry_it->second));
    }
  }

 private:
  static std::string MakeEntryKey(const Entry& entry) {
    std::stringstream ss;
    ss << entry.codec_profile() << "|" << entry.size().ToString() << "|"
       << entry.frame_rate();
    return ss.str();
  }

  std::map<std::string, Info> entries_;
};

class VideoDecodePerfHistoryTest : public ::testing::Test {
 public:
  void SetUp() override {
    // Sniff the database pointer so tests can inject entries.
    database_ = base::MakeUnique<FakeCapabilitiesDatabase>();
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
  MOCK_METHOD2(MockPerfInfoCb, void(bool is_smooth, bool is_power_efficient));

 protected:
  using Entry = media::MediaCapabilitiesDatabase::Entry;
  using Info = media::MediaCapabilitiesDatabase::Info;

  static constexpr double kMaxSmoothDroppedFramesPercent =
      VideoDecodePerfHistory::kMaxSmoothDroppedFramesPercent;

  // The VideoDecodeStatsReporter being tested.
  VideoDecodePerfHistory* perf_history_;

  // The database |perf_history_| uses to store/query performance stats.
  std::unique_ptr<FakeCapabilitiesDatabase> database_;
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
  database_->AppendInfoToEntry(
      Entry(kKnownProfile, kKownSize, kSmoothFrameRate),
      Info(kFramesDecoded, kSmoothFramesDropped));
  database_->AppendInfoToEntry(
      Entry(kKnownProfile, kKownSize, kNotSmoothFrameRate),
      Info(kFramesDecoded, kNotSmoothFramesDropped));

  // Verify perf history returns is_smooth = true for the smooth entry.
  bool is_smooth = true;
  bool is_power_efficient = true;
  EXPECT_CALL(*this, MockPerfInfoCb(is_smooth, is_power_efficient));
  perf_history_->GetPerfInfo(
      kKnownProfile, kKownSize, kSmoothFrameRate,
      base::BindOnce(&VideoDecodePerfHistoryTest::MockPerfInfoCb,
                     base::Unretained(this)));

  // Verify perf history returns is_smooth = false for the NOT smooth entry.
  is_smooth = false;
  EXPECT_CALL(*this, MockPerfInfoCb(is_smooth, is_power_efficient));
  perf_history_->GetPerfInfo(
      kKnownProfile, kKownSize, kNotSmoothFrameRate,
      base::BindOnce(&VideoDecodePerfHistoryTest::MockPerfInfoCb,
                     base::Unretained(this)));

  // Verify perf history optimistically returns is_smooth = true when no entry
  // can be found with the given configuration.
  const VideoCodecProfile kUnknownProfile = VP9PROFILE_PROFILE2;
  is_smooth = true;
  EXPECT_CALL(*this, MockPerfInfoCb(is_smooth, is_power_efficient));
  perf_history_->GetPerfInfo(
      kUnknownProfile, kKownSize, kNotSmoothFrameRate,
      base::BindOnce(&VideoDecodePerfHistoryTest::MockPerfInfoCb,
                     base::Unretained(this)));
}

}  // namespace media
