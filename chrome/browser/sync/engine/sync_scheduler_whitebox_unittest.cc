// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "base/time.h"
#include "chrome/browser/sync/engine/sync_scheduler.h"
#include "chrome/browser/sync/sessions/sync_session_context.h"
#include "chrome/browser/sync/sessions/test_util.h"
#include "chrome/browser/sync/test/engine/fake_model_safe_worker_registrar.h"
#include "chrome/browser/sync/test/engine/mock_connection_manager.h"
#include "chrome/browser/sync/test/engine/test_directory_setter_upper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::TimeDelta;
using base::TimeTicks;

namespace browser_sync {
using sessions::SyncSessionContext;
using browser_sync::Syncer;

class SyncSchedulerWhiteboxTest : public testing::Test {
 public:
  virtual void SetUp() {
    syncdb_.SetUp();
    Syncer* syncer = new Syncer();
    ModelSafeRoutingInfo routes;
    routes[syncable::BOOKMARKS] = GROUP_UI;
    routes[syncable::NIGORI] = GROUP_PASSIVE;
    registrar_.reset(new FakeModelSafeWorkerRegistrar(routes));
    connection_.reset(new MockConnectionManager(syncdb_.manager(), "Test"));
    connection_->SetServerReachable();
    context_ = new SyncSessionContext(connection_.get(), syncdb_.manager(),
        registrar_.get(), std::vector<SyncEngineEventListener*>(), NULL);
    context_->set_notifications_enabled(true);
    context_->set_account_name("Test");
    scheduler_.reset(
        new SyncScheduler("TestSyncSchedulerWhitebox", context_, syncer));
  }

  virtual void TearDown() {
    scheduler_.reset();
    syncdb_.TearDown();
  }

  void SetMode(SyncScheduler::Mode mode) {
    scheduler_->mode_ = mode;
  }

  void SetLastSyncedTime(base::TimeTicks ticks) {
    scheduler_->last_sync_session_end_time_ = ticks;
  }

  void SetServerConnection(bool connected) {
    scheduler_->server_connection_ok_ = connected;
  }

  void ResetWaitInterval() {
    scheduler_->wait_interval_.reset();
  }

  void SetWaitIntervalToThrottled() {
    scheduler_->wait_interval_.reset(new SyncScheduler::WaitInterval(
        SyncScheduler::WaitInterval::THROTTLED, TimeDelta::FromSeconds(1)));
  }

  void SetWaitIntervalToExponentialBackoff() {
    scheduler_->wait_interval_.reset(
       new SyncScheduler::WaitInterval(
       SyncScheduler::WaitInterval::EXPONENTIAL_BACKOFF,
       TimeDelta::FromSeconds(1)));
  }

  void SetWaitIntervalHadNudge(bool had_nudge) {
    scheduler_->wait_interval_->had_nudge = had_nudge;
  }

  SyncScheduler::JobProcessDecision DecideOnJob(
      const SyncScheduler::SyncSessionJob& job) {
    return scheduler_->DecideOnJob(job);
  }

  void InitializeSyncerOnNormalMode() {
    SetMode(SyncScheduler::NORMAL_MODE);
    ResetWaitInterval();
    SetServerConnection(true);
    SetLastSyncedTime(base::TimeTicks::Now());
  }

  SyncScheduler::JobProcessDecision CreateAndDecideJob(
      SyncScheduler::SyncSessionJob::SyncSessionJobPurpose purpose) {
    struct SyncScheduler::SyncSessionJob job;
    job.purpose = purpose;
    job.scheduled_start = TimeTicks::Now();
    return DecideOnJob(job);
  }

 private:
  MessageLoop message_loop_;
  scoped_ptr<SyncScheduler> scheduler_;
  scoped_ptr<MockConnectionManager> connection_;
  SyncSessionContext* context_;
  scoped_ptr<FakeModelSafeWorkerRegistrar> registrar_;
  MockDirectorySetterUpper syncdb_;
};

TEST_F(SyncSchedulerWhiteboxTest, SaveNudge) {
  InitializeSyncerOnNormalMode();

  // Now set the mode to configure.
  SetMode(SyncScheduler::CONFIGURATION_MODE);

  SyncScheduler::JobProcessDecision decision =
      CreateAndDecideJob(SyncScheduler::SyncSessionJob::NUDGE);

  EXPECT_EQ(decision, SyncScheduler::SAVE);
}

TEST_F(SyncSchedulerWhiteboxTest, ContinueNudge) {
  InitializeSyncerOnNormalMode();

  SyncScheduler::JobProcessDecision decision = CreateAndDecideJob(
      SyncScheduler::SyncSessionJob::NUDGE);

  EXPECT_EQ(decision, SyncScheduler::CONTINUE);
}

TEST_F(SyncSchedulerWhiteboxTest, DropPoll) {
  InitializeSyncerOnNormalMode();
  SetMode(SyncScheduler::CONFIGURATION_MODE);

  SyncScheduler::JobProcessDecision decision = CreateAndDecideJob(
      SyncScheduler::SyncSessionJob::POLL);

  EXPECT_EQ(decision, SyncScheduler::DROP);
}

TEST_F(SyncSchedulerWhiteboxTest, ContinuePoll) {
  InitializeSyncerOnNormalMode();

  SyncScheduler::JobProcessDecision decision = CreateAndDecideJob(
      SyncScheduler::SyncSessionJob::POLL);

  EXPECT_EQ(decision, SyncScheduler::CONTINUE);
}

TEST_F(SyncSchedulerWhiteboxTest, ContinueConfiguration) {
  InitializeSyncerOnNormalMode();
  SetMode(SyncScheduler::CONFIGURATION_MODE);

  SyncScheduler::JobProcessDecision decision = CreateAndDecideJob(
      SyncScheduler::SyncSessionJob::CONFIGURATION);

  EXPECT_EQ(decision, SyncScheduler::CONTINUE);
}

TEST_F(SyncSchedulerWhiteboxTest, SaveConfigurationWhileThrottled) {
  InitializeSyncerOnNormalMode();
  SetMode(SyncScheduler::CONFIGURATION_MODE);

  SetWaitIntervalToThrottled();

  SyncScheduler::JobProcessDecision decision = CreateAndDecideJob(
      SyncScheduler::SyncSessionJob::CONFIGURATION);

  EXPECT_EQ(decision, SyncScheduler::SAVE);
}

TEST_F(SyncSchedulerWhiteboxTest, SaveNudgeWhileThrottled) {
  InitializeSyncerOnNormalMode();
  SetMode(SyncScheduler::CONFIGURATION_MODE);

  SetWaitIntervalToThrottled();

  SyncScheduler::JobProcessDecision decision = CreateAndDecideJob(
      SyncScheduler::SyncSessionJob::NUDGE);

  EXPECT_EQ(decision, SyncScheduler::SAVE);
}

TEST_F(SyncSchedulerWhiteboxTest,
       ContinueClearUserDataUnderAllCircumstances) {
  InitializeSyncerOnNormalMode();

  SetMode(SyncScheduler::CONFIGURATION_MODE);
  SetWaitIntervalToThrottled();
  SyncScheduler::JobProcessDecision decision = CreateAndDecideJob(
      SyncScheduler::SyncSessionJob::CLEAR_USER_DATA);
  EXPECT_EQ(decision, SyncScheduler::CONTINUE);

  SetMode(SyncScheduler::NORMAL_MODE);
  SetWaitIntervalToExponentialBackoff();
  decision = CreateAndDecideJob(
      SyncScheduler::SyncSessionJob::CLEAR_USER_DATA);
  EXPECT_EQ(decision, SyncScheduler::CONTINUE);
}

TEST_F(SyncSchedulerWhiteboxTest, ContinueNudgeWhileExponentialBackOff) {
  InitializeSyncerOnNormalMode();
  SetMode(SyncScheduler::NORMAL_MODE);
  SetWaitIntervalToExponentialBackoff();

  SyncScheduler::JobProcessDecision decision = CreateAndDecideJob(
      SyncScheduler::SyncSessionJob::NUDGE);

  EXPECT_EQ(decision, SyncScheduler::CONTINUE);
}

TEST_F(SyncSchedulerWhiteboxTest, DropNudgeWhileExponentialBackOff) {
  InitializeSyncerOnNormalMode();
  SetMode(SyncScheduler::NORMAL_MODE);
  SetWaitIntervalToExponentialBackoff();
  SetWaitIntervalHadNudge(true);

  SyncScheduler::JobProcessDecision decision = CreateAndDecideJob(
      SyncScheduler::SyncSessionJob::NUDGE);

  EXPECT_EQ(decision, SyncScheduler::DROP);
}

TEST_F(SyncSchedulerWhiteboxTest, ContinueCanaryJobConfig) {
  InitializeSyncerOnNormalMode();
  SetMode(SyncScheduler::CONFIGURATION_MODE);
  SetWaitIntervalToExponentialBackoff();

  struct SyncScheduler::SyncSessionJob job;
  job.purpose = SyncScheduler::SyncSessionJob::CONFIGURATION;
  job.scheduled_start = TimeTicks::Now();
  job.is_canary_job = true;
  SyncScheduler::JobProcessDecision decision = DecideOnJob(job);

  EXPECT_EQ(decision, SyncScheduler::CONTINUE);
}

}  // namespace browser_sync
