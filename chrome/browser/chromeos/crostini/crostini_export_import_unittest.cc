// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/crostini/crostini_export_import.h"

#include "base/files/file_path.h"
#include "base/run_loop.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/chromeos/file_manager/path_util.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_cicerone_client.h"
#include "chromeos/dbus/fake_seneschal_client.h"
#include "chromeos/dbus/seneschal/seneschal_service.pb.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "storage/browser/fileapi/external_mount_points.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/message_center/public/cpp/notification.h"

namespace crostini {

class CrostiniExportImportTest : public testing::Test {
 public:
  void SendExportProgress(
      vm_tools::cicerone::ExportLxdContainerProgressSignal_Status status,
      int progress_percent) {
    vm_tools::cicerone::ExportLxdContainerProgressSignal signal;
    signal.set_owner_id(CryptohomeIdForProfile(profile()));
    signal.set_vm_name(kCrostiniDefaultVmName);
    signal.set_container_name(kCrostiniDefaultContainerName);
    signal.set_status(status);
    signal.set_progress_percent(progress_percent);
    fake_cicerone_client_->NotifyExportLxdContainerProgress(signal);
  }

  void SendImportProgress(
      vm_tools::cicerone::ImportLxdContainerProgressSignal_Status status,
      int progress_percent) {
    vm_tools::cicerone::ImportLxdContainerProgressSignal signal;
    signal.set_owner_id(CryptohomeIdForProfile(profile()));
    signal.set_vm_name(kCrostiniDefaultVmName);
    signal.set_container_name(kCrostiniDefaultContainerName);
    signal.set_status(status);
    signal.set_progress_percent(progress_percent);
    fake_cicerone_client_->NotifyImportLxdContainerProgress(signal);
  }

  CrostiniExportImportTest()
      : test_browser_thread_bundle_(
            content::TestBrowserThreadBundle::REAL_IO_THREAD) {
    chromeos::DBusThreadManager::Initialize();
    fake_seneschal_client_ = static_cast<chromeos::FakeSeneschalClient*>(
        chromeos::DBusThreadManager::Get()->GetSeneschalClient());
    fake_cicerone_client_ = static_cast<chromeos::FakeCiceroneClient*>(
        chromeos::DBusThreadManager::Get()->GetCiceroneClient());
  }

  ~CrostiniExportImportTest() override {
    chromeos::DBusThreadManager::Shutdown();
  }

  void SetUp() override {
    run_loop_ = std::make_unique<base::RunLoop>();
    profile_ = std::make_unique<TestingProfile>();
    crostini_export_import_ = std::make_unique<CrostiniExportImport>(profile());
    CrostiniManager::GetForProfile(profile())->AddRunningVmForTesting(
        kCrostiniDefaultVmName);
    container_id_ =
        ContainerId(kCrostiniDefaultVmName, kCrostiniDefaultContainerName);

    storage::ExternalMountPoints* mount_points =
        storage::ExternalMountPoints::GetSystemInstance();
    mount_points->RegisterFileSystem(
        file_manager::util::GetDownloadsMountPointName(profile()),
        storage::kFileSystemTypeNativeLocal, storage::FileSystemMountOption(),
        file_manager::util::GetMyFilesFolderForProfile(profile()));
    tarball_ = file_manager::util::GetMyFilesFolderForProfile(profile()).Append(
        "tarball.tar.gz");
  }

  void TearDown() override {
    run_loop_.reset();
    profile_.reset();
  }

 protected:
  base::RunLoop* run_loop() { return run_loop_.get(); }
  Profile* profile() { return profile_.get(); }
  CrostiniExportImport* crostini_export_import() {
    return crostini_export_import_.get();
  }

  // Owned by chromeos::DBusThreadManager
  chromeos::FakeCiceroneClient* fake_cicerone_client_;
  chromeos::FakeSeneschalClient* fake_seneschal_client_;

  std::unique_ptr<base::RunLoop> run_loop_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<CrostiniExportImport> crostini_export_import_;
  ContainerId container_id_;
  base::FilePath tarball_;

 private:
  content::TestBrowserThreadBundle test_browser_thread_bundle_;
  DISALLOW_COPY_AND_ASSIGN(CrostiniExportImportTest);
};

TEST_F(CrostiniExportImportTest, TestExportSuccess) {
  crostini_export_import()->FileSelected(
      tarball_, 0, reinterpret_cast<void*>(ExportImportType::EXPORT));
  run_loop_->RunUntilIdle();
  CrostiniExportImportNotification* notification =
      crostini_export_import()->GetNotificationForTesting(container_id_);
  EXPECT_NE(notification, nullptr);
  EXPECT_EQ(notification->get_status(),
            CrostiniExportImportNotification::Status::RUNNING);
  EXPECT_EQ(notification->get_notification()->progress(), 0);

  // 20% PACK = 10% overall.
  SendExportProgress(vm_tools::cicerone::
                         ExportLxdContainerProgressSignal_Status_EXPORTING_PACK,
                     20);
  EXPECT_EQ(notification->get_status(),
            CrostiniExportImportNotification::Status::RUNNING);
  EXPECT_EQ(notification->get_notification()->progress(), 10);

  // 20% DOWNLOAD = 60% overall.
  SendExportProgress(
      vm_tools::cicerone::
          ExportLxdContainerProgressSignal_Status_EXPORTING_DOWNLOAD,
      20);
  EXPECT_EQ(notification->get_status(),
            CrostiniExportImportNotification::Status::RUNNING);
  EXPECT_EQ(notification->get_notification()->progress(), 60);

  // Close notification and update progress.  Should not update notification.
  notification->Close(false);
  SendExportProgress(
      vm_tools::cicerone::
          ExportLxdContainerProgressSignal_Status_EXPORTING_DOWNLOAD,
      40);
  EXPECT_EQ(notification->get_status(),
            CrostiniExportImportNotification::Status::RUNNING);
  EXPECT_EQ(notification->get_notification()->progress(), 60);

  // Done.
  SendExportProgress(
      vm_tools::cicerone::ExportLxdContainerProgressSignal_Status_DONE, 0);
  EXPECT_EQ(notification->get_status(),
            CrostiniExportImportNotification::Status::DONE);
}

TEST_F(CrostiniExportImportTest, TestExportFail) {
  crostini_export_import()->FileSelected(
      tarball_, 0, reinterpret_cast<void*>(ExportImportType::EXPORT));
  run_loop_->RunUntilIdle();
  CrostiniExportImportNotification* notification =
      crostini_export_import()->GetNotificationForTesting(container_id_);

  // Failed.
  SendExportProgress(
      vm_tools::cicerone::ExportLxdContainerProgressSignal_Status_FAILED, 0);
  EXPECT_EQ(notification->get_status(),
            CrostiniExportImportNotification::Status::FAILED);
}

TEST_F(CrostiniExportImportTest, TestImportSuccess) {
  crostini_export_import()->FileSelected(
      tarball_, 0, reinterpret_cast<void*>(ExportImportType::IMPORT));
  run_loop_->RunUntilIdle();
  CrostiniExportImportNotification* notification =
      crostini_export_import()->GetNotificationForTesting(container_id_);
  EXPECT_NE(notification, nullptr);
  EXPECT_EQ(notification->get_status(),
            CrostiniExportImportNotification::Status::RUNNING);
  EXPECT_EQ(notification->get_notification()->progress(), 0);

  // 20% UPLOAD = 10% overall.
  SendImportProgress(
      vm_tools::cicerone::
          ImportLxdContainerProgressSignal_Status_IMPORTING_UPLOAD,
      20);
  EXPECT_EQ(notification->get_status(),
            CrostiniExportImportNotification::Status::RUNNING);
  EXPECT_EQ(notification->get_notification()->progress(), 10);

  // 20% UNPACK = 60% overall.
  SendImportProgress(
      vm_tools::cicerone::
          ImportLxdContainerProgressSignal_Status_IMPORTING_UNPACK,
      20);
  EXPECT_EQ(notification->get_status(),
            CrostiniExportImportNotification::Status::RUNNING);
  EXPECT_EQ(notification->get_notification()->progress(), 60);

  // Close notification and update progress.  Should not update notification.
  notification->Close(false);
  SendImportProgress(
      vm_tools::cicerone::
          ImportLxdContainerProgressSignal_Status_IMPORTING_UNPACK,
      40);
  EXPECT_EQ(notification->get_status(),
            CrostiniExportImportNotification::Status::RUNNING);
  EXPECT_EQ(notification->get_notification()->progress(), 60);

  // Done.
  SendImportProgress(
      vm_tools::cicerone::ImportLxdContainerProgressSignal_Status_DONE, 0);
  EXPECT_EQ(notification->get_status(),
            CrostiniExportImportNotification::Status::DONE);
}

TEST_F(CrostiniExportImportTest, TestImportFail) {
  crostini_export_import()->FileSelected(
      tarball_, 0, reinterpret_cast<void*>(ExportImportType::IMPORT));
  run_loop_->RunUntilIdle();
  CrostiniExportImportNotification* notification =
      crostini_export_import()->GetNotificationForTesting(container_id_);

  // Failed.
  SendImportProgress(
      vm_tools::cicerone::ImportLxdContainerProgressSignal_Status_FAILED, 0);
  EXPECT_EQ(notification->get_status(),
            CrostiniExportImportNotification::Status::FAILED);
}
}  // namespace crostini
