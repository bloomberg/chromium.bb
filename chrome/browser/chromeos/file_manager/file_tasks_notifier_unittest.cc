// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_manager/file_tasks_notifier.h"

#include <memory>
#include <string>
#include <utility>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/scoped_observer.h"
#include "base/test/bind_test_util.h"
#include "chrome/browser/chromeos/file_manager/file_tasks_observer.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/components/drivefs/mojom/drivefs.mojom-test-utils.h"
#include "components/drive/file_errors.h"
#include "content/public/test/fake_download_item.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "storage/browser/fileapi/external_mount_points.h"
#include "storage/browser/fileapi/file_system_url.h"
#include "storage/common/fileapi/file_system_types.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/shell_dialogs/selected_file_info.h"

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

ui::SelectedFileInfo CreateSelectedFileInfo(
    const base::FilePath& path,
    const base::FilePath& local_path = {}) {
  return ui::SelectedFileInfo(path, local_path);
}

class FakeDriveFs : public drivefs::mojom::DriveFsInterceptorForTesting {
 public:
  DriveFs* GetForwardingInterface() override {
    NOTREACHED();
    return nullptr;
  }

  void GetMetadata(const base::FilePath& path,
                   GetMetadataCallback callback) override {
    if (path.value().find("offline") != std::string::npos) {
      auto metadata = drivefs::mojom::FileMetadata::New();
      metadata->available_offline = path.value() == "available_offline";
      metadata->capabilities = drivefs::mojom::Capabilities::New();
      std::move(callback).Run(drive::FILE_ERROR_OK, std::move(metadata));
      return;
    }
    if (path.value() == "not_found") {
      std::move(callback).Run(drive::FILE_ERROR_NOT_FOUND, nullptr);
      return;
    }
    if (path.value() == "error") {
      std::move(callback).Run(drive::FILE_ERROR_SERVICE_UNAVAILABLE, nullptr);
      return;
    }
    FAIL() << "Unexpected DriveFS metadata request for " << path;
    std::move(callback).Run(drive::FILE_ERROR_INVALID_URL, nullptr);
  }
};

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

class FileTasksNotifierForTest : public FileTasksNotifier {
 public:
  FileTasksNotifierForTest(Profile* profile, drivefs::mojom::DriveFsPtr drivefs)
      : FileTasksNotifier(profile), drivefs_(std::move(drivefs)) {}

  drivefs::mojom::DriveFs* GetDriveFsInterface() override {
    return drivefs_.get();
  }

  bool GetRelativeDrivePath(const base::FilePath& path,
                            base::FilePath* drive_relative_path) override {
    *drive_relative_path = path.BaseName();
    return true;
  }

  bool IsOffline() override { return is_offline_; }

  void set_is_offline(bool is_offline) { is_offline_ = is_offline; }

 private:
  const drivefs::mojom::DriveFsPtr drivefs_;
  bool is_offline_ = false;
};

class FileTasksNotifierTest : public testing::Test {
 protected:
  FileTasksNotifierTest() : drivefs_binding_(&fake_drivefs_) {}

  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();

    drivefs::mojom::DriveFsPtr fake_drivefs_ptr;
    drivefs_binding_.Bind(mojo::MakeRequest(&fake_drivefs_ptr));
    notifier_ = std::make_unique<FileTasksNotifierForTest>(
        profile_.get(), std::move(fake_drivefs_ptr));
    observer_ = std::make_unique<MockFileTasksObserver>(notifier_.get());

    auto* mount_points = storage::ExternalMountPoints::GetSystemInstance();
    my_files_ = profile().GetPath().Append("MyFiles");
    ASSERT_TRUE(base::CreateDirectory(my_files_));
    base::WriteFile(my_files_.Append("file"), "data", 4);
    ASSERT_TRUE(mount_points->RegisterFileSystem(
        "downloads", storage::kFileSystemTypeNativeLocal, {},
        profile().GetPath().Append("MyFiles")));
    ASSERT_TRUE(mount_points->RegisterFileSystem(
        "drivefs", storage::kFileSystemTypeDriveFs, {},
        base::FilePath("/media/fuse/drivefs")));
    ASSERT_TRUE(
        mount_points->RegisterFileSystem("drive", storage::kFileSystemTypeDrive,
                                         {}, base::FilePath("/special/drive")));
  }

  void TearDown() override {
    auto* mount_points = storage::ExternalMountPoints::GetSystemInstance();
    mount_points->RevokeFileSystem("downloads");
    mount_points->RevokeFileSystem("drivefs");
    mount_points->RevokeFileSystem("drive");

    observer_.reset();
    notifier_.reset();
    profile_.reset();
  }

  Profile& profile() { return *profile_; }
  MockFileTasksObserver& observer() { return *observer_; }
  FileTasksNotifierForTest& notifier() { return *notifier_; }

  download::DownloadItem* CreateCompletedDownloadItem(
      const base::FilePath& path) {
    download_item_ = std::make_unique<content::FakeDownloadItem>();
    download_item_->SetTargetFilePath(path);
    download_item_->SetState(download::DownloadItem::DownloadState::COMPLETE);
    return download_item_.get();
  }

  const base::FilePath& my_files() { return my_files_; }

 private:
  content::TestBrowserThreadBundle threads_;
  FakeDriveFs fake_drivefs_;
  mojo::Binding<drivefs::mojom::DriveFs> drivefs_binding_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<FileTasksNotifierForTest> notifier_;
  std::unique_ptr<MockFileTasksObserver> observer_;
  std::unique_ptr<content::FakeDownloadItem> download_item_;
  base::FilePath my_files_;
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

TEST_F(FileTasksNotifierTest, DialogSelection_Local) {
  base::FilePath path = profile().GetPath().Append("file");
  EXPECT_CALL(observer(),
              OnFilesOpenedImpl(path, FileTasksObserver::OpenType::kOpen));
  notifier().NotifyFileDialogSelection({CreateSelectedFileInfo(path)}, true);

  EXPECT_CALL(observer(),
              OnFilesOpenedImpl(path, FileTasksObserver::OpenType::kSaveAs));
  notifier().NotifyFileDialogSelection({CreateSelectedFileInfo(path)}, false);
}

TEST_F(FileTasksNotifierTest, DialogSelection_DriveFs) {
  base::FilePath path("/media/fuse/drivefs-abcdef/root/file");
  EXPECT_CALL(observer(),
              OnFilesOpenedImpl(path, FileTasksObserver::OpenType::kOpen));
  notifier().NotifyFileDialogSelection({CreateSelectedFileInfo(path)}, true);

  EXPECT_CALL(observer(),
              OnFilesOpenedImpl(path, FileTasksObserver::OpenType::kSaveAs));
  notifier().NotifyFileDialogSelection({CreateSelectedFileInfo(path)}, false);
}

TEST_F(FileTasksNotifierTest, DialogSelection_Arc) {
  base::FilePath path("/run/arc/sdcard/write/emulated/0/file");
  EXPECT_CALL(observer(),
              OnFilesOpenedImpl(path, FileTasksObserver::OpenType::kOpen));
  notifier().NotifyFileDialogSelection({CreateSelectedFileInfo(path)}, true);

  EXPECT_CALL(observer(),
              OnFilesOpenedImpl(path, FileTasksObserver::OpenType::kSaveAs));
  notifier().NotifyFileDialogSelection({CreateSelectedFileInfo(path)}, false);
}

TEST_F(FileTasksNotifierTest, DialogSelection_Crostini) {
  base::FilePath path("/media/fuse/crostini-abcdef/file");
  EXPECT_CALL(observer(),
              OnFilesOpenedImpl(path, FileTasksObserver::OpenType::kOpen));
  notifier().NotifyFileDialogSelection({CreateSelectedFileInfo(path)}, true);

  EXPECT_CALL(observer(),
              OnFilesOpenedImpl(path, FileTasksObserver::OpenType::kSaveAs));
  notifier().NotifyFileDialogSelection({CreateSelectedFileInfo(path)}, false);
}

TEST_F(FileTasksNotifierTest, DialogSelection_UnknownPath) {
  base::FilePath path("/some/other/path");
  EXPECT_CALL(observer(), OnFilesOpenedImpl(_, _)).Times(0);
  notifier().NotifyFileDialogSelection({CreateSelectedFileInfo(path)}, true);
  notifier().NotifyFileDialogSelection({CreateSelectedFileInfo(path)}, false);
}

TEST_F(FileTasksNotifierTest, DialogSelection_RemovableMedia) {
  base::FilePath path("/media/removable/device/file");
  EXPECT_CALL(observer(), OnFilesOpenedImpl(_, _)).Times(0);
  notifier().NotifyFileDialogSelection({CreateSelectedFileInfo(path)}, true);
  notifier().NotifyFileDialogSelection({CreateSelectedFileInfo(path)}, false);
}

TEST_F(FileTasksNotifierTest, DialogSelection_LegacyDrive) {
  base::FilePath path("/special/drive/root/file");
  base::FilePath local_path =
      profile().GetPath().Append("GCache/v1/files/file");
  EXPECT_CALL(observer(), OnFilesOpenedImpl(_, _)).Times(0);
  notifier().NotifyFileDialogSelection(
      {CreateSelectedFileInfo(path, local_path), CreateSelectedFileInfo(path)},
      true);
  notifier().NotifyFileDialogSelection(
      {CreateSelectedFileInfo(path, local_path), CreateSelectedFileInfo(path)},
      false);
}

TEST_F(FileTasksNotifierTest, DialogSelection_Multiple) {
  base::FilePath local_path = profile().GetPath().Append("file");
  base::FilePath drivefs_path("/media/fuse/drivefs-abcdef/root/file");
  base::FilePath arc_path("/run/arc/sdcard/write/emulated/0/file");
  base::FilePath crostini_path("/media/fuse/crostini-abcdef/file");
  base::FilePath unknown_path("/some/other/path");
  base::FilePath removable_path("/media/removable/device/file");
  base::FilePath legacy_drive_path("/special/drive/root/file");
  base::FilePath legacy_drive_local_path =
      profile().GetPath().Append("GCache/v1/files/file");
  EXPECT_CALL(observer(), OnFilesOpenedImpl(
                              local_path, FileTasksObserver::OpenType::kOpen));
  EXPECT_CALL(
      observer(),
      OnFilesOpenedImpl(drivefs_path, FileTasksObserver::OpenType::kOpen));
  EXPECT_CALL(observer(),
              OnFilesOpenedImpl(arc_path, FileTasksObserver::OpenType::kOpen));
  EXPECT_CALL(
      observer(),
      OnFilesOpenedImpl(crostini_path, FileTasksObserver::OpenType::kOpen));

  notifier().NotifyFileDialogSelection(
      {CreateSelectedFileInfo(local_path), CreateSelectedFileInfo(drivefs_path),
       CreateSelectedFileInfo(arc_path), CreateSelectedFileInfo(crostini_path),
       CreateSelectedFileInfo(unknown_path),
       CreateSelectedFileInfo(legacy_drive_path, legacy_drive_local_path),
       CreateSelectedFileInfo(legacy_drive_path),
       CreateSelectedFileInfo(removable_path)},
      true);

  EXPECT_CALL(
      observer(),
      OnFilesOpenedImpl(local_path, FileTasksObserver::OpenType::kSaveAs));
  EXPECT_CALL(
      observer(),
      OnFilesOpenedImpl(drivefs_path, FileTasksObserver::OpenType::kSaveAs));
  EXPECT_CALL(observer(), OnFilesOpenedImpl(
                              arc_path, FileTasksObserver::OpenType::kSaveAs));
  EXPECT_CALL(
      observer(),
      OnFilesOpenedImpl(crostini_path, FileTasksObserver::OpenType::kSaveAs));

  notifier().NotifyFileDialogSelection(
      {CreateSelectedFileInfo(local_path), CreateSelectedFileInfo(drivefs_path),
       CreateSelectedFileInfo(arc_path), CreateSelectedFileInfo(crostini_path),
       CreateSelectedFileInfo(unknown_path),
       CreateSelectedFileInfo(legacy_drive_path, legacy_drive_local_path),
       CreateSelectedFileInfo(legacy_drive_path),
       CreateSelectedFileInfo(removable_path)},
      false);
}

TEST_F(FileTasksNotifierTest, Download_Local) {
  base::FilePath path = profile().GetPath().Append("file");
  EXPECT_CALL(observer(),
              OnFilesOpenedImpl(path, FileTasksObserver::OpenType::kDownload));
  notifier().OnDownloadUpdated(nullptr, CreateCompletedDownloadItem(path));
}

TEST_F(FileTasksNotifierTest, Download_DriveFs) {
  base::FilePath path("/media/fuse/drivefs/root/file");
  EXPECT_CALL(observer(),
              OnFilesOpenedImpl(path, FileTasksObserver::OpenType::kDownload));
  notifier().OnDownloadUpdated(nullptr, CreateCompletedDownloadItem(path));
}

TEST_F(FileTasksNotifierTest, Download_Arc) {
  base::FilePath path("/run/arc/sdcard/write/emulated/0/file");
  EXPECT_CALL(observer(),
              OnFilesOpenedImpl(path, FileTasksObserver::OpenType::kDownload));
  notifier().OnDownloadUpdated(nullptr, CreateCompletedDownloadItem(path));
}

TEST_F(FileTasksNotifierTest, Download_Crostini) {
  base::FilePath path("/media/fuse/crostini-abcdef/file");
  EXPECT_CALL(observer(),
              OnFilesOpenedImpl(path, FileTasksObserver::OpenType::kDownload));
  notifier().OnDownloadUpdated(nullptr, CreateCompletedDownloadItem(path));
}

TEST_F(FileTasksNotifierTest, Download_UnknownPath) {
  base::FilePath path("/some/other/path");
  EXPECT_CALL(observer(), OnFilesOpenedImpl(_, _)).Times(0);
  notifier().OnDownloadUpdated(nullptr, CreateCompletedDownloadItem(path));
}

TEST_F(FileTasksNotifierTest, Download_RemovableMedia) {
  base::FilePath path("/media/removable/device/file");
  EXPECT_CALL(observer(), OnFilesOpenedImpl(_, _)).Times(0);
  notifier().OnDownloadUpdated(nullptr, CreateCompletedDownloadItem(path));
}

TEST_F(FileTasksNotifierTest, Download_LegacyDrive) {
  base::FilePath path("/special/drive/root/file");
  EXPECT_CALL(observer(), OnFilesOpenedImpl(_, _)).Times(0);
  notifier().OnDownloadUpdated(nullptr, CreateCompletedDownloadItem(path));
}

TEST_F(FileTasksNotifierTest, Download_Incomplete) {
  EXPECT_CALL(observer(), OnFilesOpenedImpl(_, _)).Times(0);
  content::FakeDownloadItem download_item;
  download_item.SetTargetFilePath(profile().GetPath().Append("file"));

  for (auto state : {download::DownloadItem::DownloadState::IN_PROGRESS,
                     download::DownloadItem::DownloadState::CANCELLED,
                     download::DownloadItem::DownloadState::INTERRUPTED}) {
    download_item.SetState(state);
    notifier().OnDownloadUpdated(nullptr, &download_item);
  }
}

TEST_F(FileTasksNotifierTest, QueryFileAvailability_NotFound) {
  base::RunLoop run_loop;
  notifier().QueryFileAvailability(
      {my_files().Append("not_found.txt")},
      base::BindLambdaForTesting(
          [&](std::vector<FileTasksNotifier::FileAvailability> results) {
            run_loop.Quit();
            ASSERT_EQ(1u, results.size());
            EXPECT_EQ(FileTasksNotifier::FileAvailability::kGone, results[0]);
          }));
  run_loop.Run();
}

TEST_F(FileTasksNotifierTest, QueryFileAvailability_FileExists) {
  base::RunLoop run_loop;
  notifier().QueryFileAvailability(
      {my_files().Append("file")},
      base::BindLambdaForTesting(
          [&](std::vector<FileTasksNotifier::FileAvailability> results) {
            run_loop.Quit();
            ASSERT_EQ(1u, results.size());
            EXPECT_EQ(FileTasksNotifier::FileAvailability::kOk, results[0]);
          }));
  run_loop.Run();
}

TEST_F(FileTasksNotifierTest, QueryFileAvailability_UnsupportedMountType) {
  base::RunLoop run_loop;
  notifier().QueryFileAvailability(
      {base::FilePath("/special/drive/root/file")},
      base::BindLambdaForTesting(
          [&](std::vector<FileTasksNotifier::FileAvailability> results) {
            run_loop.Quit();
            ASSERT_EQ(1u, results.size());
            EXPECT_EQ(FileTasksNotifier::FileAvailability::kGone, results[0]);
          }));
  run_loop.Run();
}

TEST_F(FileTasksNotifierTest, QueryFileAvailability_OutsideMounts) {
  base::RunLoop run_loop;
  notifier().QueryFileAvailability(
      {base::FilePath("/media/fuse/crostini-abcdef/file")},
      base::BindLambdaForTesting(
          [&](std::vector<FileTasksNotifier::FileAvailability> results) {
            run_loop.Quit();
            ASSERT_EQ(1u, results.size());
            EXPECT_EQ(FileTasksNotifier::FileAvailability::kUnknown,
                      results[0]);
          }));
  run_loop.Run();
}

TEST_F(FileTasksNotifierTest, QueryFileAvailability_DriveFsAvailableOffline) {
  base::RunLoop run_loop;
  notifier().QueryFileAvailability(
      {base::FilePath("/media/fuse/drivefs/root/available_offline")},
      base::BindLambdaForTesting(
          [&](std::vector<FileTasksNotifier::FileAvailability> results) {
            run_loop.Quit();
            ASSERT_EQ(1u, results.size());
            EXPECT_EQ(FileTasksNotifier::FileAvailability::kOk, results[0]);
          }));
  run_loop.Run();
}

TEST_F(FileTasksNotifierTest, QueryFileAvailability_DriveFsUnavailableOffline) {
  base::RunLoop run_loop;
  notifier().QueryFileAvailability(
      {base::FilePath("/media/fuse/drivefs/root/unavailable_offline")},
      base::BindLambdaForTesting(
          [&](std::vector<FileTasksNotifier::FileAvailability> results) {
            run_loop.Quit();
            ASSERT_EQ(1u, results.size());
            EXPECT_EQ(FileTasksNotifier::FileAvailability::kOk, results[0]);
          }));
  run_loop.Run();
}

TEST_F(FileTasksNotifierTest,
       QueryFileAvailability_DriveFsUnavailableOfflineWhileOffline) {
  notifier().set_is_offline(true);

  base::RunLoop run_loop;
  notifier().QueryFileAvailability(
      {base::FilePath("/media/fuse/drivefs/root/unavailable_offline")},
      base::BindLambdaForTesting(
          [&](std::vector<FileTasksNotifier::FileAvailability> results) {
            run_loop.Quit();
            ASSERT_EQ(1u, results.size());
            EXPECT_EQ(
                FileTasksNotifier::FileAvailability::kTemporarilyUnavailable,
                results[0]);
          }));
  run_loop.Run();
}

TEST_F(FileTasksNotifierTest, QueryFileAvailability_DriveFsNotFound) {
  base::RunLoop run_loop;
  notifier().QueryFileAvailability(
      {base::FilePath("/media/fuse/drivefs/root/not_found")},
      base::BindLambdaForTesting(
          [&](std::vector<FileTasksNotifier::FileAvailability> results) {
            run_loop.Quit();
            ASSERT_EQ(1u, results.size());
            EXPECT_EQ(FileTasksNotifier::FileAvailability::kGone, results[0]);
          }));
  run_loop.Run();
}

TEST_F(FileTasksNotifierTest, QueryFileAvailability_DriveFsServiceError) {
  base::RunLoop run_loop;
  notifier().QueryFileAvailability(
      {base::FilePath("/media/fuse/drivefs/root/error")},
      base::BindLambdaForTesting(
          [&](std::vector<FileTasksNotifier::FileAvailability> results) {
            run_loop.Quit();
            ASSERT_EQ(1u, results.size());
            EXPECT_EQ(FileTasksNotifier::FileAvailability::kUnknown,
                      results[0]);
          }));
  run_loop.Run();
}

TEST_F(FileTasksNotifierTest, QueryFileAvailability_Multiple) {
  notifier().set_is_offline(true);

  base::RunLoop run_loop;
  notifier().QueryFileAvailability(
      {my_files().Append("not_found"), my_files().Append("file"),
       base::FilePath("/special/drive/root/file"),
       base::FilePath("/media/fuse/crostini-abcdef/file"),
       base::FilePath("/media/fuse/drivefs/root/available_offline"),
       base::FilePath("/media/fuse/drivefs/root/unavailable_offline"),
       base::FilePath("/media/fuse/drivefs/root/not_found"),
       base::FilePath("/media/fuse/drivefs/root/error")},
      base::BindLambdaForTesting(
          [&](std::vector<FileTasksNotifier::FileAvailability> results) {
            run_loop.Quit();
            EXPECT_EQ((std::vector<FileTasksNotifier::FileAvailability>{
                          FileTasksNotifier::FileAvailability::kGone,
                          FileTasksNotifier::FileAvailability::kOk,
                          FileTasksNotifier::FileAvailability::kGone,
                          FileTasksNotifier::FileAvailability::kUnknown,
                          FileTasksNotifier::FileAvailability::kOk,
                          FileTasksNotifier::FileAvailability::
                              kTemporarilyUnavailable,
                          FileTasksNotifier::FileAvailability::kGone,
                          FileTasksNotifier::FileAvailability::kUnknown}),
                      results);
          }));
  run_loop.Run();
}

}  // namespace
}  // namespace file_tasks
}  // namespace file_manager
