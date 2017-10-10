// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/test/scoped_task_environment.h"
#include "components/leveldb_proto/testing/fake_db.h"
#include "media/base/test_data_util.h"
#include "media/base/video_codecs.h"
#include "media/mojo/services/video_decode_stats.pb.h"
#include "media/mojo/services/video_decode_stats_db_impl.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"

using leveldb_proto::test::FakeDB;
using testing::Pointee;
using testing::Eq;

namespace media {

class VideoDecodeStatsDBImplTest : public ::testing::Test {
 public:
  VideoDecodeStatsDBImplTest() = default;

  void SetUp() override {
    // Fake DB simply wraps a std::map with the LevelDB interface. We own the
    // map and will delete it in TearDown().
    fake_db_map_ = std::make_unique<FakeDB<DecodeStatsProto>::EntryMap>();
    // |stats_db_| will own this pointer, but we hold a reference to control
    // its behavior.
    fake_db_ = new FakeDB<DecodeStatsProto>(fake_db_map_.get());

    // Make our DB wrapper and kick off Initialize. Each test should start
    // by completing initialize, calling fake_db_->InitCallback(...).
    stats_db_ = std::make_unique<VideoDecodeStatsDBImpl>();
    stats_db_->Initialize(std::unique_ptr<FakeDB<DecodeStatsProto>>(fake_db_),
                          base::FilePath(FILE_PATH_LITERAL("/fake/path")));
  }

  void TearDown() override {
    stats_db_.reset();
    fake_db_map_.reset();
  }

  // Unwraps move-only parameters to pass to the mock function.
  void GetDecodeStatsCb(
      bool success,
      std::unique_ptr<VideoDecodeStatsDB::DecodeStatsEntry> entry) {
    MockGetDecodeStatsCb(success, entry.get());
  }

  MOCK_METHOD2(MockGetDecodeStatsCb,
               void(bool success, VideoDecodeStatsDB::DecodeStatsEntry* entry));

  void AppendInfoCallback(base::Closure callback, bool success) {
    ASSERT_TRUE(success);
    std::move(callback).Run();
  }

 protected:
  using VideoDescKey = VideoDecodeStatsDB::VideoDescKey;
  using DecodeStatsEntry = VideoDecodeStatsDB::DecodeStatsEntry;

  base::test::ScopedTaskEnvironment scoped_task_environment_;

  // See documentation in SetUp()
  std::unique_ptr<FakeDB<DecodeStatsProto>::EntryMap> fake_db_map_;
  FakeDB<DecodeStatsProto>* fake_db_;
  std::unique_ptr<VideoDecodeStatsDBImpl> stats_db_;

 private:
  DISALLOW_COPY_AND_ASSIGN(VideoDecodeStatsDBImplTest);
};

MATCHER_P(EntryEq, other_entry, "") {
  return arg.frames_decoded == other_entry.frames_decoded &&
         arg.frames_dropped == other_entry.frames_dropped;
}

TEST_F(VideoDecodeStatsDBImplTest, ReadExpectingNothing) {
  fake_db_->InitCallback(true);

  VideoDescKey entry(VP9PROFILE_PROFILE3, gfx::Size(1024, 768), 60);

  // Database is empty. Expect null entry.
  EXPECT_CALL(*this, MockGetDecodeStatsCb(true, nullptr));
  stats_db_->GetDecodeStats(
      entry, base::BindOnce(&VideoDecodeStatsDBImplTest::GetDecodeStatsCb,
                            base::Unretained(this)));

  fake_db_->GetCallback(true);
}

TEST_F(VideoDecodeStatsDBImplTest, WriteAndRead) {
  fake_db_->InitCallback(true);

  VideoDescKey key(VP9PROFILE_PROFILE3, gfx::Size(1024, 768), 60);
  VideoDecodeStatsDB::DecodeStatsEntry entry(1000, 2);

  stats_db_->AppendDecodeStats(key, entry);
  fake_db_->GetCallback(true);

  EXPECT_CALL(*this, MockGetDecodeStatsCb(true, Pointee(EntryEq(entry))));
  stats_db_->GetDecodeStats(
      key, base::BindOnce(&VideoDecodeStatsDBImplTest::GetDecodeStatsCb,
                          base::Unretained(this)));
  fake_db_->GetCallback(true);

  // Append the same entry again.
  stats_db_->AppendDecodeStats(key, entry);
  fake_db_->GetCallback(true);

  // Expect to read what was written (2x the initial entry).
  VideoDecodeStatsDB::DecodeStatsEntry aggregate_entry(2000, 4);
  EXPECT_CALL(*this,
              MockGetDecodeStatsCb(true, Pointee(EntryEq(aggregate_entry))));
  stats_db_->GetDecodeStats(
      key, base::BindOnce(&VideoDecodeStatsDBImplTest::GetDecodeStatsCb,
                          base::Unretained(this)));
  fake_db_->GetCallback(true);
}

TEST_F(VideoDecodeStatsDBImplTest, GetDecodeStatsDbNotInitialized) {
  // Simulate failed/forgotten initialization.
  fake_db_->InitCallback(false);

  VideoDescKey key(VP9PROFILE_PROFILE3, gfx::Size(1024, 768), 60);
  EXPECT_CALL(*this, MockGetDecodeStatsCb(false, nullptr));
  // Note that no |fake_db_| is not instructed to execute the GetCallback(...).
  // The DB is not initialized, so the callback is executed (failed) early
  // without consulting the DB.
  stats_db_->GetDecodeStats(
      key, base::BindOnce(&VideoDecodeStatsDBImplTest::GetDecodeStatsCb,
                          base::Unretained(this)));
}

}  // namespace media
