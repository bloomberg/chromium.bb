// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/crostini/crostini_manager.h"
#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_concierge_client.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace crostini {

namespace {
const char kVmName[] = "vm_name";
const char kContainerName[] = "container_name";
const char kContainerUserName[] = "container_username";
}  // namespace

class CrostiniManagerTest : public testing::Test {
 public:
  void CreateDiskImageClientErrorCallback(base::OnceClosure closure,
                                          ConciergeClientResult result,
                                          const base::FilePath& file_path) {
    EXPECT_FALSE(fake_concierge_client_->create_disk_image_called());
    EXPECT_EQ(result, ConciergeClientResult::CLIENT_ERROR);
    std::move(closure).Run();
  }

  void DestroyDiskImageClientErrorCallback(base::OnceClosure closure,
                                           ConciergeClientResult result) {
    EXPECT_FALSE(fake_concierge_client_->destroy_disk_image_called());
    EXPECT_EQ(result, ConciergeClientResult::CLIENT_ERROR);
    std::move(closure).Run();
  }

  void StartTerminaVmClientErrorCallback(base::OnceClosure closure,
                                         ConciergeClientResult result) {
    EXPECT_FALSE(fake_concierge_client_->start_termina_vm_called());
    EXPECT_EQ(result, ConciergeClientResult::CLIENT_ERROR);
    std::move(closure).Run();
  }

  void StopVmClientErrorCallback(base::OnceClosure closure,
                                 ConciergeClientResult result) {
    EXPECT_FALSE(fake_concierge_client_->stop_vm_called());
    EXPECT_EQ(result, ConciergeClientResult::CLIENT_ERROR);
    std::move(closure).Run();
  }

  void StartContainerClientErrorCallback(base::OnceClosure closure,
                                         ConciergeClientResult result) {
    EXPECT_FALSE(fake_concierge_client_->start_container_called());
    EXPECT_EQ(result, ConciergeClientResult::CLIENT_ERROR);
    std::move(closure).Run();
  }

  void CreateDiskImageSuccessCallback(base::OnceClosure closure,
                                      ConciergeClientResult result,
                                      const base::FilePath& file_path) {
    EXPECT_TRUE(fake_concierge_client_->create_disk_image_called());
    std::move(closure).Run();
  }

  void DestroyDiskImageSuccessCallback(base::OnceClosure closure,
                                       ConciergeClientResult result) {
    EXPECT_TRUE(fake_concierge_client_->destroy_disk_image_called());
    std::move(closure).Run();
  }

  void StartTerminaVmSuccessCallback(base::OnceClosure closure,
                                     ConciergeClientResult result) {
    EXPECT_TRUE(fake_concierge_client_->start_termina_vm_called());
    std::move(closure).Run();
  }

  void StopVmSuccessCallback(base::OnceClosure closure,
                             ConciergeClientResult result) {
    EXPECT_TRUE(fake_concierge_client_->stop_vm_called());
    std::move(closure).Run();
  }

  void StartContainerSuccessCallback(base::OnceClosure closure,
                                     ConciergeClientResult result) {
    EXPECT_TRUE(fake_concierge_client_->start_container_called());
    std::move(closure).Run();
  }

  CrostiniManagerTest()
      : fake_concierge_client_(new chromeos::FakeConciergeClient()),
        scoped_task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::UI) {
    chromeos::DBusThreadManager::GetSetterForTesting()->SetConciergeClient(
        base::WrapUnique(fake_concierge_client_));
    chromeos::DBusThreadManager::Initialize();
  }

  ~CrostiniManagerTest() override { chromeos::DBusThreadManager::Shutdown(); }

 protected:
  chromeos::FakeConciergeClient* fake_concierge_client_;

 private:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  DISALLOW_COPY_AND_ASSIGN(CrostiniManagerTest);
};

TEST_F(CrostiniManagerTest, CreateDiskImageNameError) {
  const base::FilePath& disk_path = base::FilePath("");
  base::RunLoop loop;
  CrostiniManager::GetInstance()->CreateDiskImage(
      "i_dont_know_what_cryptohome_id_is", disk_path,
      vm_tools::concierge::STORAGE_CRYPTOHOME_ROOT,
      base::BindOnce(&CrostiniManagerTest::CreateDiskImageClientErrorCallback,
                     base::Unretained(this), loop.QuitClosure()));
  loop.Run();
}

TEST_F(CrostiniManagerTest, CreateDiskImageCryptohomeError) {
  const base::FilePath& disk_path = base::FilePath(kVmName);
  base::RunLoop loop;
  CrostiniManager::GetInstance()->CreateDiskImage(
      "", disk_path, vm_tools::concierge::STORAGE_CRYPTOHOME_ROOT,
      base::BindOnce(&CrostiniManagerTest::CreateDiskImageClientErrorCallback,
                     base::Unretained(this), loop.QuitClosure()));
  loop.Run();
}

TEST_F(CrostiniManagerTest, CreateDiskImageStorageLocationError) {
  const base::FilePath& disk_path = base::FilePath(kVmName);
  base::RunLoop loop;
  CrostiniManager::GetInstance()->CreateDiskImage(
      "i_dont_know_what_cryptohome_id_is", disk_path,
      vm_tools::concierge::StorageLocation_INT_MIN_SENTINEL_DO_NOT_USE_,
      base::BindOnce(&CrostiniManagerTest::CreateDiskImageClientErrorCallback,
                     base::Unretained(this), loop.QuitClosure()));
  loop.Run();
}

TEST_F(CrostiniManagerTest, CreateDiskImageSuccess) {
  const base::FilePath& disk_path = base::FilePath(kVmName);
  base::RunLoop loop;
  CrostiniManager::GetInstance()->CreateDiskImage(
      "i_dont_know_what_cryptohome_id_is", disk_path,
      vm_tools::concierge::STORAGE_CRYPTOHOME_DOWNLOADS,
      base::BindOnce(&CrostiniManagerTest::CreateDiskImageSuccessCallback,
                     base::Unretained(this), loop.QuitClosure()));
  loop.Run();
}

TEST_F(CrostiniManagerTest, DestroyDiskImageNameError) {
  const base::FilePath& disk_path = base::FilePath("");
  base::RunLoop loop;
  CrostiniManager::GetInstance()->DestroyDiskImage(
      "i_dont_know_what_cryptohome_id_is", disk_path,
      vm_tools::concierge::STORAGE_CRYPTOHOME_ROOT,
      base::BindOnce(&CrostiniManagerTest::DestroyDiskImageClientErrorCallback,
                     base::Unretained(this), loop.QuitClosure()));
  loop.Run();
}

TEST_F(CrostiniManagerTest, DestroyDiskImageCryptohomeError) {
  const base::FilePath& disk_path = base::FilePath(kVmName);
  base::RunLoop loop;
  CrostiniManager::GetInstance()->DestroyDiskImage(
      "", disk_path, vm_tools::concierge::STORAGE_CRYPTOHOME_ROOT,
      base::BindOnce(&CrostiniManagerTest::DestroyDiskImageClientErrorCallback,
                     base::Unretained(this), loop.QuitClosure()));
  loop.Run();
}

TEST_F(CrostiniManagerTest, DestroyDiskImageStorageLocationError) {
  const base::FilePath& disk_path = base::FilePath(kVmName);
  base::RunLoop loop;
  CrostiniManager::GetInstance()->DestroyDiskImage(
      "i_dont_know_what_cryptohome_id_is", disk_path,
      vm_tools::concierge::StorageLocation_INT_MIN_SENTINEL_DO_NOT_USE_,
      base::BindOnce(&CrostiniManagerTest::DestroyDiskImageClientErrorCallback,
                     base::Unretained(this), loop.QuitClosure()));
  loop.Run();
}

TEST_F(CrostiniManagerTest, DestroyDiskImageSuccess) {
  const base::FilePath& disk_path = base::FilePath(kVmName);
  base::RunLoop loop;
  CrostiniManager::GetInstance()->DestroyDiskImage(
      "i_dont_know_what_cryptohome_id_is", disk_path,
      vm_tools::concierge::STORAGE_CRYPTOHOME_DOWNLOADS,
      base::BindOnce(&CrostiniManagerTest::DestroyDiskImageSuccessCallback,
                     base::Unretained(this), loop.QuitClosure()));
  loop.Run();
}

TEST_F(CrostiniManagerTest, StartTerminaVmNameError) {
  const base::FilePath& disk_path = base::FilePath(kVmName);
  base::RunLoop loop;
  CrostiniManager::GetInstance()->StartTerminaVm(
      "", disk_path,
      base::BindOnce(&CrostiniManagerTest::StartTerminaVmClientErrorCallback,
                     base::Unretained(this), loop.QuitClosure()));
  loop.Run();
}

TEST_F(CrostiniManagerTest, StartTerminaVmDiskPathError) {
  const base::FilePath& disk_path = base::FilePath();
  base::RunLoop loop;
  CrostiniManager::GetInstance()->StartTerminaVm(
      kVmName, disk_path,
      base::BindOnce(&CrostiniManagerTest::StartTerminaVmClientErrorCallback,
                     base::Unretained(this), loop.QuitClosure()));
  loop.Run();
}

TEST_F(CrostiniManagerTest, StartTerminaVmSuccess) {
  const base::FilePath& disk_path = base::FilePath(kVmName);
  base::RunLoop loop;
  CrostiniManager::GetInstance()->StartTerminaVm(
      kVmName, disk_path,
      base::BindOnce(&CrostiniManagerTest::StartTerminaVmSuccessCallback,
                     base::Unretained(this), loop.QuitClosure()));
  loop.Run();
}

TEST_F(CrostiniManagerTest, StopVmNameError) {
  base::RunLoop loop;
  CrostiniManager::GetInstance()->StopVm(
      "", base::BindOnce(&CrostiniManagerTest::StopVmClientErrorCallback,
                         base::Unretained(this), loop.QuitClosure()));
  loop.Run();
}

TEST_F(CrostiniManagerTest, StopVmSuccess) {
  base::RunLoop loop;
  CrostiniManager::GetInstance()->StopVm(
      kVmName, base::BindOnce(&CrostiniManagerTest::StopVmSuccessCallback,
                              base::Unretained(this), loop.QuitClosure()));
  loop.Run();
}

TEST_F(CrostiniManagerTest, StartContainerVmNameError) {
  base::RunLoop loop;
  CrostiniManager::GetInstance()->StartContainer(
      "", kContainerName, kContainerUserName,
      base::BindOnce(&CrostiniManagerTest::StartContainerClientErrorCallback,
                     base::Unretained(this), loop.QuitClosure()));
  loop.Run();
}

TEST_F(CrostiniManagerTest, StartContainerContainerNameError) {
  base::RunLoop loop;
  CrostiniManager::GetInstance()->StartContainer(
      kVmName, "", kContainerUserName,
      base::BindOnce(&CrostiniManagerTest::StartContainerClientErrorCallback,
                     base::Unretained(this), loop.QuitClosure()));
  loop.Run();
}

TEST_F(CrostiniManagerTest, StartContainerContainerUserNameError) {
  base::RunLoop loop;
  CrostiniManager::GetInstance()->StartContainer(
      kVmName, kContainerName, "",
      base::BindOnce(&CrostiniManagerTest::StartContainerClientErrorCallback,
                     base::Unretained(this), loop.QuitClosure()));
  loop.Run();
}

TEST_F(CrostiniManagerTest, StartContainerSignalNotConnectedError) {
  base::RunLoop loop;
  fake_concierge_client_->set_container_started_signal_connected(false);
  CrostiniManager::GetInstance()->StartContainer(
      kVmName, kContainerName, kContainerUserName,
      base::BindOnce(&CrostiniManagerTest::StartContainerClientErrorCallback,
                     base::Unretained(this), loop.QuitClosure()));
  loop.Run();
}

TEST_F(CrostiniManagerTest, StartContainerSuccess) {
  base::RunLoop loop;
  CrostiniManager::GetInstance()->StartContainer(
      kVmName, kContainerName, kContainerUserName,
      base::BindOnce(&CrostiniManagerTest::StartContainerSuccessCallback,
                     base::Unretained(this), loop.QuitClosure()));
  loop.Run();
}

class CrostiniManagerRestartTest : public CrostiniManagerTest,
                                   public CrostiniManager::RestartObserver {
 public:
  CrostiniManagerRestartTest()
      : test_browser_thread_bundle_(
            content::TestBrowserThreadBundle::REAL_IO_THREAD) {}

  void SetUp() override {
    loop_ = std::make_unique<base::RunLoop>();
    profile_ = std::make_unique<TestingProfile>();
  }

  void RestartCrostiniCallback(base::OnceClosure closure,
                               ConciergeClientResult result) {
    restart_crostini_callback_called_ = true;
    std::move(closure).Run();
  }

  // CrostiniManager::RestartObserver
  void OnComponentLoaded(ConciergeClientResult result) override {
    if (abort_on_component_loaded_) {
      Abort();
    }
  }

  void OnConciergeStarted(ConciergeClientResult result) override {
    if (abort_on_concierge_started_) {
      Abort();
    }
  }

  void OnDiskImageCreated(ConciergeClientResult result) override {
    if (abort_on_disk_image_created_) {
      Abort();
    }
  }

  void OnVmStarted(ConciergeClientResult result) override {
    if (abort_on_vm_started_) {
      Abort();
    }
  }

 protected:
  void Abort() {
    CrostiniManager::GetInstance()->AbortRestartCrostini(restart_id_);
    loop_->Quit();
  }

  content::TestBrowserThreadBundle test_browser_thread_bundle_;

  CrostiniManager::RestartId restart_id_ =
      CrostiniManager::kUninitializedRestartId;
  bool abort_on_component_loaded_ = false;
  bool abort_on_concierge_started_ = false;
  bool abort_on_disk_image_created_ = false;
  bool abort_on_vm_started_ = false;
  bool restart_crostini_callback_called_ = false;
  std::unique_ptr<base::RunLoop>
      loop_;  // loop_ must be created on the UI thread.
  std::unique_ptr<TestingProfile> profile_;
};

TEST_F(CrostiniManagerRestartTest, RestartSuccess) {
  restart_id_ = CrostiniManager::GetInstance()->RestartCrostini(
      profile_.get(), kVmName, kContainerName,
      base::BindOnce(&CrostiniManagerRestartTest::RestartCrostiniCallback,
                     base::Unretained(this), loop_->QuitClosure()),
      this);
  loop_->Run();
  EXPECT_TRUE(fake_concierge_client_->create_disk_image_called());
  EXPECT_TRUE(fake_concierge_client_->start_termina_vm_called());
  EXPECT_TRUE(fake_concierge_client_->start_container_called());
  EXPECT_TRUE(restart_crostini_callback_called_);
}

TEST_F(CrostiniManagerRestartTest, AbortOnComponentLoaded) {
  abort_on_component_loaded_ = true;
  restart_id_ = CrostiniManager::GetInstance()->RestartCrostini(
      profile_.get(), kVmName, kContainerName,
      base::BindOnce(&CrostiniManagerRestartTest::RestartCrostiniCallback,
                     base::Unretained(this), loop_->QuitClosure()),
      this);
  loop_->Run();
  EXPECT_FALSE(fake_concierge_client_->create_disk_image_called());
  EXPECT_FALSE(fake_concierge_client_->start_termina_vm_called());
  EXPECT_FALSE(fake_concierge_client_->start_container_called());
  EXPECT_FALSE(restart_crostini_callback_called_);
}

TEST_F(CrostiniManagerRestartTest, AbortOnConciergeStarted) {
  abort_on_concierge_started_ = true;
  restart_id_ = CrostiniManager::GetInstance()->RestartCrostini(
      profile_.get(), kVmName, kContainerName,
      base::BindOnce(&CrostiniManagerRestartTest::RestartCrostiniCallback,
                     base::Unretained(this), loop_->QuitClosure()),
      this);
  loop_->Run();
  EXPECT_FALSE(fake_concierge_client_->create_disk_image_called());
  EXPECT_FALSE(fake_concierge_client_->start_termina_vm_called());
  EXPECT_FALSE(fake_concierge_client_->start_container_called());
  EXPECT_FALSE(restart_crostini_callback_called_);
}

TEST_F(CrostiniManagerRestartTest, AbortOnDiskImageCreated) {
  abort_on_disk_image_created_ = true;
  restart_id_ = CrostiniManager::GetInstance()->RestartCrostini(
      profile_.get(), kVmName, kContainerName,
      base::BindOnce(&CrostiniManagerRestartTest::RestartCrostiniCallback,
                     base::Unretained(this), loop_->QuitClosure()),
      this);
  loop_->Run();
  EXPECT_TRUE(fake_concierge_client_->create_disk_image_called());
  EXPECT_FALSE(fake_concierge_client_->start_termina_vm_called());
  EXPECT_FALSE(fake_concierge_client_->start_container_called());
  EXPECT_FALSE(restart_crostini_callback_called_);
}

TEST_F(CrostiniManagerRestartTest, AbortOnVmStarted) {
  abort_on_vm_started_ = true;
  restart_id_ = CrostiniManager::GetInstance()->RestartCrostini(
      profile_.get(), kVmName, kContainerName,
      base::BindOnce(&CrostiniManagerRestartTest::RestartCrostiniCallback,
                     base::Unretained(this), loop_->QuitClosure()),
      this);
  loop_->Run();
  EXPECT_TRUE(fake_concierge_client_->create_disk_image_called());
  EXPECT_TRUE(fake_concierge_client_->start_termina_vm_called());
  EXPECT_FALSE(fake_concierge_client_->start_container_called());
  EXPECT_FALSE(restart_crostini_callback_called_);
}

}  // namespace crostini
