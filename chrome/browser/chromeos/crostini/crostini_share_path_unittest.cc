// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/crostini/crostini_share_path.h"
#include "base/bind.h"
#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "chrome/browser/chromeos/crostini/crostini_manager.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_cicerone_client.h"
#include "chromeos/dbus/fake_concierge_client.h"
#include "chromeos/dbus/fake_seneschal_client.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace crostini {

class CrostiniSharePathTest : public testing::Test {
 public:
  void SharePathSuccessStartTerminaVmCallback(ConciergeClientResult result) {
    EXPECT_TRUE(fake_concierge_client_->start_termina_vm_called());
    EXPECT_EQ(result, ConciergeClientResult::SUCCESS);

    CrostiniSharePath::GetInstance()->SharePath(
        profile(), "vm-running-success", "path",
        base::BindOnce(&CrostiniSharePathTest::SharePathSuccessCallback,
                       base::Unretained(this), run_loop()->QuitClosure()));
  }

  void SharePathSuccessCallback(base::OnceClosure closure,
                                bool success,
                                std::string failure_reason) {
    EXPECT_TRUE(fake_seneschal_client_->share_path_called());
    EXPECT_EQ(success, true);
    EXPECT_EQ(failure_reason, "");
    std::move(closure).Run();
  }

  void SharePathErrorStartTerminaVmCallback(ConciergeClientResult result) {
    EXPECT_TRUE(fake_concierge_client_->start_termina_vm_called());
    EXPECT_EQ(result, ConciergeClientResult::SUCCESS);

    CrostiniSharePath::GetInstance()->SharePath(
        profile(), "vm-running-error", "path",
        base::BindOnce(&CrostiniSharePathTest::SharePathErrorCallback,
                       base::Unretained(this), run_loop()->QuitClosure()));
  }

  void SharePathErrorCallback(base::OnceClosure closure,
                              bool success,
                              std::string failure_reason) {
    EXPECT_TRUE(fake_seneschal_client_->share_path_called());
    EXPECT_EQ(success, false);
    EXPECT_EQ(failure_reason, "test failure");
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
      : fake_seneschal_client_(new chromeos::FakeSeneschalClient()),
        fake_concierge_client_(new chromeos::FakeConciergeClient()),
        fake_cicerone_client_(new chromeos::FakeCiceroneClient()),
        scoped_task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::UI),
        test_browser_thread_bundle_(
            content::TestBrowserThreadBundle::REAL_IO_THREAD) {
    // Initialization between D-Bus, fake clients, and the singleton
    // CrostiniManager is tricky since CrostiniManager is a global singleton and
    // doesn't add itself as an observer for the new Concierge/Cicerone clients
    // created for each test.  We must first get a handle on the D-Bus setter
    // and initialize it, then reset CrostiniManager and add it as an observer
    // for the clients in this test, then set the fake clients into D-Bus.
    auto dbus_setter = chromeos::DBusThreadManager::GetSetterForTesting();
    chromeos::DBusThreadManager::Initialize();
    CrostiniManager::GetInstance()->ResetForTesting();
    fake_concierge_client_->AddObserver(CrostiniManager::GetInstance());
    fake_cicerone_client_->AddObserver(CrostiniManager::GetInstance());
    dbus_setter->SetConciergeClient(base::WrapUnique(fake_concierge_client_));
    dbus_setter->SetCiceroneClient(base::WrapUnique(fake_cicerone_client_));
    dbus_setter->SetSeneschalClient(base::WrapUnique(fake_seneschal_client_));
  }

  ~CrostiniSharePathTest() override { chromeos::DBusThreadManager::Shutdown(); }

  void SetUp() override {
    run_loop_ = std::make_unique<base::RunLoop>();
    profile_ = std::make_unique<TestingProfile>();
  }

  void TearDown() override {
    run_loop_.reset();
    profile_.reset();
  }

 protected:
  base::RunLoop* run_loop() { return run_loop_.get(); }
  Profile* profile() { return profile_.get(); }

  // Owned by chromeos::DBusThreadManager
  chromeos::FakeSeneschalClient* fake_seneschal_client_;
  chromeos::FakeConciergeClient* fake_concierge_client_;
  chromeos::FakeCiceroneClient* fake_cicerone_client_;

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
  start_vm_response.set_success(true);
  start_vm_response.mutable_vm_info()->set_seneschal_server_handle(123);
  fake_concierge_client_->set_start_vm_response(start_vm_response);

  CrostiniManager::GetInstance()->StartTerminaVm(
      "test", "vm-running-success", base::FilePath("path"),
      base::BindOnce(
          &CrostiniSharePathTest::SharePathSuccessStartTerminaVmCallback,
          base::Unretained(this)));
  run_loop()->Run();
}

TEST_F(CrostiniSharePathTest, SharePathError) {
  vm_tools::concierge::StartVmResponse start_vm_response;
  start_vm_response.set_success(true);
  start_vm_response.mutable_vm_info()->set_seneschal_server_handle(123);
  fake_concierge_client_->set_start_vm_response(start_vm_response);

  vm_tools::seneschal::SharePathResponse share_path_response;
  share_path_response.set_success(false);
  share_path_response.set_failure_reason("test failure");
  fake_seneschal_client_->set_share_path_response(share_path_response);

  CrostiniManager::GetInstance()->StartTerminaVm(
      "test", "vm-running-error", base::FilePath("path"),
      base::BindOnce(
          &CrostiniSharePathTest::SharePathErrorStartTerminaVmCallback,
          base::Unretained(this)));
  run_loop()->Run();
}

TEST_F(CrostiniSharePathTest, SharePathErrorVmNotRunning) {
  CrostiniSharePath::GetInstance()->SharePath(
      profile(), "vm-not-running", "path",
      base::BindOnce(&CrostiniSharePathTest::SharePathErrorVmNotRunningCallback,
                     base::Unretained(this), run_loop()->QuitClosure()));
  run_loop()->Run();
}

}  // namespace crostini
