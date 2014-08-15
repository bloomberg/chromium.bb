// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/backup_rollback_controller.h"

#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "chrome/browser/sync/supervised_user_signin_manager_wrapper.h"
#include "chrome/common/chrome_switches.h"
#include "components/sync_driver/sync_prefs.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Return;

namespace {

#if defined(ENABLE_PRE_SYNC_BACKUP)

class MockSigninManagerWrapper : public SupervisedUserSigninManagerWrapper {
 public:
  MockSigninManagerWrapper() : SupervisedUserSigninManagerWrapper(NULL, NULL) {}

  MOCK_CONST_METHOD0(GetEffectiveUsername, std::string());
};

class FakeSyncPrefs : public sync_driver::SyncPrefs {
 public:
  FakeSyncPrefs() : rollback_tries_left_(0) {}

  virtual int GetRemainingRollbackTries() const OVERRIDE {
    return rollback_tries_left_;
  }

  virtual void SetRemainingRollbackTries(int v) OVERRIDE {
    rollback_tries_left_ = v;
  }

 private:
  int rollback_tries_left_;
};

class BackupRollbackControllerTest : public testing::Test {
 public:
  void ControllerCallback(bool start_backup) {
    if (start_backup)
      backup_started_ = true;
    else
      rollback_started_ = true;
  }

 protected:
  virtual void SetUp() OVERRIDE {
    backup_started_ = false;
    rollback_started_ = false;

    EXPECT_CALL(signin_wrapper_, GetEffectiveUsername())
        .WillRepeatedly(Return(""));

    controller_.reset(new browser_sync::BackupRollbackController(
        &fake_prefs_, &signin_wrapper_,
        base::Bind(&BackupRollbackControllerTest::ControllerCallback,
                   base::Unretained(this), true),
        base::Bind(&BackupRollbackControllerTest::ControllerCallback,
                   base::Unretained(this), false)));
  }

  void PumpLoop() {
    base::RunLoop run_loop;
    loop_.PostTask(FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }

  MockSigninManagerWrapper signin_wrapper_;
  FakeSyncPrefs fake_prefs_;
  scoped_ptr<browser_sync::BackupRollbackController> controller_;
  bool backup_started_;
  bool rollback_started_;
  base::MessageLoop loop_;
};

TEST_F(BackupRollbackControllerTest, StartBackup) {
  EXPECT_TRUE(controller_->StartBackup());
  PumpLoop();
  EXPECT_TRUE(backup_started_);
}

TEST_F(BackupRollbackControllerTest, NoBackupIfDisabled) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kSyncDisableBackup);

  base::RunLoop run_loop;
  EXPECT_FALSE(controller_->StartBackup());
  loop_.PostTask(FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
  EXPECT_FALSE(backup_started_);
}

TEST_F(BackupRollbackControllerTest, StartRollback) {
  fake_prefs_.SetRemainingRollbackTries(1);

  EXPECT_TRUE(controller_->StartRollback());
  PumpLoop();
  EXPECT_TRUE(rollback_started_);
  EXPECT_EQ(0, fake_prefs_.GetRemainingRollbackTries());
}

TEST_F(BackupRollbackControllerTest, NoRollbackIfOutOfTries) {
  fake_prefs_.SetRemainingRollbackTries(0);

  EXPECT_FALSE(controller_->StartRollback());
  PumpLoop();
  EXPECT_FALSE(rollback_started_);
}

TEST_F(BackupRollbackControllerTest, NoRollbackIfUserSignedIn) {
  fake_prefs_.SetRemainingRollbackTries(1);
  EXPECT_CALL(signin_wrapper_, GetEffectiveUsername())
      .Times(1)
      .WillOnce(Return("test"));
  EXPECT_FALSE(controller_->StartRollback());
  EXPECT_EQ(0, fake_prefs_.GetRemainingRollbackTries());

  PumpLoop();
  EXPECT_FALSE(backup_started_);
  EXPECT_FALSE(rollback_started_);
}

TEST_F(BackupRollbackControllerTest, NoRollbackIfDisabled) {
  fake_prefs_.SetRemainingRollbackTries(1);

  CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kSyncDisableRollback);
  EXPECT_FALSE(controller_->StartRollback());
  EXPECT_EQ(0, fake_prefs_.GetRemainingRollbackTries());

  PumpLoop();
  EXPECT_FALSE(rollback_started_);
}

#endif

}  // anonymous namespace

