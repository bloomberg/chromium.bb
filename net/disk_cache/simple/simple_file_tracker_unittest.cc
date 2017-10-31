// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "net/base/cache_type.h"
#include "net/disk_cache/disk_cache_test_base.h"
#include "net/disk_cache/simple/simple_file_tracker.h"
#include "net/disk_cache/simple/simple_synchronous_entry.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace disk_cache {

class SimpleFileTrackerTest : public DiskCacheTest {
 public:
  void DeleteSyncEntry(SimpleSynchronousEntry* entry) { delete entry; }

 protected:
  // A bit of messiness since we rely on friendship of the fixture to be able to
  // create/delete SimpleSynchronousEntry objects.
  class SyncEntryDeleter {
   public:
    SyncEntryDeleter(SimpleFileTrackerTest* fixture) : fixture_(fixture) {}
    void operator()(SimpleSynchronousEntry* entry) {
      fixture_->DeleteSyncEntry(entry);
    }

   private:
    SimpleFileTrackerTest* fixture_;
  };

  using SyncEntryPointer =
      std::unique_ptr<SimpleSynchronousEntry, SyncEntryDeleter>;

  SyncEntryPointer MakeSyncEntry(uint64_t hash) {
    return SyncEntryPointer(
        new SimpleSynchronousEntry(net::DISK_CACHE, cache_path_, "dummy", hash,
                                   /* had_index=*/true, &file_tracker_),
        SyncEntryDeleter(this));
  }

  SimpleFileTracker file_tracker_;
};

TEST_F(SimpleFileTrackerTest, Basic) {
  SyncEntryPointer entry = MakeSyncEntry(1);

  // Just transfer some files to the tracker, and then do some I/O on getting
  // them back.
  base::FilePath path_0 = cache_path_.AppendASCII("file_0");
  base::FilePath path_1 = cache_path_.AppendASCII("file_1");

  std::unique_ptr<base::File> file_0 = std::make_unique<base::File>(
      path_0, base::File::FLAG_CREATE | base::File::FLAG_WRITE);
  std::unique_ptr<base::File> file_1 = std::make_unique<base::File>(
      path_1, base::File::FLAG_CREATE | base::File::FLAG_WRITE);
  ASSERT_TRUE(file_0->IsValid());
  ASSERT_TRUE(file_1->IsValid());

  file_tracker_.Register(entry.get(), SimpleFileTracker::SubFile::FILE_0,
                         std::move(file_0));
  file_tracker_.Register(entry.get(), SimpleFileTracker::SubFile::FILE_1,
                         std::move(file_1));

  base::StringPiece msg_0 = "Hello";
  base::StringPiece msg_1 = "Worldish Place";

  {
    SimpleFileTracker::FileHandle borrow_0 =
        file_tracker_.Acquire(entry.get(), SimpleFileTracker::SubFile::FILE_0);
    SimpleFileTracker::FileHandle borrow_1 =
        file_tracker_.Acquire(entry.get(), SimpleFileTracker::SubFile::FILE_1);

    EXPECT_EQ(static_cast<int>(msg_0.size()),
              borrow_0->Write(0, msg_0.data(), msg_0.size()));
    EXPECT_EQ(static_cast<int>(msg_1.size()),
              borrow_1->Write(0, msg_1.data(), msg_1.size()));

    // For stream 0 do release/close, for stream 1 do close/release --- where
    // release happens when borrow_{0,1} go out of scope
    file_tracker_.Close(entry.get(), SimpleFileTracker::SubFile::FILE_1);
  }
  file_tracker_.Close(entry.get(), SimpleFileTracker::SubFile::FILE_0);

  // Verify contents.
  std::string verify_0, verify_1;
  EXPECT_TRUE(ReadFileToString(path_0, &verify_0));
  EXPECT_TRUE(ReadFileToString(path_1, &verify_1));
  EXPECT_EQ(msg_0, verify_0);
  EXPECT_EQ(msg_1, verify_1);
  EXPECT_TRUE(file_tracker_.IsEmptyForTesting());
}

TEST_F(SimpleFileTrackerTest, Collision) {
  // Two entries with same key.
  SyncEntryPointer entry = MakeSyncEntry(1);
  SyncEntryPointer entry2 = MakeSyncEntry(1);

  base::FilePath path = cache_path_.AppendASCII("file");
  base::FilePath path2 = cache_path_.AppendASCII("file2");

  std::unique_ptr<base::File> file = std::make_unique<base::File>(
      path, base::File::FLAG_CREATE | base::File::FLAG_WRITE);
  std::unique_ptr<base::File> file2 = std::make_unique<base::File>(
      path2, base::File::FLAG_CREATE | base::File::FLAG_WRITE);
  ASSERT_TRUE(file->IsValid());
  ASSERT_TRUE(file2->IsValid());

  file_tracker_.Register(entry.get(), SimpleFileTracker::SubFile::FILE_0,
                         std::move(file));
  file_tracker_.Register(entry2.get(), SimpleFileTracker::SubFile::FILE_0,
                         std::move(file2));

  base::StringPiece msg = "Alpha";
  base::StringPiece msg2 = "Beta";

  {
    SimpleFileTracker::FileHandle borrow =
        file_tracker_.Acquire(entry.get(), SimpleFileTracker::SubFile::FILE_0);
    SimpleFileTracker::FileHandle borrow2 =
        file_tracker_.Acquire(entry2.get(), SimpleFileTracker::SubFile::FILE_0);

    EXPECT_EQ(static_cast<int>(msg.size()),
              borrow->Write(0, msg.data(), msg.size()));
    EXPECT_EQ(static_cast<int>(msg2.size()),
              borrow2->Write(0, msg2.data(), msg2.size()));
  }
  file_tracker_.Close(entry.get(), SimpleFileTracker::SubFile::FILE_0);
  file_tracker_.Close(entry2.get(), SimpleFileTracker::SubFile::FILE_0);

  // Verify contents.
  std::string verify, verify2;
  EXPECT_TRUE(ReadFileToString(path, &verify));
  EXPECT_TRUE(ReadFileToString(path2, &verify2));
  EXPECT_EQ(msg, verify);
  EXPECT_EQ(msg2, verify2);
  EXPECT_TRUE(file_tracker_.IsEmptyForTesting());
}

TEST_F(SimpleFileTrackerTest, Reopen) {
  // We may sometimes go Register -> Close -> Register, with info still
  // alive.
  SyncEntryPointer entry = MakeSyncEntry(1);

  base::FilePath path_0 = cache_path_.AppendASCII("file_0");
  base::FilePath path_1 = cache_path_.AppendASCII("file_1");

  std::unique_ptr<base::File> file_0 = std::make_unique<base::File>(
      path_0, base::File::FLAG_CREATE | base::File::FLAG_WRITE);
  std::unique_ptr<base::File> file_1 = std::make_unique<base::File>(
      path_1, base::File::FLAG_CREATE | base::File::FLAG_WRITE);
  ASSERT_TRUE(file_0->IsValid());
  ASSERT_TRUE(file_1->IsValid());

  file_tracker_.Register(entry.get(), SimpleFileTracker::SubFile::FILE_0,
                         std::move(file_0));
  file_tracker_.Register(entry.get(), SimpleFileTracker::SubFile::FILE_1,
                         std::move(file_1));

  file_tracker_.Close(entry.get(), SimpleFileTracker::SubFile::FILE_1);
  base::File file_1b(path_1, base::File::FLAG_OPEN | base::File::FLAG_WRITE);
  ASSERT_TRUE(file_1b.IsValid());
  file_tracker_.Register(entry.get(), SimpleFileTracker::SubFile::FILE_1,
                         std::move(file_1));
  file_tracker_.Close(entry.get(), SimpleFileTracker::SubFile::FILE_0);
  file_tracker_.Close(entry.get(), SimpleFileTracker::SubFile::FILE_1);
  EXPECT_TRUE(file_tracker_.IsEmptyForTesting());
}

TEST_F(SimpleFileTrackerTest, PointerStability) {
  // Make sure the FileHandle lent out doesn't get screwed up as we update
  // the state (and potentially move the underlying base::File object around).
  const int kEntries = 8;
  SyncEntryPointer entries[kEntries] = {
      MakeSyncEntry(1), MakeSyncEntry(1), MakeSyncEntry(1), MakeSyncEntry(1),
      MakeSyncEntry(1), MakeSyncEntry(1), MakeSyncEntry(1), MakeSyncEntry(1),
  };
  std::unique_ptr<base::File> file_0 = std::make_unique<base::File>(
      cache_path_.AppendASCII("0"),
      base::File::FLAG_CREATE | base::File::FLAG_WRITE);
  ASSERT_TRUE(file_0->IsValid());
  file_tracker_.Register(entries[0].get(), SimpleFileTracker::SubFile::FILE_0,
                         std::move(file_0));

  base::StringPiece msg = "Message to write";
  {
    SimpleFileTracker::FileHandle borrow = file_tracker_.Acquire(
        entries[0].get(), SimpleFileTracker::SubFile::FILE_0);
    for (int i = 1; i < kEntries; ++i) {
      std::unique_ptr<base::File> file_n = std::make_unique<base::File>(
          cache_path_.AppendASCII(base::IntToString(i)),
          base::File::FLAG_CREATE | base::File::FLAG_WRITE);
      ASSERT_TRUE(file_n->IsValid());
      file_tracker_.Register(entries[i].get(),
                             SimpleFileTracker::SubFile::FILE_0,
                             std::move(file_n));
    }

    EXPECT_EQ(static_cast<int>(msg.size()),
              borrow->Write(0, msg.data(), msg.size()));
  }

  for (int i = 0; i < kEntries; ++i)
    file_tracker_.Close(entries[i].get(), SimpleFileTracker::SubFile::FILE_0);

  // Verify the file.
  std::string verify;
  EXPECT_TRUE(ReadFileToString(cache_path_.AppendASCII("0"), &verify));
  EXPECT_EQ(msg, verify);
  EXPECT_TRUE(file_tracker_.IsEmptyForTesting());
}

}  // namespace disk_cache
