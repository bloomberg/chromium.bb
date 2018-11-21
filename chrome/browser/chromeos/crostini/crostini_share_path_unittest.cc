// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/crostini/crostini_share_path.h"
#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_task_environment.h"
#include "chrome/browser/chromeos/crostini/crostini_manager.h"
#include "chrome/browser/chromeos/crostini/crostini_pref_names.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/chromeos/file_manager/path_util.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/chromeos_features.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_cicerone_client.h"
#include "chromeos/dbus/fake_concierge_client.h"
#include "chromeos/dbus/fake_seneschal_client.h"
#include "chromeos/dbus/seneschal/seneschal_service.pb.h"
#include "components/drive/drive_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "storage/browser/fileapi/external_mount_points.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace crostini {

class CrostiniSharePathTest : public testing::Test {
 public:
  const bool PERSIST_YES = true;
  const bool PERSIST_NO = false;
  enum class Persist { NO, YES };
  enum class SeneschalClientCalled { NO, YES };
  enum class Success { NO, YES };

  void SharePathCallback(
      Persist expected_persist,
      SeneschalClientCalled expected_seneschal_client_called,
      const vm_tools::seneschal::SharePathRequest::StorageLocation*
          expected_seneschal_storage_location,
      std::string expected_seneschal_path,
      Success expected_success,
      std::string expected_failure_reason,
      bool success,
      std::string failure_reason) {
    const base::ListValue* prefs =
        profile()->GetPrefs()->GetList(prefs::kCrostiniSharedPaths);
    std::string share_path;
    if (expected_persist == Persist::YES) {
      EXPECT_EQ(prefs->GetSize(), 2U);
      prefs->GetString(0, &share_path);
      EXPECT_EQ(share_path, shared_path_.value());
      prefs->GetString(1, &share_path);
      EXPECT_EQ(share_path, share_path_.value());
    } else {
      EXPECT_EQ(prefs->GetSize(), 1U);
      prefs->GetString(0, &share_path);
      EXPECT_EQ(share_path, shared_path_.value());
    }
    EXPECT_EQ(fake_seneschal_client_->share_path_called(),
              expected_seneschal_client_called == SeneschalClientCalled::YES);
    if (expected_seneschal_client_called == SeneschalClientCalled::YES) {
      EXPECT_EQ(
          fake_seneschal_client_->last_share_path_request().storage_location(),
          *expected_seneschal_storage_location);
      EXPECT_EQ(fake_seneschal_client_->last_share_path_request()
                    .shared_path()
                    .path(),
                expected_seneschal_path);
    }
    EXPECT_EQ(success, expected_success == Success::YES);
    EXPECT_EQ(failure_reason, expected_failure_reason);
    run_loop()->Quit();
  }

  void SharePersistedPathsCallback(bool success, std::string failure_reason) {
    EXPECT_TRUE(success);
    EXPECT_EQ(
        profile()->GetPrefs()->GetList(prefs::kCrostiniSharedPaths)->GetSize(),
        2U);
    run_loop()->Quit();
  }

  void SharePathErrorVmNotRunningCallback(base::OnceClosure closure,
                                          bool success,
                                          std::string failure_reason) {
    EXPECT_FALSE(fake_seneschal_client_->share_path_called());
    EXPECT_EQ(success, false);
    EXPECT_EQ(failure_reason, "Cannot share, VM not running");
    std::move(closure).Run();
  }

  void UnsharePathCallback(
      const base::FilePath& path,
      SeneschalClientCalled expected_seneschal_client_called,
      std::string expected_seneschal_path,
      Success expected_success,
      std::string expected_failure_reason,
      bool success,
      std::string failure_reason) {
    const base::ListValue* prefs =
        profile()->GetPrefs()->GetList(prefs::kCrostiniSharedPaths);
    EXPECT_EQ(prefs->Find(base::Value(path.value())), prefs->end());
    EXPECT_EQ(fake_seneschal_client_->unshare_path_called(),
              expected_seneschal_client_called == SeneschalClientCalled::YES);
    if (expected_seneschal_client_called == SeneschalClientCalled::YES) {
      EXPECT_EQ(fake_seneschal_client_->last_unshare_path_request().path(),
                expected_seneschal_path);
    }
    EXPECT_EQ(success, expected_success == Success::YES);
    EXPECT_EQ(failure_reason, expected_failure_reason);
    run_loop()->Quit();
  }

  CrostiniSharePathTest()
      : scoped_task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::UI),
        test_browser_thread_bundle_(
            content::TestBrowserThreadBundle::REAL_IO_THREAD) {
    chromeos::DBusThreadManager::Initialize();
    fake_concierge_client_ = static_cast<chromeos::FakeConciergeClient*>(
        chromeos::DBusThreadManager::Get()->GetConciergeClient());
    fake_seneschal_client_ = static_cast<chromeos::FakeSeneschalClient*>(
        chromeos::DBusThreadManager::Get()->GetSeneschalClient());
  }

  ~CrostiniSharePathTest() override { chromeos::DBusThreadManager::Shutdown(); }

  void SetUp() override {
    run_loop_ = std::make_unique<base::RunLoop>();
    profile_ = std::make_unique<TestingProfile>();
    crostini_share_path_ = std::make_unique<CrostiniSharePath>(profile());

    // Setup for DriveFS.
    user_manager.AddUser(AccountId::FromUserEmailGaiaId(
        profile()->GetProfileUserName(), "12345"));
    profile()->GetPrefs()->SetString(drive::prefs::kDriveFsProfileSalt, "a");
    drivefs_ =
        base::FilePath("/media/fuse/drivefs-84675c855b63e12f384d45f033826980");

    // Setup Downloads and path to share.
    storage::ExternalMountPoints* mount_points =
        storage::ExternalMountPoints::GetSystemInstance();
    mount_points->RegisterFileSystem(
        file_manager::util::GetDownloadsMountPointName(profile()),
        storage::kFileSystemTypeNativeLocal, storage::FileSystemMountOption(),
        file_manager::util::GetDownloadsFolderForProfile(profile()));
    downloads_ = file_manager::util::GetDownloadsFolderForProfile(profile());
    share_path_ = downloads_.Append("path-to-share");
    shared_path_ = downloads_.Append("already-shared");
    ListPrefUpdate update(profile()->GetPrefs(),
                          crostini::prefs::kCrostiniSharedPaths);
    base::ListValue* shared_paths = update.Get();
    shared_paths->Append(std::make_unique<base::Value>(shared_path_.value()));

    // Create 'vm-running' VM instance which is running.
    vm_tools::concierge::VmInfo vm_info;
    CrostiniManager::GetForProfile(profile())->AddRunningVmForTesting(
        "vm-running", vm_info);
  }

  void TearDown() override {
    run_loop_.reset();
    profile_.reset();
  }

 protected:
  base::RunLoop* run_loop() { return run_loop_.get(); }
  Profile* profile() { return profile_.get(); }
  CrostiniSharePath* crostini_share_path() {
    return crostini_share_path_.get();
  }
  base::FilePath downloads_;
  base::FilePath share_path_;
  base::FilePath shared_path_;
  base::FilePath drivefs_;

  // Owned by chromeos::DBusThreadManager
  chromeos::FakeSeneschalClient* fake_seneschal_client_;
  chromeos::FakeConciergeClient* fake_concierge_client_;

  std::unique_ptr<base::RunLoop>
      run_loop_;  // run_loop_ must be created on the UI thread.
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<CrostiniSharePath> crostini_share_path_;
  base::test::ScopedFeatureList features_;
  chromeos::FakeChromeUserManager user_manager;

 private:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  content::TestBrowserThreadBundle test_browser_thread_bundle_;
  DISALLOW_COPY_AND_ASSIGN(CrostiniSharePathTest);
};

TEST_F(CrostiniSharePathTest, SuccessDownloadsRoot) {
  crostini_share_path()->SharePath(
      "vm-running", downloads_, PERSIST_NO,
      base::BindOnce(&CrostiniSharePathTest::SharePathCallback,
                     base::Unretained(this), Persist::NO,
                     SeneschalClientCalled::YES,
                     &vm_tools::seneschal::SharePathRequest::DOWNLOADS, "",
                     Success::YES, ""));
  run_loop()->Run();
}

TEST_F(CrostiniSharePathTest, SuccessMyFilesRoot) {
  features_.InitAndEnableFeature(chromeos::features::kMyFilesVolume);
  base::FilePath my_files =
      file_manager::util::GetMyFilesFolderForProfile(profile());
  crostini_share_path()->SharePath(
      "vm-running", my_files, PERSIST_NO,
      base::BindOnce(&CrostiniSharePathTest::SharePathCallback,
                     base::Unretained(this), Persist::NO,
                     SeneschalClientCalled::YES,
                     &vm_tools::seneschal::SharePathRequest::MY_FILES, "",
                     Success::YES, ""));
  run_loop()->Run();
}

TEST_F(CrostiniSharePathTest, SuccessNoPersist) {
  crostini_share_path()->SharePath(
      "vm-running", share_path_, PERSIST_NO,
      base::BindOnce(&CrostiniSharePathTest::SharePathCallback,
                     base::Unretained(this), Persist::NO,
                     SeneschalClientCalled::YES,
                     &vm_tools::seneschal::SharePathRequest::DOWNLOADS,
                     "path-to-share", Success::YES, ""));
  run_loop()->Run();
}

TEST_F(CrostiniSharePathTest, SuccessPersist) {
  crostini_share_path()->SharePath(
      "vm-running", share_path_, PERSIST_YES,
      base::BindOnce(&CrostiniSharePathTest::SharePathCallback,
                     base::Unretained(this), Persist::YES,
                     SeneschalClientCalled::YES,
                     &vm_tools::seneschal::SharePathRequest::DOWNLOADS,
                     "path-to-share", Success::YES, ""));
  run_loop()->Run();
}

TEST_F(CrostiniSharePathTest, SuccessDriveFsMyDrive) {
  features_.InitAndEnableFeature(chromeos::features::kDriveFs);
  crostini_share_path()->SharePath(
      "vm-running", drivefs_.Append("root").Append("my"), PERSIST_NO,
      base::BindOnce(&CrostiniSharePathTest::SharePathCallback,
                     base::Unretained(this), Persist::NO,
                     SeneschalClientCalled::YES,
                     &vm_tools::seneschal::SharePathRequest::DRIVEFS_MY_DRIVE,
                     "my", Success::YES, ""));
  run_loop()->Run();
}

TEST_F(CrostiniSharePathTest, FailureDriveFsDisabled) {
  features_.InitAndDisableFeature(chromeos::features::kDriveFs);
  crostini_share_path()->SharePath(
      "vm-running", drivefs_.Append("root").Append("my"), PERSIST_NO,
      base::BindOnce(&CrostiniSharePathTest::SharePathCallback,
                     base::Unretained(this), Persist::NO,
                     SeneschalClientCalled::NO, nullptr, "my", Success::NO,
                     "Path is not allowed"));
  run_loop()->Run();
}

TEST_F(CrostiniSharePathTest, SuccessDriveFsMyDriveRoot) {
  features_.InitAndEnableFeature(chromeos::features::kDriveFs);
  crostini_share_path()->SharePath(
      "vm-running", drivefs_.Append("root"), PERSIST_NO,
      base::BindOnce(&CrostiniSharePathTest::SharePathCallback,
                     base::Unretained(this), Persist::NO,
                     SeneschalClientCalled::YES,
                     &vm_tools::seneschal::SharePathRequest::DRIVEFS_MY_DRIVE,
                     "", Success::YES, ""));
  run_loop()->Run();
}

TEST_F(CrostiniSharePathTest, FailDriveFsRoot) {
  features_.InitAndEnableFeature(chromeos::features::kDriveFs);
  crostini_share_path()->SharePath(
      "vm-running", drivefs_, PERSIST_NO,
      base::BindOnce(&CrostiniSharePathTest::SharePathCallback,
                     base::Unretained(this), Persist::NO,
                     SeneschalClientCalled::NO, nullptr, "", Success::NO,
                     "Path is not allowed"));
  run_loop()->Run();
}

TEST_F(CrostiniSharePathTest, SuccessDriveFsTeamDrives) {
  features_.InitAndEnableFeature(chromeos::features::kDriveFs);
  crostini_share_path()->SharePath(
      "vm-running", drivefs_.Append("team_drives").Append("team"), PERSIST_NO,
      base::BindOnce(
          &CrostiniSharePathTest::SharePathCallback, base::Unretained(this),
          Persist::NO, SeneschalClientCalled::YES,
          &vm_tools::seneschal::SharePathRequest::DRIVEFS_TEAM_DRIVES, "team",
          Success::YES, ""));
  run_loop()->Run();
}

TEST_F(CrostiniSharePathTest, SuccessDriveFsComputers) {
  features_.InitAndEnableFeature(chromeos::features::kDriveFs);
  crostini_share_path()->SharePath(
      "vm-running", drivefs_.Append("Computers").Append("pc"), PERSIST_NO,
      base::BindOnce(&CrostiniSharePathTest::SharePathCallback,
                     base::Unretained(this), Persist::NO,
                     SeneschalClientCalled::YES,
                     &vm_tools::seneschal::SharePathRequest::DRIVEFS_COMPUTERS,
                     "pc", Success::YES, ""));
  run_loop()->Run();
}

TEST_F(CrostiniSharePathTest, FailDriveFsTrash) {
  features_.InitAndEnableFeature(chromeos::features::kDriveFs);
  crostini_share_path()->SharePath(
      "vm-running", drivefs_.Append(".Trash").Append("in-the-trash"),
      PERSIST_NO,
      base::BindOnce(&CrostiniSharePathTest::SharePathCallback,
                     base::Unretained(this), Persist::NO,
                     SeneschalClientCalled::NO, nullptr, "", Success::NO,
                     "Path is not allowed"));
  run_loop()->Run();
}

TEST_F(CrostiniSharePathTest, SuccessRemovable) {
  crostini_share_path()->SharePath(
      "vm-running", base::FilePath("/media/removable/MyUSB"), PERSIST_NO,
      base::BindOnce(&CrostiniSharePathTest::SharePathCallback,
                     base::Unretained(this), Persist::NO,
                     SeneschalClientCalled::YES,
                     &vm_tools::seneschal::SharePathRequest::REMOVABLE, "MyUSB",
                     Success::YES, ""));
  run_loop()->Run();
}

TEST_F(CrostiniSharePathTest, FailRemovableRoot) {
  crostini_share_path()->SharePath(
      "vm-running", base::FilePath("/media/removable"), PERSIST_NO,
      base::BindOnce(&CrostiniSharePathTest::SharePathCallback,
                     base::Unretained(this), Persist::NO,
                     SeneschalClientCalled::NO, nullptr, "", Success::NO,
                     "Path is not allowed"));
  run_loop()->Run();
}

TEST_F(CrostiniSharePathTest, SharePathErrorSeneschal) {
  vm_tools::concierge::StartVmResponse start_vm_response;
  start_vm_response.set_status(vm_tools::concierge::VM_STATUS_RUNNING);
  start_vm_response.mutable_vm_info()->set_seneschal_server_handle(123);
  fake_concierge_client_->set_start_vm_response(start_vm_response);

  vm_tools::seneschal::SharePathResponse share_path_response;
  share_path_response.set_success(false);
  share_path_response.set_failure_reason("test failure");
  fake_seneschal_client_->set_share_path_response(share_path_response);

  crostini_share_path()->SharePath(
      "error-seneschal", share_path_, PERSIST_YES,
      base::BindOnce(&CrostiniSharePathTest::SharePathCallback,
                     base::Unretained(this), Persist::YES,
                     SeneschalClientCalled::YES,
                     &vm_tools::seneschal::SharePathRequest::DOWNLOADS,
                     "path-to-share", Success::NO, "test failure"));
  run_loop()->Run();
}

TEST_F(CrostiniSharePathTest, SharePathErrorPathNotAbsolute) {
  const base::FilePath path("not/absolute/dir");
  crostini_share_path()->SharePath(
      "vm-running", path, PERSIST_YES,
      base::BindOnce(&CrostiniSharePathTest::SharePathCallback,
                     base::Unretained(this), Persist::NO,
                     SeneschalClientCalled::NO, nullptr, "", Success::NO,
                     "Path must be absolute"));
  run_loop()->Run();
}

TEST_F(CrostiniSharePathTest, SharePathErrorReferencesParent) {
  const base::FilePath path("/path/../references/parent");
  crostini_share_path()->SharePath(
      "vm-running", path, PERSIST_NO,
      base::BindOnce(&CrostiniSharePathTest::SharePathCallback,
                     base::Unretained(this), Persist::NO,
                     SeneschalClientCalled::NO, nullptr, "", Success::NO,
                     "Path must be absolute"));
  run_loop()->Run();
}

TEST_F(CrostiniSharePathTest, SharePathErrorNotUnderDownloads) {
  const base::FilePath path("/not/under/downloads");
  crostini_share_path()->SharePath(
      "vm-running", path, PERSIST_YES,
      base::BindOnce(&CrostiniSharePathTest::SharePathCallback,
                     base::Unretained(this), Persist::NO,
                     SeneschalClientCalled::NO, nullptr, "", Success::NO,
                     "Path is not allowed"));
  run_loop()->Run();
}

TEST_F(CrostiniSharePathTest, SharePathVmToBeRestarted) {
  crostini_share_path()->SharePath(
      "vm-to-be-started", share_path_, PERSIST_YES,
      base::BindOnce(&CrostiniSharePathTest::SharePathCallback,
                     base::Unretained(this), Persist::YES,
                     SeneschalClientCalled::YES,
                     &vm_tools::seneschal::SharePathRequest::DOWNLOADS,
                     "path-to-share", Success::YES, ""));
  run_loop()->Run();
}

TEST_F(CrostiniSharePathTest, SharePathErrorVmCouldNotBeStarted) {
  vm_tools::concierge::StartVmResponse start_vm_response;
  start_vm_response.set_status(vm_tools::concierge::VM_STATUS_FAILURE);
  fake_concierge_client_->set_start_vm_response(start_vm_response);

  crostini_share_path()->SharePath(
      "error-vm-could-not-be-started", share_path_, PERSIST_YES,
      base::BindOnce(&CrostiniSharePathTest::SharePathCallback,
                     base::Unretained(this), Persist::YES,
                     SeneschalClientCalled::NO, nullptr, "", Success::NO,
                     "VM could not be started"));
  run_loop()->Run();
}

TEST_F(CrostiniSharePathTest, SharePersistedPaths) {
  base::FilePath share_path2_ = downloads_.AppendASCII("path-to-share-2");
  ASSERT_TRUE(base::CreateDirectory(share_path2_));
  vm_tools::concierge::VmInfo vm_info;
  CrostiniManager::GetForProfile(profile())->AddRunningVmForTesting(
      kCrostiniDefaultVmName, vm_info);
  base::ListValue shared_paths = base::ListValue();
  shared_paths.GetList().push_back(base::Value(share_path_.value()));
  shared_paths.GetList().push_back(base::Value(share_path2_.value()));
  profile()->GetPrefs()->Set(prefs::kCrostiniSharedPaths, shared_paths);
  crostini_share_path()->SharePersistedPaths(
      base::BindOnce(&CrostiniSharePathTest::SharePersistedPathsCallback,
                     base::Unretained(this)));
  run_loop()->Run();
}

TEST_F(CrostiniSharePathTest, RegisterPersistedPaths) {
  base::ListValue shared_paths = base::ListValue();
  profile()->GetPrefs()->Set(prefs::kCrostiniSharedPaths, shared_paths);

  crostini_share_path()->RegisterPersistedPath(base::FilePath("/a/a/a"));
  const base::ListValue* prefs =
      profile()->GetPrefs()->GetList(prefs::kCrostiniSharedPaths);
  EXPECT_EQ(prefs->GetSize(), 1U);
  std::string path;
  prefs->GetString(0, &path);
  EXPECT_EQ(path, "/a/a/a");

  // Adding the same path again should not cause any changes.
  crostini_share_path()->RegisterPersistedPath(base::FilePath("/a/a/a"));
  EXPECT_EQ(prefs->GetSize(), 1U);
  prefs->GetString(0, &path);
  EXPECT_EQ(path, "/a/a/a");

  // Add more paths.
  crostini_share_path()->RegisterPersistedPath(base::FilePath("/a/a/b"));
  crostini_share_path()->RegisterPersistedPath(base::FilePath("/a/a/c"));
  crostini_share_path()->RegisterPersistedPath(base::FilePath("/a/b/a"));
  crostini_share_path()->RegisterPersistedPath(base::FilePath("/b/a/a"));
  prefs = profile()->GetPrefs()->GetList(prefs::kCrostiniSharedPaths);
  EXPECT_EQ(prefs->GetSize(), 5U);
  prefs->GetString(0, &path);
  EXPECT_EQ(path, "/a/a/a");
  prefs->GetString(1, &path);
  EXPECT_EQ(path, "/a/a/b");
  prefs->GetString(2, &path);
  EXPECT_EQ(path, "/a/a/c");
  prefs->GetString(3, &path);
  EXPECT_EQ(path, "/a/b/a");
  prefs->GetString(4, &path);
  EXPECT_EQ(path, "/b/a/a");

  // Adding /a/a should remove /a/a/b, /a/a/c.
  crostini_share_path()->RegisterPersistedPath(base::FilePath("/a/a"));
  prefs = profile()->GetPrefs()->GetList(prefs::kCrostiniSharedPaths);
  EXPECT_EQ(prefs->GetSize(), 3U);
  prefs->GetString(0, &path);
  EXPECT_EQ(path, "/a/b/a");
  prefs->GetString(1, &path);
  EXPECT_EQ(path, "/b/a/a");
  prefs->GetString(2, &path);
  EXPECT_EQ(path, "/a/a");

  // Adding /a should remove /a/a, /a/b/a.
  crostini_share_path()->RegisterPersistedPath(base::FilePath("/a"));
  prefs = profile()->GetPrefs()->GetList(prefs::kCrostiniSharedPaths);
  EXPECT_EQ(prefs->GetSize(), 2U);
  prefs->GetString(0, &path);
  EXPECT_EQ(path, "/b/a/a");
  prefs->GetString(1, &path);
  EXPECT_EQ(path, "/a");

  // Adding / should remove all others.
  crostini_share_path()->RegisterPersistedPath(base::FilePath("/"));
  prefs = profile()->GetPrefs()->GetList(prefs::kCrostiniSharedPaths);
  EXPECT_EQ(prefs->GetSize(), 1U);
  prefs->GetString(0, &path);
  EXPECT_EQ(path, "/");
}

TEST_F(CrostiniSharePathTest, UnsharePathSuccess) {
  crostini_share_path()->UnsharePath(
      "vm-running", shared_path_,
      base::BindOnce(&CrostiniSharePathTest::UnsharePathCallback,
                     base::Unretained(this), shared_path_,
                     SeneschalClientCalled::YES,
                     "MyFiles/Downloads/already-shared", Success::YES, ""));
  run_loop()->Run();
}

TEST_F(CrostiniSharePathTest, UnsharePathRoot) {
  crostini_share_path()->UnsharePath(
      "vm-running", downloads_,
      base::BindOnce(&CrostiniSharePathTest::UnsharePathCallback,
                     base::Unretained(this), downloads_,
                     SeneschalClientCalled::YES, "MyFiles/Downloads",
                     Success::YES, ""));
  run_loop()->Run();
}

TEST_F(CrostiniSharePathTest, UnsharePathVmNotRunning) {
  crostini_share_path()->UnsharePath(
      "vm-not-running", shared_path_,
      base::BindOnce(&CrostiniSharePathTest::UnsharePathCallback,
                     base::Unretained(this), shared_path_,
                     SeneschalClientCalled::NO, "", Success::YES,
                     "VM not running"));
  run_loop()->Run();
}

TEST_F(CrostiniSharePathTest, UnsharePathInvalidPath) {
  base::FilePath invalid("invalid/path");
  crostini_share_path()->UnsharePath(
      "vm-running", invalid,
      base::BindOnce(&CrostiniSharePathTest::UnsharePathCallback,
                     base::Unretained(this), invalid, SeneschalClientCalled::NO,
                     "", Success::NO, "Invalid path to unshare"));
  run_loop()->Run();
}

}  // namespace crostini
