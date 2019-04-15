// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_manager/file_tasks_notifier.h"

#include <memory>

#include "base/scoped_observer.h"
#include "chrome/browser/chromeos/file_manager/file_tasks_observer.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "storage/browser/fileapi/file_system_url.h"
#include "storage/common/fileapi/file_system_types.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace file_manager {
namespace file_tasks {
namespace {

using testing::_;

storage::FileSystemURL CreateFileSystemUrl(
    const base::FilePath& path,
    storage::FileSystemType type = storage::kFileSystemTypeNativeLocal) {
  return storage::FileSystemURL::CreateForTest({}, {}, {}, "", type, path, "",
                                               {});
}

class MockFileTasksObserver : public file_tasks::FileTasksObserver {
 public:
  explicit MockFileTasksObserver(FileTasksNotifier* notifier)
      : observer_(this) {
    observer_.Add(notifier);
  }

  MOCK_METHOD2(OnFilesOpenedImpl,
               void(const base::FilePath& path, OpenType open_type));

  void OnFilesOpened(const std::vector<FileOpenEvent>& opens) {
    ASSERT_TRUE(!opens.empty());
    for (auto& open : opens) {
      OnFilesOpenedImpl(open.path, open.open_type);
    }
  }

 private:
  ScopedObserver<file_tasks::FileTasksNotifier, file_tasks::FileTasksObserver>
      observer_;
};

class FileTasksNotifierTest : public testing::Test {
 protected:
  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();
    notifier_ = std::make_unique<FileTasksNotifier>(profile_.get());
    observer_ = std::make_unique<MockFileTasksObserver>(notifier_.get());
  }

  void TearDown() override {
    observer_.reset();
    notifier_.reset();
    profile_.reset();
  }

  Profile& profile() { return *profile_; }
  MockFileTasksObserver& observer() { return *observer_; }
  FileTasksNotifier& notifier() { return *notifier_; }

 private:
  content::TestBrowserThreadBundle threads_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<FileTasksNotifier> notifier_;
  std::unique_ptr<MockFileTasksObserver> observer_;
};

TEST_F(FileTasksNotifierTest, FileTask_Local) {
  base::FilePath path = profile().GetPath().Append("file");
  EXPECT_CALL(observer(),
              OnFilesOpenedImpl(path, FileTasksObserver::OpenType::kLaunch));
  notifier().NotifyFileTasks({CreateFileSystemUrl(path)});
}

TEST_F(FileTasksNotifierTest, FileTask_DriveFs) {
  base::FilePath path("/media/fuse/drivefs-abcedf/root/file");
  EXPECT_CALL(observer(),
              OnFilesOpenedImpl(path, FileTasksObserver::OpenType::kLaunch));
  notifier().NotifyFileTasks(
      {CreateFileSystemUrl(path, storage::kFileSystemTypeDriveFs)});
}

TEST_F(FileTasksNotifierTest, FileTask_Arc) {
  base::FilePath path("/run/arc/sdcard/write/emulated/0/file");
  EXPECT_CALL(observer(),
              OnFilesOpenedImpl(path, FileTasksObserver::OpenType::kLaunch));
  notifier().NotifyFileTasks({CreateFileSystemUrl(path)});
}

TEST_F(FileTasksNotifierTest, FileTask_Crostini) {
  base::FilePath path("/media/fuse/crostini-abcdef/file");
  EXPECT_CALL(observer(),
              OnFilesOpenedImpl(path, FileTasksObserver::OpenType::kLaunch));
  notifier().NotifyFileTasks({CreateFileSystemUrl(path)});
}

TEST_F(FileTasksNotifierTest, FileTask_UnknownPath) {
  base::FilePath path("/some/other/path");
  EXPECT_CALL(observer(), OnFilesOpenedImpl(_, _)).Times(0);
  notifier().NotifyFileTasks({CreateFileSystemUrl(path)});
}

TEST_F(FileTasksNotifierTest, FileTask_RemovableMedia) {
  base::FilePath path("/media/removable/device/file");
  EXPECT_CALL(observer(), OnFilesOpenedImpl(_, _)).Times(0);
  notifier().NotifyFileTasks({CreateFileSystemUrl(path)});
}

TEST_F(FileTasksNotifierTest, FileTask_LegacyDrive) {
  base::FilePath path("/special/drive/root/file");
  EXPECT_CALL(observer(), OnFilesOpenedImpl(_, _)).Times(0);
  notifier().NotifyFileTasks(
      {CreateFileSystemUrl(path, storage::kFileSystemTypeDrive)});
}

TEST_F(FileTasksNotifierTest, FileTask_Multiple) {
  base::FilePath local_path = profile().GetPath().Append("file");
  base::FilePath drivefs_path("/media/fuse/drivefs-abcedf/root/file");
  base::FilePath arc_path("/run/arc/sdcard/write/emulated/0/file");
  base::FilePath crostini_path("/media/fuse/crostini-abcdef/file");
  base::FilePath unknown_path("/some/other/path");
  base::FilePath removable_path("/media/removable/device/file");
  base::FilePath legacy_drive_path("/special/drive/root/file");
  EXPECT_CALL(
      observer(),
      OnFilesOpenedImpl(local_path, FileTasksObserver::OpenType::kLaunch));
  EXPECT_CALL(
      observer(),
      OnFilesOpenedImpl(drivefs_path, FileTasksObserver::OpenType::kLaunch));
  EXPECT_CALL(observer(), OnFilesOpenedImpl(
                              arc_path, FileTasksObserver::OpenType::kLaunch));
  EXPECT_CALL(
      observer(),
      OnFilesOpenedImpl(crostini_path, FileTasksObserver::OpenType::kLaunch));
  notifier().NotifyFileTasks({
      CreateFileSystemUrl(local_path),
      CreateFileSystemUrl(drivefs_path, storage::kFileSystemTypeDriveFs),
      CreateFileSystemUrl(arc_path),
      CreateFileSystemUrl(crostini_path),
      CreateFileSystemUrl(unknown_path),
      CreateFileSystemUrl(removable_path),
      CreateFileSystemUrl(legacy_drive_path),
  });
}

}  // namespace
}  // namespace file_tasks
}  // namespace file_manager
