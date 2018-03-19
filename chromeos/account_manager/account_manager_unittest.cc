// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/account_manager/account_manager.h"

#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

class AccountManagerTest : public testing::Test {
 public:
  AccountManagerTest() = default;
  ~AccountManagerTest() override {}

 protected:
  void SetUp() override {
    ASSERT_TRUE(tmp_dir_.CreateUniqueTempDir());
    account_manager_ = std::make_unique<AccountManager>();
    account_manager_->Initialize(tmp_dir_.GetPath(),
                                 base::SequencedTaskRunnerHandle::Get());
  }

  // Check base/test/scoped_task_environment.h. This must be the first member /
  // declared before any member that cares about tasks.
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  base::ScopedTempDir tmp_dir_;
  std::unique_ptr<AccountManager> account_manager_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AccountManagerTest);
};

TEST_F(AccountManagerTest, TestInitialization) {
  AccountManager account_manager;

  EXPECT_EQ(account_manager.init_state_,
            AccountManager::InitializationState::kNotStarted);
  account_manager.Initialize(tmp_dir_.GetPath(),
                             base::SequencedTaskRunnerHandle::Get());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(account_manager.init_state_,
            AccountManager::InitializationState::kInitialized);
}

TEST_F(AccountManagerTest, TestUpsert) {
  account_manager_->UpsertToken("abc", "123");

  std::vector<std::string> accounts;
  base::RunLoop run_loop;
  account_manager_->GetAccounts(base::BindOnce(
      [](std::vector<std::string>* accounts, base::OnceClosure quit_closure,
         std::vector<std::string> stored_accounts) -> void {
        *accounts = stored_accounts;
        std::move(quit_closure).Run();
      },
      base::Unretained(&accounts), run_loop.QuitClosure()));
  run_loop.Run();

  EXPECT_EQ(1UL, accounts.size());
  EXPECT_EQ("abc", accounts[0]);
}

TEST_F(AccountManagerTest, TestPersistence) {
  account_manager_->UpsertToken("abc", "123");
  base::RunLoop().RunUntilIdle();

  account_manager_ = std::make_unique<AccountManager>();
  account_manager_->Initialize(tmp_dir_.GetPath(),
                               base::SequencedTaskRunnerHandle::Get());

  std::vector<std::string> accounts;
  base::RunLoop run_loop;
  account_manager_->GetAccounts(base::BindOnce(
      [](std::vector<std::string>* accounts, base::OnceClosure quit_closure,
         std::vector<std::string> stored_accounts) -> void {
        *accounts = stored_accounts;
        std::move(quit_closure).Run();
      },
      base::Unretained(&accounts), run_loop.QuitClosure()));
  run_loop.Run();

  EXPECT_EQ(1UL, accounts.size());
  EXPECT_EQ("abc", accounts[0]);
}

}  // namespace chromeos
