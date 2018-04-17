// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/crostini/crostini_manager.h"
#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_concierge_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace crostini {

namespace {
const char kVmName[] = "vm_name";
const char kContainerName[] = "container_name";
const char kContainerUserName[] = "container_username";
}  // namespace

class CrostiniManagerTest : public testing::Test {
 public:
  void CreateDiskImageClientErrorCallback(
      base::OnceClosure closure,
      crostini::ConciergeClientResult result,
      const base::FilePath& file_path) {
    EXPECT_FALSE(fake_concierge_client_->create_disk_image_called());
    EXPECT_EQ(result, crostini::ConciergeClientResult::CLIENT_ERROR);
    std::move(closure).Run();
  }

  void DestroyDiskImageClientErrorCallback(
      base::OnceClosure closure,
      crostini::ConciergeClientResult result) {
    EXPECT_FALSE(fake_concierge_client_->destroy_disk_image_called());
    EXPECT_EQ(result, crostini::ConciergeClientResult::CLIENT_ERROR);
    std::move(closure).Run();
  }

  void StartTerminaVmClientErrorCallback(
      base::OnceClosure closure,
      crostini::ConciergeClientResult result) {
    EXPECT_FALSE(fake_concierge_client_->start_termina_vm_called());
    EXPECT_EQ(result, crostini::ConciergeClientResult::CLIENT_ERROR);
    std::move(closure).Run();
  }

  void StopVmClientErrorCallback(base::OnceClosure closure,
                                 crostini::ConciergeClientResult result) {
    EXPECT_FALSE(fake_concierge_client_->stop_vm_called());
    EXPECT_EQ(result, crostini::ConciergeClientResult::CLIENT_ERROR);
    std::move(closure).Run();
  }

  void StartContainerClientErrorCallback(
      base::OnceClosure closure,
      crostini::ConciergeClientResult result) {
    EXPECT_FALSE(fake_concierge_client_->start_container_called());
    EXPECT_EQ(result, crostini::ConciergeClientResult::CLIENT_ERROR);
    std::move(closure).Run();
  }

  void CreateDiskImageSuccessCallback(base::OnceClosure closure,
                                      crostini::ConciergeClientResult result,
                                      const base::FilePath& file_path) {
    EXPECT_TRUE(fake_concierge_client_->create_disk_image_called());
    std::move(closure).Run();
  }

  void DestroyDiskImageSuccessCallback(base::OnceClosure closure,
                                       crostini::ConciergeClientResult result) {
    EXPECT_TRUE(fake_concierge_client_->destroy_disk_image_called());
    std::move(closure).Run();
  }

  void StartTerminaVmSuccessCallback(base::OnceClosure closure,
                                     crostini::ConciergeClientResult result) {
    EXPECT_TRUE(fake_concierge_client_->start_termina_vm_called());
    std::move(closure).Run();
  }

  void StopVmSuccessCallback(base::OnceClosure closure,
                             crostini::ConciergeClientResult result) {
    EXPECT_TRUE(fake_concierge_client_->stop_vm_called());
    std::move(closure).Run();
  }

  void StartContainerSuccessCallback(base::OnceClosure closure,
                                     crostini::ConciergeClientResult result) {
    EXPECT_TRUE(fake_concierge_client_->start_container_called());
    std::move(closure).Run();
  }

  CrostiniManagerTest()
      : fake_concierge_client_(new chromeos::FakeConciergeClient()) {
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
  crostini::CrostiniManager::GetInstance()->CreateDiskImage(
      "i_dont_know_what_cryptohome_id_is", disk_path,
      vm_tools::concierge::STORAGE_CRYPTOHOME_ROOT,
      base::BindOnce(&CrostiniManagerTest::CreateDiskImageClientErrorCallback,
                     base::Unretained(this), loop.QuitClosure()));
  loop.Run();
}

TEST_F(CrostiniManagerTest, CreateDiskImageCryptohomeError) {
  const base::FilePath& disk_path = base::FilePath(kVmName);
  base::RunLoop loop;
  crostini::CrostiniManager::GetInstance()->CreateDiskImage(
      "", disk_path, vm_tools::concierge::STORAGE_CRYPTOHOME_ROOT,
      base::BindOnce(&CrostiniManagerTest::CreateDiskImageClientErrorCallback,
                     base::Unretained(this), loop.QuitClosure()));
  loop.Run();
}

TEST_F(CrostiniManagerTest, CreateDiskImageStorageLocationError) {
  const base::FilePath& disk_path = base::FilePath(kVmName);
  base::RunLoop loop;
  crostini::CrostiniManager::GetInstance()->CreateDiskImage(
      "i_dont_know_what_cryptohome_id_is", disk_path,
      vm_tools::concierge::StorageLocation_INT_MIN_SENTINEL_DO_NOT_USE_,
      base::BindOnce(&CrostiniManagerTest::CreateDiskImageClientErrorCallback,
                     base::Unretained(this), loop.QuitClosure()));
  loop.Run();
}

TEST_F(CrostiniManagerTest, CreateDiskImageSuccess) {
  const base::FilePath& disk_path = base::FilePath(kVmName);
  base::RunLoop loop;
  crostini::CrostiniManager::GetInstance()->CreateDiskImage(
      "i_dont_know_what_cryptohome_id_is", disk_path,
      vm_tools::concierge::STORAGE_CRYPTOHOME_DOWNLOADS,
      base::BindOnce(&CrostiniManagerTest::CreateDiskImageSuccessCallback,
                     base::Unretained(this), loop.QuitClosure()));
  loop.Run();
}

TEST_F(CrostiniManagerTest, DestroyDiskImageNameError) {
  const base::FilePath& disk_path = base::FilePath("");
  base::RunLoop loop;
  crostini::CrostiniManager::GetInstance()->DestroyDiskImage(
      "i_dont_know_what_cryptohome_id_is", disk_path,
      vm_tools::concierge::STORAGE_CRYPTOHOME_ROOT,
      base::BindOnce(&CrostiniManagerTest::DestroyDiskImageClientErrorCallback,
                     base::Unretained(this), loop.QuitClosure()));
  loop.Run();
}

TEST_F(CrostiniManagerTest, DestroyDiskImageCryptohomeError) {
  const base::FilePath& disk_path = base::FilePath(kVmName);
  base::RunLoop loop;
  crostini::CrostiniManager::GetInstance()->DestroyDiskImage(
      "", disk_path, vm_tools::concierge::STORAGE_CRYPTOHOME_ROOT,
      base::BindOnce(&CrostiniManagerTest::DestroyDiskImageClientErrorCallback,
                     base::Unretained(this), loop.QuitClosure()));
  loop.Run();
}

TEST_F(CrostiniManagerTest, DestroyDiskImageStorageLocationError) {
  const base::FilePath& disk_path = base::FilePath(kVmName);
  base::RunLoop loop;
  crostini::CrostiniManager::GetInstance()->DestroyDiskImage(
      "i_dont_know_what_cryptohome_id_is", disk_path,
      vm_tools::concierge::StorageLocation_INT_MIN_SENTINEL_DO_NOT_USE_,
      base::BindOnce(&CrostiniManagerTest::DestroyDiskImageClientErrorCallback,
                     base::Unretained(this), loop.QuitClosure()));
  loop.Run();
}

TEST_F(CrostiniManagerTest, DestroyDiskImageSuccess) {
  const base::FilePath& disk_path = base::FilePath(kVmName);
  base::RunLoop loop;
  crostini::CrostiniManager::GetInstance()->DestroyDiskImage(
      "i_dont_know_what_cryptohome_id_is", disk_path,
      vm_tools::concierge::STORAGE_CRYPTOHOME_DOWNLOADS,
      base::BindOnce(&CrostiniManagerTest::DestroyDiskImageSuccessCallback,
                     base::Unretained(this), loop.QuitClosure()));
  loop.Run();
}

TEST_F(CrostiniManagerTest, StartTerminaVmNameError) {
  const base::FilePath& disk_path = base::FilePath(kVmName);
  base::RunLoop loop;
  crostini::CrostiniManager::GetInstance()->StartTerminaVm(
      "", disk_path,
      base::BindOnce(&CrostiniManagerTest::StartTerminaVmClientErrorCallback,
                     base::Unretained(this), loop.QuitClosure()));
  loop.Run();
}

TEST_F(CrostiniManagerTest, StartTerminaVmDiskPathError) {
  const base::FilePath& disk_path = base::FilePath();
  base::RunLoop loop;
  crostini::CrostiniManager::GetInstance()->StartTerminaVm(
      kVmName, disk_path,
      base::BindOnce(&CrostiniManagerTest::StartTerminaVmClientErrorCallback,
                     base::Unretained(this), loop.QuitClosure()));
  loop.Run();
}

TEST_F(CrostiniManagerTest, StartTerminaVmSuccess) {
  const base::FilePath& disk_path = base::FilePath(kVmName);
  base::RunLoop loop;
  crostini::CrostiniManager::GetInstance()->StartTerminaVm(
      kVmName, disk_path,
      base::BindOnce(&CrostiniManagerTest::StartTerminaVmSuccessCallback,
                     base::Unretained(this), loop.QuitClosure()));
  loop.Run();
}

TEST_F(CrostiniManagerTest, StopVmNameError) {
  base::RunLoop loop;
  crostini::CrostiniManager::GetInstance()->StopVm(
      "", base::BindOnce(&CrostiniManagerTest::StopVmClientErrorCallback,
                         base::Unretained(this), loop.QuitClosure()));
  loop.Run();
}

TEST_F(CrostiniManagerTest, StopVmSuccess) {
  base::RunLoop loop;
  crostini::CrostiniManager::GetInstance()->StopVm(
      kVmName, base::BindOnce(&CrostiniManagerTest::StopVmSuccessCallback,
                              base::Unretained(this), loop.QuitClosure()));
  loop.Run();
}

TEST_F(CrostiniManagerTest, StartContainerVmNameError) {
  base::RunLoop loop;
  crostini::CrostiniManager::GetInstance()->StartContainer(
      "", kContainerName, kContainerUserName,
      base::BindOnce(&CrostiniManagerTest::StartContainerClientErrorCallback,
                     base::Unretained(this), loop.QuitClosure()));
  loop.Run();
}

TEST_F(CrostiniManagerTest, StartContainerContainerNameError) {
  base::RunLoop loop;
  crostini::CrostiniManager::GetInstance()->StartContainer(
      kVmName, "", kContainerUserName,
      base::BindOnce(&CrostiniManagerTest::StartContainerClientErrorCallback,
                     base::Unretained(this), loop.QuitClosure()));
  loop.Run();
}

TEST_F(CrostiniManagerTest, StartContainerContainerUserNameError) {
  base::RunLoop loop;
  crostini::CrostiniManager::GetInstance()->StartContainer(
      kVmName, kContainerName, "",
      base::BindOnce(&CrostiniManagerTest::StartContainerClientErrorCallback,
                     base::Unretained(this), loop.QuitClosure()));
  loop.Run();
}

TEST_F(CrostiniManagerTest, StartContainerSignalNotConnectedError) {
  base::RunLoop loop;
  fake_concierge_client_->set_container_started_signal_connected(false);
  crostini::CrostiniManager::GetInstance()->StartContainer(
      kVmName, kContainerName, kContainerUserName,
      base::BindOnce(&CrostiniManagerTest::StartContainerClientErrorCallback,
                     base::Unretained(this), loop.QuitClosure()));
  loop.Run();
}

TEST_F(CrostiniManagerTest, StartContainerSuccess) {
  base::RunLoop loop;
  crostini::CrostiniManager::GetInstance()->StartContainer(
      kVmName, kContainerName, kContainerUserName,
      base::BindOnce(&CrostiniManagerTest::StartContainerSuccessCallback,
                     base::Unretained(this), loop.QuitClosure()));
  loop.Run();
}

}  // namespace crostini
