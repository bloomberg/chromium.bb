// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/time.h"
#include "chrome/browser/sync/engine/mock_model_safe_workers.h"
#include "chrome/browser/sync/engine/syncer_thread.h"
#include "chrome/browser/sync/sessions/sync_session_context.h"
#include "chrome/browser/sync/sessions/test_util.h"
#include "chrome/test/sync/engine/mock_connection_manager.h"
#include "chrome/test/sync/engine/test_directory_setter_upper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gmock/include/gmock/gmock.h"

using base::TimeDelta;
using base::TimeTicks;

namespace browser_sync {
using sessions::SyncSessionContext;
using browser_sync::Syncer;

class SyncerThread2WhiteboxTest : public testing::Test {
 public:
  virtual void SetUp() {
    syncdb_.SetUp();
    Syncer* syncer = new Syncer();
    registrar_.reset(MockModelSafeWorkerRegistrar::PassiveBookmarks());
    context_ = new SyncSessionContext(connection_.get(), syncdb_.manager(),
        registrar_.get(), std::vector<SyncEngineEventListener*>());
    context_->set_notifications_enabled(true);
    context_->set_account_name("Test");
    syncer_thread_.reset(new SyncerThread(context_, syncer));
  }

  virtual void TearDown() {
    syncdb_.TearDown();
  }

  void SetMode(SyncerThread::Mode mode) {
    syncer_thread_->mode_ = mode;
  }

  void SetLastSyncedTime(base::TimeTicks ticks) {
    syncer_thread_->last_sync_session_end_time_ = ticks;
  }

  void SetServerConnection(bool connected) {
    syncer_thread_->server_connection_ok_ = connected;
  }

  void ResetWaitInterval() {
    syncer_thread_->wait_interval_.reset();
  }

  void SetWaitIntervalToThrottled() {
    syncer_thread_->wait_interval_.reset(new SyncerThread::WaitInterval(
        SyncerThread::WaitInterval::THROTTLED, TimeDelta::FromSeconds(1)));
  }

  void SetWaitIntervalToExponentialBackoff() {
    syncer_thread_->wait_interval_.reset(
       new SyncerThread::WaitInterval(
       SyncerThread::WaitInterval::EXPONENTIAL_BACKOFF,
       TimeDelta::FromSeconds(1)));
  }

  SyncerThread::JobProcessDecision DecideOnJob(
      const SyncerThread::SyncSessionJob& job) {
    return syncer_thread_->DecideOnJob(job);
  }

  void InitializeSyncerOnNormalMode() {
    SetMode(SyncerThread::NORMAL_MODE);
    ResetWaitInterval();
    SetServerConnection(true);
    SetLastSyncedTime(base::TimeTicks::Now());
  }

  SyncerThread::JobProcessDecision CreateAndDecideJob(
      SyncerThread::SyncSessionJob::SyncSessionJobPurpose purpose) {
    struct SyncerThread::SyncSessionJob job;
    job.purpose = purpose;
    job.scheduled_start = TimeTicks::Now();
    return DecideOnJob(job);
  }

 protected:
  scoped_ptr<SyncerThread> syncer_thread_;

 private:
  scoped_ptr<MockConnectionManager> connection_;
  SyncSessionContext* context_;
  //MockDelayProvider* delay_;
  scoped_ptr<MockModelSafeWorkerRegistrar> registrar_;
  MockDirectorySetterUpper syncdb_;
};

TEST_F(SyncerThread2WhiteboxTest, SaveNudge) {
  InitializeSyncerOnNormalMode();

  // Now set the mode to configure.
  SetMode(SyncerThread::CONFIGURATION_MODE);

  SyncerThread::JobProcessDecision decision =
      CreateAndDecideJob(SyncerThread::SyncSessionJob::NUDGE);

  EXPECT_EQ(decision, SyncerThread::SAVE);
}

TEST_F(SyncerThread2WhiteboxTest, ContinueNudge) {
  InitializeSyncerOnNormalMode();

  SyncerThread::JobProcessDecision decision = CreateAndDecideJob(
      SyncerThread::SyncSessionJob::NUDGE);

  EXPECT_EQ(decision, SyncerThread::CONTINUE);
}

TEST_F(SyncerThread2WhiteboxTest, DropPoll) {
  InitializeSyncerOnNormalMode();
  SetMode(SyncerThread::CONFIGURATION_MODE);

  SyncerThread::JobProcessDecision decision = CreateAndDecideJob(
      SyncerThread::SyncSessionJob::POLL);

  EXPECT_EQ(decision, SyncerThread::DROP);
}

TEST_F(SyncerThread2WhiteboxTest, ContinuePoll) {
  InitializeSyncerOnNormalMode();

  SyncerThread::JobProcessDecision decision = CreateAndDecideJob(
      SyncerThread::SyncSessionJob::POLL);

  EXPECT_EQ(decision, SyncerThread::CONTINUE);
}

TEST_F(SyncerThread2WhiteboxTest, ContinueConfiguration) {
  InitializeSyncerOnNormalMode();
  SetMode(SyncerThread::CONFIGURATION_MODE);

  SyncerThread::JobProcessDecision decision = CreateAndDecideJob(
      SyncerThread::SyncSessionJob::CONFIGURATION);

  EXPECT_EQ(decision, SyncerThread::CONTINUE);
}

TEST_F(SyncerThread2WhiteboxTest, SaveConfigurationWhileThrottled) {
  InitializeSyncerOnNormalMode();
  SetMode(SyncerThread::CONFIGURATION_MODE);

  SetWaitIntervalToThrottled();

  SyncerThread::JobProcessDecision decision = CreateAndDecideJob(
      SyncerThread::SyncSessionJob::CONFIGURATION);

  EXPECT_EQ(decision, SyncerThread::SAVE);
}

TEST_F(SyncerThread2WhiteboxTest, SaveNudgeWhileThrottled) {
  InitializeSyncerOnNormalMode();
  SetMode(SyncerThread::CONFIGURATION_MODE);

  SetWaitIntervalToThrottled();

  SyncerThread::JobProcessDecision decision = CreateAndDecideJob(
      SyncerThread::SyncSessionJob::NUDGE);

  EXPECT_EQ(decision, SyncerThread::SAVE);

}

TEST_F(SyncerThread2WhiteboxTest, ContinueClearUserDataUnderAllCircumstances) {
  InitializeSyncerOnNormalMode();

  SetMode(SyncerThread::CONFIGURATION_MODE);
  SetWaitIntervalToThrottled();
  SyncerThread::JobProcessDecision decision = CreateAndDecideJob(
      SyncerThread::SyncSessionJob::CLEAR_USER_DATA);
  EXPECT_EQ(decision, SyncerThread::CONTINUE);

  SetMode(SyncerThread::NORMAL_MODE);
  SetWaitIntervalToExponentialBackoff();
  decision = CreateAndDecideJob(
      SyncerThread::SyncSessionJob::CLEAR_USER_DATA);
  EXPECT_EQ(decision, SyncerThread::CONTINUE);
}

TEST_F(SyncerThread2WhiteboxTest, ContinueNudgeWhileExponentialBackOff) {
  InitializeSyncerOnNormalMode();
  SetMode(SyncerThread::NORMAL_MODE);
  SetWaitIntervalToExponentialBackoff();

  SyncerThread::JobProcessDecision decision = CreateAndDecideJob(
      SyncerThread::SyncSessionJob::NUDGE);

  EXPECT_EQ(decision, SyncerThread::CONTINUE);
}

TEST_F(SyncerThread2WhiteboxTest, DropNudgeWhileExponentialBackOff) {
  InitializeSyncerOnNormalMode();
  SetMode(SyncerThread::NORMAL_MODE);
  SetWaitIntervalToExponentialBackoff();

  syncer_thread_->wait_interval_->had_nudge = true;

  SyncerThread::JobProcessDecision decision = CreateAndDecideJob(
      SyncerThread::SyncSessionJob::NUDGE);

  EXPECT_EQ(decision, SyncerThread::DROP);
}

TEST_F(SyncerThread2WhiteboxTest, ContinueCanaryJobConfig) {
  InitializeSyncerOnNormalMode();
  SetMode(SyncerThread::CONFIGURATION_MODE);
  SetWaitIntervalToExponentialBackoff();

  struct SyncerThread::SyncSessionJob job;
  job.purpose = SyncerThread::SyncSessionJob::CONFIGURATION;
  job.scheduled_start = TimeTicks::Now();
  job.is_canary_job = true;
  SyncerThread::JobProcessDecision decision = DecideOnJob(job);

  EXPECT_EQ(decision, SyncerThread::CONTINUE);
}

}  // namespace browser_sync

// SyncerThread won't outlive the test!
DISABLE_RUNNABLE_METHOD_REFCOUNT(
    browser_sync::SyncerThread2WhiteboxTest);
