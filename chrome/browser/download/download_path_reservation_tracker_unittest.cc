// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/observer_list.h"
#include "base/strings/stringprintf.h"
#include "base/test/test_file_util.h"
#include "chrome/browser/download/download_path_reservation_tracker.h"
#include "chrome/browser/download/download_target_determiner.h"
#include "content/public/test/mock_download_item.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;
using content::DownloadItem;
using content::MockDownloadItem;
using testing::AnyNumber;
using testing::Return;
using testing::ReturnRef;
using testing::ReturnRefOfCopy;

namespace {

class DownloadPathReservationTrackerTest : public testing::Test {
 public:
  DownloadPathReservationTrackerTest();

  // testing::Test
  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;

  MockDownloadItem* CreateDownloadItem(int32 id);
  base::FilePath GetPathInDownloadsDirectory(
      const base::FilePath::CharType* suffix);
  bool IsPathInUse(const base::FilePath& path);
  void CallGetReservedPath(
      DownloadItem* download_item,
      const base::FilePath& target_path,
      bool create_directory,
      DownloadPathReservationTracker::FilenameConflictAction conflict_action,
      base::FilePath* return_path,
      bool* return_verified);

  const base::FilePath& default_download_path() const {
    return default_download_path_;
  }
  void set_default_download_path(const base::FilePath& path) {
    default_download_path_ = path;
  }
  // Creates a name of form 'a'*repeat + suffix
  base::FilePath GetLongNamePathInDownloadsDirectory(
      size_t repeat, const base::FilePath::CharType* suffix);

 protected:
  base::ScopedTempDir test_download_dir_;
  base::FilePath default_download_path_;
  base::MessageLoopForUI message_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread file_thread_;

 private:
  void TestReservedPathCallback(base::FilePath* return_path,
                                bool* return_verified, bool* did_run_callback,
                                const base::FilePath& path, bool verified);
};

DownloadPathReservationTrackerTest::DownloadPathReservationTrackerTest()
    : ui_thread_(BrowserThread::UI, &message_loop_),
      file_thread_(BrowserThread::FILE, &message_loop_) {
}

void DownloadPathReservationTrackerTest::SetUp() {
  ASSERT_TRUE(test_download_dir_.CreateUniqueTempDir());
  set_default_download_path(test_download_dir_.path());
}

void DownloadPathReservationTrackerTest::TearDown() {
  message_loop_.RunUntilIdle();
}

MockDownloadItem* DownloadPathReservationTrackerTest::CreateDownloadItem(
    int32 id) {
  MockDownloadItem* item = new ::testing::StrictMock<MockDownloadItem>;
  EXPECT_CALL(*item, GetId())
      .WillRepeatedly(Return(id));
  EXPECT_CALL(*item, GetTargetFilePath())
      .WillRepeatedly(ReturnRefOfCopy(base::FilePath()));
  EXPECT_CALL(*item, GetState())
      .WillRepeatedly(Return(DownloadItem::IN_PROGRESS));
  return item;
}

base::FilePath DownloadPathReservationTrackerTest::GetPathInDownloadsDirectory(
    const base::FilePath::CharType* suffix) {
  return default_download_path().Append(suffix).NormalizePathSeparators();
}

bool DownloadPathReservationTrackerTest::IsPathInUse(
    const base::FilePath& path) {
  return DownloadPathReservationTracker::IsPathInUseForTesting(path);
}

void DownloadPathReservationTrackerTest::CallGetReservedPath(
    DownloadItem* download_item,
    const base::FilePath& target_path,
    bool create_directory,
    DownloadPathReservationTracker::FilenameConflictAction conflict_action,
    base::FilePath* return_path,
    bool* return_verified) {
  // Weak pointer factory to prevent the callback from running after this
  // function has returned.
  base::WeakPtrFactory<DownloadPathReservationTrackerTest> weak_ptr_factory(
      this);
  bool did_run_callback = false;
  DownloadPathReservationTracker::GetReservedPath(
      download_item,
      target_path,
      default_download_path(),
      create_directory,
      conflict_action,
      base::Bind(&DownloadPathReservationTrackerTest::TestReservedPathCallback,
                 weak_ptr_factory.GetWeakPtr(), return_path, return_verified,
                 &did_run_callback));
  message_loop_.RunUntilIdle();
  EXPECT_TRUE(did_run_callback);
}

void DownloadPathReservationTrackerTest::TestReservedPathCallback(
    base::FilePath* return_path, bool* return_verified, bool* did_run_callback,
    const base::FilePath& path, bool verified) {
  *did_run_callback = true;
  *return_path = path;
  *return_verified = verified;
}

base::FilePath
DownloadPathReservationTrackerTest::GetLongNamePathInDownloadsDirectory(
    size_t repeat, const base::FilePath::CharType* suffix) {
  return GetPathInDownloadsDirectory(
      (base::FilePath::StringType(repeat, FILE_PATH_LITERAL('a'))
          + suffix).c_str());
}

void SetDownloadItemState(content::MockDownloadItem* download_item,
                          content::DownloadItem::DownloadState state) {
  EXPECT_CALL(*download_item, GetState())
      .WillRepeatedly(Return(state));
  download_item->NotifyObserversDownloadUpdated();
}

}  // namespace

// A basic reservation is acquired and committed.
TEST_F(DownloadPathReservationTrackerTest, BasicReservation) {
  scoped_ptr<MockDownloadItem> item(CreateDownloadItem(1));
  base::FilePath path(
      GetPathInDownloadsDirectory(FILE_PATH_LITERAL("foo.txt")));
  ASSERT_FALSE(IsPathInUse(path));

  base::FilePath reserved_path;
  bool verified = false;
  DownloadPathReservationTracker::FilenameConflictAction conflict_action =
    DownloadPathReservationTracker::OVERWRITE;
  bool create_directory = false;
  CallGetReservedPath(
      item.get(),
      path,
      create_directory,
      conflict_action,
      &reserved_path,
      &verified);
  EXPECT_TRUE(IsPathInUse(path));
  EXPECT_TRUE(verified);
  EXPECT_EQ(path.value(), reserved_path.value());

  // Destroying the item should release the reservation.
  SetDownloadItemState(item.get(), DownloadItem::COMPLETE);
  item.reset();
  message_loop_.RunUntilIdle();
  EXPECT_FALSE(IsPathInUse(path));
}

// A download that is interrupted should lose its reservation.
TEST_F(DownloadPathReservationTrackerTest, InterruptedDownload) {
  scoped_ptr<MockDownloadItem> item(CreateDownloadItem(1));
  base::FilePath path(
      GetPathInDownloadsDirectory(FILE_PATH_LITERAL("foo.txt")));
  ASSERT_FALSE(IsPathInUse(path));

  base::FilePath reserved_path;
  bool verified = false;
  DownloadPathReservationTracker::FilenameConflictAction conflict_action =
    DownloadPathReservationTracker::OVERWRITE;
  bool create_directory = false;
  CallGetReservedPath(
      item.get(),
      path,
      create_directory,
      conflict_action,
      &reserved_path,
      &verified);
  EXPECT_TRUE(IsPathInUse(path));
  EXPECT_TRUE(verified);
  EXPECT_EQ(path.value(), reserved_path.value());

  // Once the download is interrupted, the path should become available again.
  SetDownloadItemState(item.get(), DownloadItem::INTERRUPTED);
  message_loop_.RunUntilIdle();
  EXPECT_FALSE(IsPathInUse(path));
}

// A completed download should also lose its reservation.
TEST_F(DownloadPathReservationTrackerTest, CompleteDownload) {
  scoped_ptr<MockDownloadItem> item(CreateDownloadItem(1));
  base::FilePath path(
      GetPathInDownloadsDirectory(FILE_PATH_LITERAL("foo.txt")));
  ASSERT_FALSE(IsPathInUse(path));

  base::FilePath reserved_path;
  bool verified = false;
  DownloadPathReservationTracker::FilenameConflictAction conflict_action =
    DownloadPathReservationTracker::OVERWRITE;
  bool create_directory = false;
  CallGetReservedPath(
      item.get(),
      path,
      create_directory,
      conflict_action,
      &reserved_path,
      &verified);
  EXPECT_TRUE(IsPathInUse(path));
  EXPECT_TRUE(verified);
  EXPECT_EQ(path.value(), reserved_path.value());

  // Once the download completes, the path should become available again. For a
  // real download, at this point only the path reservation will be released.
  // The path wouldn't be available since it is occupied on disk by the
  // completed download.
  SetDownloadItemState(item.get(), DownloadItem::COMPLETE);
  message_loop_.RunUntilIdle();
  EXPECT_FALSE(IsPathInUse(path));
}

// If there are files on the file system, a unique reservation should uniquify
// around it.
TEST_F(DownloadPathReservationTrackerTest, ConflictingFiles) {
  scoped_ptr<MockDownloadItem> item(CreateDownloadItem(1));
  base::FilePath path(
      GetPathInDownloadsDirectory(FILE_PATH_LITERAL("foo.txt")));
  base::FilePath path1(
      GetPathInDownloadsDirectory(FILE_PATH_LITERAL("foo (1).txt")));
  // Create a file at |path|, and a .crdownload file at |path1|.
  ASSERT_EQ(0, base::WriteFile(path, "", 0));
  ASSERT_EQ(0,
            base::WriteFile(
                DownloadTargetDeterminer::GetCrDownloadPath(path1), "", 0));
  ASSERT_TRUE(IsPathInUse(path));

  base::FilePath reserved_path;
  bool verified = false;
  bool create_directory = false;
  DownloadPathReservationTracker::FilenameConflictAction conflict_action =
    DownloadPathReservationTracker::UNIQUIFY;
  CallGetReservedPath(
      item.get(),
      path,
      create_directory,
      conflict_action,
      &reserved_path,
      &verified);
  EXPECT_TRUE(IsPathInUse(path));
  EXPECT_TRUE(IsPathInUse(reserved_path));
  EXPECT_TRUE(verified);
  // The path should be uniquified, skipping over foo.txt but not over
  // "foo (1).txt.crdownload"
  EXPECT_EQ(
      GetPathInDownloadsDirectory(FILE_PATH_LITERAL("foo (1).txt")).value(),
      reserved_path.value());

  SetDownloadItemState(item.get(), DownloadItem::COMPLETE);
  item.reset();
  message_loop_.RunUntilIdle();
  EXPECT_TRUE(IsPathInUse(path));
  EXPECT_FALSE(IsPathInUse(reserved_path));
}

// Multiple reservations for the same path should uniquify around each other.
TEST_F(DownloadPathReservationTrackerTest, ConflictingReservations) {
  scoped_ptr<MockDownloadItem> item1(CreateDownloadItem(1));
  base::FilePath path(
      GetPathInDownloadsDirectory(FILE_PATH_LITERAL("foo.txt")));
  base::FilePath uniquified_path(
      GetPathInDownloadsDirectory(FILE_PATH_LITERAL("foo (1).txt")));
  ASSERT_FALSE(IsPathInUse(path));
  ASSERT_FALSE(IsPathInUse(uniquified_path));

  base::FilePath reserved_path1;
  bool verified = false;
  bool create_directory = false;

  DownloadPathReservationTracker::FilenameConflictAction conflict_action =
    DownloadPathReservationTracker::UNIQUIFY;
  CallGetReservedPath(
      item1.get(),
      path,
      create_directory,
      conflict_action,
      &reserved_path1,
      &verified);
  EXPECT_TRUE(IsPathInUse(path));
  EXPECT_TRUE(verified);


  {
    // Requesting a reservation for the same path with uniquification results in
    // a uniquified path.
    scoped_ptr<MockDownloadItem> item2(CreateDownloadItem(2));
    base::FilePath reserved_path2;
    CallGetReservedPath(
        item2.get(),
        path,
        create_directory,
        conflict_action,
        &reserved_path2,
        &verified);
    EXPECT_TRUE(IsPathInUse(path));
    EXPECT_TRUE(IsPathInUse(uniquified_path));
    EXPECT_EQ(uniquified_path.value(), reserved_path2.value());
    SetDownloadItemState(item2.get(), DownloadItem::COMPLETE);
  }
  message_loop_.RunUntilIdle();
  EXPECT_TRUE(IsPathInUse(path));
  EXPECT_FALSE(IsPathInUse(uniquified_path));

  {
    // Since the previous download item was removed, requesting a reservation
    // for the same path should result in the same uniquified path.
    scoped_ptr<MockDownloadItem> item2(CreateDownloadItem(2));
    base::FilePath reserved_path2;
    CallGetReservedPath(
        item2.get(),
        path,
        create_directory,
        conflict_action,
        &reserved_path2,
        &verified);
    EXPECT_TRUE(IsPathInUse(path));
    EXPECT_TRUE(IsPathInUse(uniquified_path));
    EXPECT_EQ(uniquified_path.value(), reserved_path2.value());
    SetDownloadItemState(item2.get(), DownloadItem::COMPLETE);
  }
  message_loop_.RunUntilIdle();

  // Now acquire an overwriting reservation. We should end up with the same
  // non-uniquified path for both reservations.
  scoped_ptr<MockDownloadItem> item3(CreateDownloadItem(2));
  base::FilePath reserved_path3;
  conflict_action = DownloadPathReservationTracker::OVERWRITE;
  CallGetReservedPath(
      item3.get(),
      path,
      create_directory,
      conflict_action,
      &reserved_path3,
      &verified);
  EXPECT_TRUE(IsPathInUse(path));
  EXPECT_FALSE(IsPathInUse(uniquified_path));

  EXPECT_EQ(path.value(), reserved_path1.value());
  EXPECT_EQ(path.value(), reserved_path3.value());

  SetDownloadItemState(item1.get(), DownloadItem::COMPLETE);
  SetDownloadItemState(item3.get(), DownloadItem::COMPLETE);
}

// If a unique path cannot be determined after trying kMaxUniqueFiles
// uniquifiers, then the callback should notified that verification failed, and
// the returned path should be set to the original requested path.
TEST_F(DownloadPathReservationTrackerTest, UnresolvedConflicts) {
  base::FilePath path(
      GetPathInDownloadsDirectory(FILE_PATH_LITERAL("foo.txt")));
  scoped_ptr<MockDownloadItem>
      items[DownloadPathReservationTracker::kMaxUniqueFiles + 1];
  DownloadPathReservationTracker::FilenameConflictAction conflict_action =
    DownloadPathReservationTracker::UNIQUIFY;
  bool create_directory = false;

  // Create |kMaxUniqueFiles + 1| reservations for |path|. The first reservation
  // will have no uniquifier. The |kMaxUniqueFiles| remaining reservations do.
  for (int i = 0; i <= DownloadPathReservationTracker::kMaxUniqueFiles; i++) {
    base::FilePath reserved_path;
    base::FilePath expected_path;
    bool verified = false;
    if (i > 0) {
      expected_path =
          path.InsertBeforeExtensionASCII(base::StringPrintf(" (%d)", i));
    } else {
      expected_path = path;
    }
    items[i].reset(CreateDownloadItem(i));
    EXPECT_FALSE(IsPathInUse(expected_path));
    CallGetReservedPath(
        items[i].get(),
        path,
        create_directory,
        conflict_action,
        &reserved_path,
        &verified);
    EXPECT_TRUE(IsPathInUse(expected_path));
    EXPECT_EQ(expected_path.value(), reserved_path.value());
    EXPECT_TRUE(verified);
  }
  // The next reservation for |path| will fail to be unique.
  scoped_ptr<MockDownloadItem> item(
      CreateDownloadItem(DownloadPathReservationTracker::kMaxUniqueFiles + 1));
  base::FilePath reserved_path;
  bool verified = true;
  CallGetReservedPath(
      item.get(),
      path,
      create_directory,
      conflict_action,
      &reserved_path,
      &verified);
  EXPECT_FALSE(verified);
  EXPECT_EQ(path.value(), reserved_path.value());

  SetDownloadItemState(item.get(), DownloadItem::COMPLETE);
  for (int i = 0; i <= DownloadPathReservationTracker::kMaxUniqueFiles; i++)
    SetDownloadItemState(items[i].get(), DownloadItem::COMPLETE);
}

// If the target directory is unwriteable, then callback should be notified that
// verification failed.
TEST_F(DownloadPathReservationTrackerTest, UnwriteableDirectory) {
  scoped_ptr<MockDownloadItem> item(CreateDownloadItem(1));
  base::FilePath path(
      GetPathInDownloadsDirectory(FILE_PATH_LITERAL("foo.txt")));
  base::FilePath dir(path.DirName());
  ASSERT_FALSE(IsPathInUse(path));

  {
    // Scope for FilePermissionRestorer
    base::FilePermissionRestorer restorer(dir);
    EXPECT_TRUE(base::MakeFileUnwritable(dir));
    base::FilePath reserved_path;
    bool verified = true;
    DownloadPathReservationTracker::FilenameConflictAction conflict_action =
      DownloadPathReservationTracker::OVERWRITE;
    bool create_directory = false;
    CallGetReservedPath(
        item.get(),
        path,
        create_directory,
        conflict_action,
        &reserved_path,
        &verified);
    // Verification fails.
    EXPECT_FALSE(verified);
    EXPECT_EQ(path.BaseName().value(), reserved_path.BaseName().value());
  }
  SetDownloadItemState(item.get(), DownloadItem::COMPLETE);
}

// If the default download directory doesn't exist, then it should be
// created. But only if we are actually going to create the download path there.
TEST_F(DownloadPathReservationTrackerTest, CreateDefaultDownloadPath) {
  base::FilePath path(
      GetPathInDownloadsDirectory(FILE_PATH_LITERAL("foo/foo.txt")));
  base::FilePath dir(path.DirName());
  ASSERT_FALSE(base::DirectoryExists(dir));
  DownloadPathReservationTracker::FilenameConflictAction conflict_action =
    DownloadPathReservationTracker::OVERWRITE;
  bool create_directory = false;

  {
    scoped_ptr<MockDownloadItem> item(CreateDownloadItem(1));
    base::FilePath reserved_path;
    bool verified = true;
    CallGetReservedPath(
        item.get(),
        path,
        create_directory,
        conflict_action,
        &reserved_path,
        &verified);
    // Verification fails because the directory doesn't exist.
    EXPECT_FALSE(verified);
    SetDownloadItemState(item.get(), DownloadItem::COMPLETE);
  }
  ASSERT_FALSE(IsPathInUse(path));
  {
    scoped_ptr<MockDownloadItem> item(CreateDownloadItem(1));
    base::FilePath reserved_path;
    bool verified = true;
    set_default_download_path(dir);
    CallGetReservedPath(
        item.get(),
        path,
        create_directory,
        conflict_action,
        &reserved_path,
        &verified);
    // Verification succeeds because the directory is created.
    EXPECT_TRUE(verified);
    EXPECT_TRUE(base::DirectoryExists(dir));
    SetDownloadItemState(item.get(), DownloadItem::COMPLETE);
  }
}

// If the target path of the download item changes, the reservation should be
// updated to match.
TEST_F(DownloadPathReservationTrackerTest, UpdatesToTargetPath) {
  scoped_ptr<MockDownloadItem> item(CreateDownloadItem(1));
  base::FilePath path(
      GetPathInDownloadsDirectory(FILE_PATH_LITERAL("foo.txt")));
  ASSERT_FALSE(IsPathInUse(path));

  base::FilePath reserved_path;
  bool verified = false;
  DownloadPathReservationTracker::FilenameConflictAction conflict_action =
    DownloadPathReservationTracker::OVERWRITE;
  bool create_directory = false;
  CallGetReservedPath(
      item.get(),
      path,
      create_directory,
      conflict_action,
      &reserved_path,
      &verified);
  EXPECT_TRUE(IsPathInUse(path));
  EXPECT_TRUE(verified);
  EXPECT_EQ(path.value(), reserved_path.value());

  // The target path is initially empty. If an OnDownloadUpdated() is issued in
  // this state, we shouldn't lose the reservation.
  ASSERT_EQ(base::FilePath::StringType(), item->GetTargetFilePath().value());
  item->NotifyObserversDownloadUpdated();
  message_loop_.RunUntilIdle();
  EXPECT_TRUE(IsPathInUse(path));

  // If the target path changes, we should update the reservation to match.
  base::FilePath new_target_path(
      GetPathInDownloadsDirectory(FILE_PATH_LITERAL("bar.txt")));
  ASSERT_FALSE(IsPathInUse(new_target_path));
  EXPECT_CALL(*item, GetTargetFilePath())
      .WillRepeatedly(ReturnRef(new_target_path));
  item->NotifyObserversDownloadUpdated();
  message_loop_.RunUntilIdle();
  EXPECT_FALSE(IsPathInUse(path));
  EXPECT_TRUE(IsPathInUse(new_target_path));

  // Destroying the item should release the reservation.
  SetDownloadItemState(item.get(), DownloadItem::COMPLETE);
  item.reset();
  message_loop_.RunUntilIdle();
  EXPECT_FALSE(IsPathInUse(new_target_path));
}

// Tests for long name truncation. On other platforms automatic truncation
// is not performed (yet).
#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_CHROMEOS)

TEST_F(DownloadPathReservationTrackerTest, BasicTruncation) {
  int real_max_length =
      base::GetMaximumPathComponentLength(default_download_path());
  ASSERT_NE(-1, real_max_length);

  // TODO(kinaba): the current implementation leaves spaces for appending
  // ".crdownload". So take it into account. Should be removed in the future.
  const size_t max_length = real_max_length - 11;

  scoped_ptr<MockDownloadItem> item(CreateDownloadItem(1));
  base::FilePath path(GetLongNamePathInDownloadsDirectory(
      max_length, FILE_PATH_LITERAL(".txt")));
  ASSERT_FALSE(IsPathInUse(path));

  base::FilePath reserved_path;
  bool verified = false;
  DownloadPathReservationTracker::FilenameConflictAction conflict_action =
    DownloadPathReservationTracker::OVERWRITE;
  bool create_directory = false;
  CallGetReservedPath(
      item.get(),
      path,
      create_directory,
      conflict_action,
      &reserved_path,
      &verified);
  EXPECT_TRUE(IsPathInUse(reserved_path));
  EXPECT_TRUE(verified);
  // The file name length is truncated to max_length.
  EXPECT_EQ(max_length, reserved_path.BaseName().value().size());
  // But the extension is kept unchanged.
  EXPECT_EQ(path.Extension(), reserved_path.Extension());
  SetDownloadItemState(item.get(), DownloadItem::COMPLETE);
}

TEST_F(DownloadPathReservationTrackerTest, TruncationConflict) {
  int real_max_length =
      base::GetMaximumPathComponentLength(default_download_path());
  ASSERT_NE(-1, real_max_length);
  const size_t max_length = real_max_length - 11;

  scoped_ptr<MockDownloadItem> item(CreateDownloadItem(1));
  base::FilePath path(GetLongNamePathInDownloadsDirectory(
      max_length, FILE_PATH_LITERAL(".txt")));
  base::FilePath path0(GetLongNamePathInDownloadsDirectory(
      max_length - 4, FILE_PATH_LITERAL(".txt")));
  base::FilePath path1(GetLongNamePathInDownloadsDirectory(
      max_length - 8, FILE_PATH_LITERAL(" (1).txt")));
  base::FilePath path2(GetLongNamePathInDownloadsDirectory(
      max_length - 8, FILE_PATH_LITERAL(" (2).txt")));
  ASSERT_FALSE(IsPathInUse(path));
  // "aaa...aaaaaaa.txt" (truncated path) and
  // "aaa...aaa (1).txt" (truncated and first uniquification try) exists.
  // "aaa...aaa (2).txt" should be used.
  ASSERT_EQ(0, base::WriteFile(path0, "", 0));
  ASSERT_EQ(0, base::WriteFile(path1, "", 0));

  base::FilePath reserved_path;
  bool verified = false;
  DownloadPathReservationTracker::FilenameConflictAction conflict_action =
    DownloadPathReservationTracker::UNIQUIFY;
  bool create_directory = false;
  CallGetReservedPath(
      item.get(),
      path,
      create_directory,
      conflict_action,
      &reserved_path,
      &verified);
  EXPECT_TRUE(IsPathInUse(reserved_path));
  EXPECT_TRUE(verified);
  EXPECT_EQ(path2, reserved_path);
  SetDownloadItemState(item.get(), DownloadItem::COMPLETE);
}

TEST_F(DownloadPathReservationTrackerTest, TruncationFail) {
  int real_max_length =
      base::GetMaximumPathComponentLength(default_download_path());
  ASSERT_NE(-1, real_max_length);
  const size_t max_length = real_max_length - 11;

  scoped_ptr<MockDownloadItem> item(CreateDownloadItem(1));
  base::FilePath path(GetPathInDownloadsDirectory(
      (FILE_PATH_LITERAL("a.") +
          base::FilePath::StringType(max_length, 'b')).c_str()));
  ASSERT_FALSE(IsPathInUse(path));

  base::FilePath reserved_path;
  bool verified = false;
  DownloadPathReservationTracker::FilenameConflictAction conflict_action =
    DownloadPathReservationTracker::OVERWRITE;
  bool create_directory = false;
  CallGetReservedPath(
      item.get(),
      path,
      create_directory,
      conflict_action,
      &reserved_path,
      &verified);
  // We cannot truncate a path with very long extension.
  EXPECT_FALSE(verified);
  SetDownloadItemState(item.get(), DownloadItem::COMPLETE);
}

#endif
