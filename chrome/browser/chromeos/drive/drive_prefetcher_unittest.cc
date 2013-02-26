// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/drive_prefetcher.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/message_loop.h"
#include "base/run_loop.h"
#include "chrome/browser/chromeos/drive/drive.pb.h"
#include "chrome/browser/chromeos/drive/drive_test_util.h"
#include "chrome/browser/chromeos/drive/event_logger.h"
#include "chrome/browser/chromeos/drive/mock_drive_file_system.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::AtMost;
using ::testing::StrictMock;
using ::testing::_;

namespace drive {

namespace {

// Enumeration values to represent the type of drive entries.
enum TestEntryType {
  TYPE_DIRECTORY,
  TYPE_REGULAR_FILE,
  TYPE_HOSTED_FILE,
};

// TestEntry represents a dummy entry for mocking a filesystem.
struct TestEntry {
  const base::FilePath::CharType* path;
  TestEntryType entry_type;
  int64 last_accessed;
  int64 last_modified;
  const char* resource_id;
  int64 file_size;

  // Checks whether this TestEntry is the direct content of the |directory|.
  bool IsDirectChildOf(const base::FilePath& directory) const {
    return base::FilePath(path).DirName() == directory;
  }

  // Converts this TestEntry to DriveEntryProto, which is the real data
  // structure used in DriveFileSystem.
  DriveEntryProto ToDriveEntryProto() const {
    DriveEntryProto entry;
    entry.set_base_name(base::FilePath(path).BaseName().value());
    entry.mutable_file_info()->set_is_directory(entry_type == TYPE_DIRECTORY);
    if (entry_type != TYPE_DIRECTORY) {
      entry.mutable_file_specific_info()->set_is_hosted_document(
          entry_type == TYPE_HOSTED_FILE);
      entry.mutable_file_info()->set_size(file_size);
    }
    entry.mutable_file_info()->set_last_accessed(last_accessed);
    entry.mutable_file_info()->set_last_modified(last_modified);
    entry.set_resource_id(resource_id);
    return entry;
  }
};

// Mocks DriveFileSystem::GetFileByResourceId. It records the requested
// resource_id (arg0) to |fetched_list|, and calls back a successful completion.
ACTION_P(MockGetFile, fetched_list) {
  fetched_list->push_back(arg0);
  arg2.Run(DRIVE_FILE_OK, base::FilePath(), std::string(), REGULAR_FILE);
}

// Mocks DriveFileSystem::ReadDirectory. It holds the flat list of all entries
// in the mock filesystem in |test_entries|, and when it is called to read a
// |directory|, it selects only the direct children of the directory.
ACTION_P(MockReadDirectory, test_entries) {
  const base::FilePath& directory = arg0;
  const ReadDirectoryWithSettingCallback& callback = arg1;

  scoped_ptr<DriveEntryProtoVector> entries(new DriveEntryProtoVector);
  for (size_t i = 0; i < test_entries.size(); ++i) {
    if (test_entries[i].IsDirectChildOf(directory))
      entries->push_back(test_entries[i].ToDriveEntryProto());
  }
  callback.Run(DRIVE_FILE_OK, false /* hide_hosted_document */, entries.Pass());
}

const TestEntry kEmptyDrive[] = {
  { FILE_PATH_LITERAL("drive"), TYPE_DIRECTORY, 0, 0, "id:drive" },
};

const TestEntry kOneFileDrive[] = {
  { FILE_PATH_LITERAL("drive"), TYPE_DIRECTORY, 0, 0, "id:drive" },
  { FILE_PATH_LITERAL("drive/abc.txt"), TYPE_REGULAR_FILE, 1, 0, "id:abc" },
};

const char* kExpectedOneFile[] = { "id:abc" };

const TestEntry kComplexDrive[] = {
  // Path                                  Type           Access Modify   ID
  { FILE_PATH_LITERAL("drive"),            TYPE_DIRECTORY,    0, 0, "id:root" },
  { FILE_PATH_LITERAL("drive/a"),          TYPE_DIRECTORY,    0, 0, "id:a" },
  { FILE_PATH_LITERAL("drive/a/foo.txt"),  TYPE_REGULAR_FILE, 3, 2, "id:foo1" },
  { FILE_PATH_LITERAL("drive/a/b"),        TYPE_DIRECTORY,    8, 0, "id:b" },
  { FILE_PATH_LITERAL("drive/a/bar.jpg"),  TYPE_REGULAR_FILE, 5, 0, "id:bar1",
                                           999 },
  { FILE_PATH_LITERAL("drive/a/b/x.gdoc"), TYPE_HOSTED_FILE,  7, 0, "id:new" },
  { FILE_PATH_LITERAL("drive/a/buz.zip"),  TYPE_REGULAR_FILE, 4, 0, "id:buz1" },
  { FILE_PATH_LITERAL("drive/a/old.gdoc"), TYPE_HOSTED_FILE,  1, 0, "id:old" },
  { FILE_PATH_LITERAL("drive/c"),          TYPE_DIRECTORY,    0, 0, "id:c" },
  { FILE_PATH_LITERAL("drive/c/foo.txt"),  TYPE_REGULAR_FILE, 3, 1, "id:foo2" },
  { FILE_PATH_LITERAL("drive/c/buz.zip"),  TYPE_REGULAR_FILE, 1, 0, "id:buz2" },
  { FILE_PATH_LITERAL("drive/bar.jpg"),    TYPE_REGULAR_FILE, 6, 0, "id:bar2" },
};

const char* kTop3Files[] = {
  "id:bar2",  // The file with the largest timestamp
              // "bar1" is the second latest, but its file size is over limit.
  "id:buz1",  // The third latest file.
  "id:foo1"   // 4th. Has same access time with id:foo2, so the one with the
              // newer modified time wins.
};

const char* kAllRegularFiles[] = {
  "id:bar2", "id:bar1", "id:buz1", "id:foo1", "id:foo2", "id:buz2",
};

}  // namespace

class DrivePrefetcherTest : public testing::Test {
 public:
  DrivePrefetcherTest()
      : dummy_event_logger_(0),
        ui_thread_(content::BrowserThread::UI, &message_loop_) {}

  virtual void SetUp() OVERRIDE {
    mock_file_system_.reset(new StrictMock<MockDriveFileSystem>);
  }

  virtual void TearDown() OVERRIDE {
    EXPECT_CALL(*mock_file_system_, RemoveObserver(_)).Times(AtMost(1));
    prefetcher_.reset();
    mock_file_system_.reset();
  }

 protected:
  // Sets a new prefetcher that fetches at most |prefetch_count| latest files.
  void InitPrefetcher(int prefetch_count, int64 size_limit) {
    EXPECT_CALL(*mock_file_system_, AddObserver(_)).Times(AtMost(1));

    DrivePrefetcherOptions options;
    options.initial_prefetch_count = prefetch_count;
    options.prefetch_file_size_limit = size_limit;
    prefetcher_.reset(new DrivePrefetcher(mock_file_system_.get(),
                                          &dummy_event_logger_,
                                          options));
  }

  // Flushes all the pending tasks on the current thread.
  void RunMessageLoop() {
    base::RunLoop run_loop;
    run_loop.RunUntilIdle();
  }

  // Verifies that fully running the prefetching loop over |test_entries|
  // correctly fetches the |expected| files, in the given order.
  void VerifyFullScan(const std::vector<TestEntry>& test_entries,
                      const std::vector<std::string>& expected) {
    std::vector<std::string> fetched_list;
    EXPECT_CALL(*mock_file_system_, ReadDirectoryByPath(_, _))
        .WillRepeatedly(MockReadDirectory(test_entries));
    EXPECT_CALL(*mock_file_system_, GetFileByResourceId(_, _, _, _)).Times(0);
    EXPECT_CALL(*mock_file_system_, GetFileByResourceId(_, _, _, _))
        .WillRepeatedly(MockGetFile(&fetched_list));
    prefetcher_->OnInitialLoadFinished(DRIVE_FILE_OK);
    RunMessageLoop();
    EXPECT_EQ(expected, fetched_list);
  }

  EventLogger dummy_event_logger_;
  scoped_ptr<StrictMock<MockDriveFileSystem> > mock_file_system_;
  scoped_ptr<DrivePrefetcher> prefetcher_;
  MessageLoopForUI message_loop_;
  content::TestBrowserThread ui_thread_;
};

TEST_F(DrivePrefetcherTest, ZeroFiles) {
  InitPrefetcher(3, 100);
  VerifyFullScan(
      std::vector<TestEntry>(kEmptyDrive,
                             kEmptyDrive + arraysize(kEmptyDrive)),
      std::vector<std::string>());
}

TEST_F(DrivePrefetcherTest, OneFile) {
  InitPrefetcher(3, 100);
  VerifyFullScan(
      std::vector<TestEntry>(kOneFileDrive,
                             kOneFileDrive + arraysize(kOneFileDrive)),
      std::vector<std::string>(kExpectedOneFile,
                               kExpectedOneFile + arraysize(kExpectedOneFile)));
}

TEST_F(DrivePrefetcherTest, MoreThanLimitFiles) {
  // Files with the largest timestamps should be listed, in the order of time.
  // Directories nor hosted files should not be listed.
  InitPrefetcher(3, 100);
  VerifyFullScan(
      std::vector<TestEntry>(kComplexDrive,
                             kComplexDrive + arraysize(kComplexDrive)),
      std::vector<std::string>(kTop3Files, kTop3Files + arraysize(kTop3Files)));
}

TEST_F(DrivePrefetcherTest, DirectoryTraversal) {
  // Ensure the prefetcher correctly traverses whole the file system tree.
  // This is checked by setting the fetch limit larger than the number of files.
  InitPrefetcher(100, 99999999);
  VerifyFullScan(
      std::vector<TestEntry>(kComplexDrive,
                             kComplexDrive + arraysize(kComplexDrive)),
      std::vector<std::string>(kAllRegularFiles,
                               kAllRegularFiles + arraysize(kAllRegularFiles)));
}

}  // namespace drive
