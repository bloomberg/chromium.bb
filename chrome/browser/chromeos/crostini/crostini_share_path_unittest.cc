// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/crostini/crostini_share_path.h"
#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/run_loop.h"
#include "base/sys_info.h"
#include "base/test/scoped_task_environment.h"
#include "chrome/browser/chromeos/crostini/crostini_manager.h"
#include "chrome/browser/chromeos/crostini/crostini_pref_names.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/chromeos/file_manager/path_util.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_cicerone_client.h"
#include "chromeos/dbus/fake_concierge_client.h"
#include "chromeos/dbus/fake_seneschal_client.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace crostini {

const char kLsbRelease[] =
    "CHROMEOS_RELEASE_NAME=Chrome OS\n"
    "CHROMEOS_RELEASE_VERSION=1.2.3.4\n";

class CrostiniSharePathTest : public testing::Test {
 public:
  void SharePathSuccessStartTerminaVmCallback(ConciergeClientResult result) {
    EXPECT_TRUE(fake_concierge_client_->start_termina_vm_called());
    EXPECT_EQ(result, ConciergeClientResult::SUCCESS);

    SharePath(profile(), "success", share_path_,
              base::BindOnce(&CrostiniSharePathTest::SharePathCallback,
                             base::Unretained(this), true, true, true, "",
                             run_loop()->QuitClosure()));
  }

  void SharePathErrorSeneschalStartTerminaVmCallback(
      ConciergeClientResult result) {
    EXPECT_TRUE(fake_concierge_client_->start_termina_vm_called());
    EXPECT_EQ(result, ConciergeClientResult::SUCCESS);

    SharePath(profile(), "error-seneschal", share_path_,
              base::BindOnce(&CrostiniSharePathTest::SharePathCallback,
                             base::Unretained(this), true, true, false,
                             "test failure", run_loop()->QuitClosure()));
  }

  void SharePathCallback(bool expected_path_saved_to_prefs,
                         bool expected_seneschal_client_called,
                         bool expected_success,
                         std::string expected_failure_reason,
                         base::OnceClosure closure,
                         bool success,
                         std::string failure_reason) {
    const base::ListValue* prefs =
        profile()->GetPrefs()->GetList(prefs::kCrostiniSharedPaths);
    if (expected_path_saved_to_prefs) {
      EXPECT_EQ(prefs->GetSize(), 1U);
      std::string share_path;
      prefs->GetString(0, &share_path);
      EXPECT_EQ(share_path_.value(), share_path);
    } else {
      EXPECT_EQ(prefs->GetSize(), 0U);
    }
    EXPECT_EQ(fake_seneschal_client_->share_path_called(),
              expected_seneschal_client_called);
    EXPECT_EQ(success, expected_success);
    EXPECT_EQ(failure_reason, expected_failure_reason);
    std::move(closure).Run();
  }

  void SharePathErrorVmNotRunningCallback(base::OnceClosure closure,
                                          bool success,
                                          std::string failure_reason) {
    EXPECT_FALSE(fake_seneschal_client_->share_path_called());
    EXPECT_EQ(success, false);
    EXPECT_EQ(failure_reason, "Cannot share, VM not running");
    std::move(closure).Run();
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

    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        chromeos::switches::kCrostiniFiles);

    // Fake that this is a real ChromeOS system in order to use TestingProfile
    // /tmp path for Downloads rather than the current Linux user $HOME.
    // SetChromeOSVersionInfoForTest() must not be called until D-Bus is
    // initialized with fake clients, and then it must be cleared in TearDown()
    // before D-Bus is re-initialized for the next test.
    base::SysInfo::SetChromeOSVersionInfoForTest(kLsbRelease, base::Time());

    // Setup Downloads and path to share.
    downloads_ = file_manager::util::GetDownloadsFolderForProfile(profile());
    share_path_ = downloads_.AppendASCII("path");
    ASSERT_TRUE(base::CreateDirectory(share_path_));

    // Create 'vm-running' VM instance which is running.
    vm_tools::concierge::VmInfo vm_info;
    CrostiniManager::GetForProfile(profile())->AddRunningVmForTesting(
        "vm-running", vm_info);
  }

  void TearDown() override {
    run_loop_.reset();
    profile_.reset();
    // Clear SetChromeOSVersionInfoForTest() so D-Bus will use fake clients
    // again in the next test.
    base::SysInfo::SetChromeOSVersionInfoForTest("", base::Time());
  }

 protected:
  base::RunLoop* run_loop() { return run_loop_.get(); }
  Profile* profile() { return profile_.get(); }
  base::FilePath downloads_;
  base::FilePath share_path_;

  // Owned by chromeos::DBusThreadManager
  chromeos::FakeSeneschalClient* fake_seneschal_client_;
  chromeos::FakeConciergeClient* fake_concierge_client_;

  std::unique_ptr<base::RunLoop>
      run_loop_;  // run_loop_ must be created on the UI thread.
  std::unique_ptr<TestingProfile> profile_;
 private:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  content::TestBrowserThreadBundle test_browser_thread_bundle_;
  DISALLOW_COPY_AND_ASSIGN(CrostiniSharePathTest);
};

TEST_F(CrostiniSharePathTest, Success) {
  vm_tools::concierge::StartVmResponse start_vm_response;
  start_vm_response.set_status(vm_tools::concierge::VM_STATUS_RUNNING);
  start_vm_response.mutable_vm_info()->set_seneschal_server_handle(123);
  fake_concierge_client_->set_start_vm_response(start_vm_response);

  CrostiniManager::GetForProfile(profile())->StartTerminaVm(
      "success", share_path_,
      base::BindOnce(
          &CrostiniSharePathTest::SharePathSuccessStartTerminaVmCallback,
          base::Unretained(this)));
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

  CrostiniManager::GetForProfile(profile())->StartTerminaVm(
      "error-seneschal", share_path_,
      base::BindOnce(
          &CrostiniSharePathTest::SharePathErrorSeneschalStartTerminaVmCallback,
          base::Unretained(this)));
  run_loop()->Run();
}

TEST_F(CrostiniSharePathTest, SharePathErrorPathNotAbsolute) {
  const base::FilePath path("not/absolute/dir");
  SharePath(profile(), "vm-running", path,
            base::BindOnce(&CrostiniSharePathTest::SharePathCallback,
                           base::Unretained(this), false, false, false,
                           "Path must be absolute", run_loop()->QuitClosure()));
  run_loop()->Run();
}

TEST_F(CrostiniSharePathTest, SharePathErrorNotUnderDownloads) {
  const base::FilePath path("/not/under/downloads");
  SharePath(profile(), "vm-running", path,
            base::BindOnce(&CrostiniSharePathTest::SharePathCallback,
                           base::Unretained(this), false, false, false,
                           "Path must be under Downloads",
                           run_loop()->QuitClosure()));
  run_loop()->Run();
}

TEST_F(CrostiniSharePathTest, SharePathErrorVmNotRunning) {
  SharePath(profile(), "error-vm-not-running", share_path_,
            base::BindOnce(&CrostiniSharePathTest::SharePathCallback,
                           base::Unretained(this), true, false, false,
                           "VM not running", run_loop()->QuitClosure()));
  run_loop()->Run();
}

TEST_F(CrostiniSharePathTest, SharePathErrorNotValidDirectory) {
  const base::FilePath path = share_path_.AppendASCII("not-exists");
  SharePath(profile(), "vm-running", path,
            base::BindOnce(&CrostiniSharePathTest::SharePathCallback,
                           base::Unretained(this), false, false, false,
                           "Path is not a valid directory",
                           run_loop()->QuitClosure()));
  run_loop()->Run();
}

TEST_F(CrostiniSharePathTest, ShareAllPaths) {
  base::FilePath share_path2_ = downloads_.AppendASCII("path");
  ASSERT_TRUE(base::CreateDirectory(share_path2_));
  vm_tools::concierge::VmInfo vm_info;
  CrostiniManager::GetForProfile(profile())->AddRunningVmForTesting(
      kCrostiniDefaultVmName, vm_info);
  base::ListValue shared_paths = base::ListValue();
  shared_paths.GetList().push_back(base::Value(share_path_.value()));
  shared_paths.GetList().push_back(base::Value(share_path2_.value()));
  profile()->GetPrefs()->Set(prefs::kCrostiniSharedPaths, shared_paths);
  ShareAllPaths(profile(), run_loop()->QuitClosure());
  run_loop()->Run();
}

}  // namespace crostini
