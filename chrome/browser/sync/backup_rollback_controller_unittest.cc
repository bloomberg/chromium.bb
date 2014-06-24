// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/backup_rollback_controller.h"

#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
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

    if (need_loop_quit_)
      base::MessageLoop::current()->Quit();
  }

 protected:
  virtual void SetUp() OVERRIDE {
    backup_started_ = false;
    rollback_started_ = false;
    need_loop_quit_ = false;

    EXPECT_CALL(signin_wrapper_, GetEffectiveUsername())
        .WillRepeatedly(Return(""));

    controller_.reset(new browser_sync::BackupRollbackController(
        &fake_prefs_, &signin_wrapper_,
        base::Bind(&BackupRollbackControllerTest::ControllerCallback,
                   base::Unretained(this), true),
        base::Bind(&BackupRollbackControllerTest::ControllerCallback,
                   base::Unretained(this), false)));
  }

  MockSigninManagerWrapper signin_wrapper_;
  FakeSyncPrefs fake_prefs_;
  scoped_ptr<browser_sync::BackupRollbackController> controller_;
  bool backup_started_;
  bool rollback_started_;
  bool need_loop_quit_;
  base::MessageLoop loop_;
};

TEST_F(BackupRollbackControllerTest, DelayStart) {
  controller_->Start(base::TimeDelta::FromMilliseconds(100));
  EXPECT_FALSE(backup_started_);
  need_loop_quit_ = true;
  base::MessageLoop::current()->Run();
  EXPECT_TRUE(backup_started_);
}

TEST_F(BackupRollbackControllerTest, NoDelayStart) {
  controller_->Start(base::TimeDelta());
  EXPECT_TRUE(backup_started_);
}

TEST_F(BackupRollbackControllerTest, NoStartWithUserSignedIn) {
  EXPECT_CALL(signin_wrapper_, GetEffectiveUsername())
      .Times(1)
      .WillOnce(Return("test"));
  controller_->Start(base::TimeDelta());
  EXPECT_FALSE(backup_started_);
  EXPECT_FALSE(rollback_started_);
}

TEST_F(BackupRollbackControllerTest, StartOnUserSignedOut) {
  EXPECT_CALL(signin_wrapper_, GetEffectiveUsername())
      .Times(2)
      .WillOnce(Return("test"))
      .WillOnce(Return(""));
  controller_->Start(base::TimeDelta());
  EXPECT_FALSE(backup_started_);
  EXPECT_FALSE(rollback_started_);

  // 2nd time no signed-in user. Starts backup.
  controller_->Start(base::TimeDelta());
  EXPECT_TRUE(backup_started_);
}

TEST_F(BackupRollbackControllerTest, StartRollback) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kSyncEnableRollback);

  EXPECT_CALL(signin_wrapper_, GetEffectiveUsername())
      .Times(2)
      .WillOnce(Return("test"))
      .WillOnce(Return(""));
  controller_->Start(base::TimeDelta());
  EXPECT_FALSE(backup_started_);
  EXPECT_FALSE(rollback_started_);

  controller_->OnRollbackReceived();
  controller_->Start(base::TimeDelta());
  EXPECT_TRUE(rollback_started_);
}

TEST_F(BackupRollbackControllerTest, RollbackOnBrowserStart) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kSyncEnableRollback);

  fake_prefs_.SetRemainingRollbackTries(1);
  controller_->Start(base::TimeDelta());
  EXPECT_TRUE(rollback_started_);
}

TEST_F(BackupRollbackControllerTest, BackupAfterRollbackDone) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kSyncEnableRollback);

  fake_prefs_.SetRemainingRollbackTries(3);
  controller_->Start(base::TimeDelta());
  EXPECT_TRUE(rollback_started_);
  EXPECT_FALSE(backup_started_);

  controller_->OnRollbackDone();
  controller_->Start(base::TimeDelta());
  EXPECT_TRUE(backup_started_);
}

TEST_F(BackupRollbackControllerTest, GiveUpRollback) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kSyncEnableRollback);

  fake_prefs_.SetRemainingRollbackTries(3);
  for (int i = 0; i < 3; ++i) {
    controller_->Start(base::TimeDelta());
    EXPECT_TRUE(rollback_started_);
    EXPECT_FALSE(backup_started_);
    rollback_started_ = false;
  }

  controller_->Start(base::TimeDelta());
  EXPECT_FALSE(rollback_started_);
  EXPECT_TRUE(backup_started_);
}

TEST_F(BackupRollbackControllerTest, SkipRollbackIfNotEnabled) {
  EXPECT_CALL(signin_wrapper_, GetEffectiveUsername())
      .Times(2)
      .WillOnce(Return("test"))
      .WillOnce(Return(""));
  controller_->Start(base::TimeDelta());
  EXPECT_FALSE(backup_started_);
  EXPECT_FALSE(rollback_started_);

  controller_->OnRollbackReceived();
  controller_->Start(base::TimeDelta());
  EXPECT_TRUE(backup_started_);
  EXPECT_FALSE(rollback_started_);
}

#endif

}  // anonymous namespace

