// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <list>
#include <map>
#include <set>
#include <strstream>

#include "base/command_line.h"
#include "base/scoped_ptr.h"
#include "base/time.h"
#include "chrome/browser/sync/engine/model_safe_worker.h"
#include "chrome/browser/sync/engine/syncer_thread.h"
#include "chrome/browser/sync/engine/syncer_thread_timed_stop.h"
#include "chrome/test/sync/engine/mock_server_connection.h"
#include "chrome/test/sync/engine/test_directory_setter_upper.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Time;
using base::TimeDelta;

namespace browser_sync {

typedef testing::Test SyncerThreadTest;

class SyncerThreadWithSyncerTest : public testing::Test {
 public:
  SyncerThreadWithSyncerTest() {}
  virtual void SetUp() {
    metadb_.SetUp();
    connection_.reset(new MockConnectionManager(metadb_.manager(),
                                                metadb_.name()));
    allstatus_.reset(new AllStatus());

    syncer_thread_ = SyncerThreadFactory::Create(NULL, metadb_.manager(),
        connection_.get(), allstatus_.get(), new ModelSafeWorker());

    allstatus_->WatchSyncerThread(syncer_thread_);
    syncer_thread_->SetConnected(true);
  }
  virtual void TearDown() {
    syncer_thread_ = NULL;
    allstatus_.reset();
    connection_.reset();
    metadb_.TearDown();
  }

  ManuallyOpenedTestDirectorySetterUpper* metadb() { return &metadb_; }
  MockConnectionManager* connection() { return connection_.get(); }
  SyncerThread* syncer_thread() { return syncer_thread_; }
 private:
  ManuallyOpenedTestDirectorySetterUpper metadb_;
  scoped_ptr<MockConnectionManager> connection_;
  scoped_ptr<AllStatus> allstatus_;
  scoped_refptr<SyncerThread> syncer_thread_;
  DISALLOW_COPY_AND_ASSIGN(SyncerThreadWithSyncerTest);
};

class SyncShareIntercept : public MockConnectionManager::MidCommitObserver {
 public:
  SyncShareIntercept() : sync_occured_(false, false) {}
  virtual ~SyncShareIntercept() {}
  virtual void Observe() {
    times_sync_occured_.push_back(Time::NowFromSystemTime());
    sync_occured_.Signal();
  }
  void WaitForSyncShare(int at_least_this_many, TimeDelta max_wait) {
    while (at_least_this_many-- > 0)
      sync_occured_.TimedWait(max_wait);
  }
  std::vector<Time> times_sync_occured() const {
    return times_sync_occured_;
  }
 private:
  std::vector<Time> times_sync_occured_;
  base::WaitableEvent sync_occured_;
  DISALLOW_COPY_AND_ASSIGN(SyncShareIntercept);
};

TEST_F(SyncerThreadTest, Construction) {
  scoped_refptr<SyncerThread> syncer_thread(
      SyncerThreadFactory::Create(NULL, NULL, NULL, NULL, NULL));
}

TEST_F(SyncerThreadTest, StartStop) {
  scoped_refptr<SyncerThread> syncer_thread(
      SyncerThreadFactory::Create(NULL, NULL, NULL, NULL, NULL));
  EXPECT_TRUE(syncer_thread->Start());
  EXPECT_TRUE(syncer_thread->Stop(2000));

  // Do it again for good measure.  I caught some bugs by adding this so
  // I would recommend keeping it.
  EXPECT_TRUE(syncer_thread->Start());
  EXPECT_TRUE(syncer_thread->Stop(2000));
}

TEST_F(SyncerThreadTest, CalculateSyncWaitTime) {
  scoped_refptr<SyncerThread> syncer_thread(
      SyncerThreadFactory::Create(NULL, NULL, NULL, NULL, NULL));
  syncer_thread->DisableIdleDetection();

  // Syncer_polling_interval_ is less than max poll interval.
  TimeDelta syncer_polling_interval = TimeDelta::FromSeconds(1);

  syncer_thread->SetSyncerPollingInterval(syncer_polling_interval);

  // user_idle_ms is less than 10 * (syncer_polling_interval*1000).
  ASSERT_EQ(syncer_polling_interval.InMilliseconds(),
            syncer_thread->CalculateSyncWaitTime(1000, 0));
  ASSERT_EQ(syncer_polling_interval.InMilliseconds(),
            syncer_thread->CalculateSyncWaitTime(1000, 1));

  // user_idle_ms is ge than 10 * (syncer_polling_interval*1000).
  int last_poll_time = 2000;
  ASSERT_LE(last_poll_time,
            syncer_thread->CalculateSyncWaitTime(last_poll_time, 10000));
  ASSERT_GE(last_poll_time*3,
            syncer_thread->CalculateSyncWaitTime(last_poll_time, 10000));
  ASSERT_LE(last_poll_time,
            syncer_thread->CalculateSyncWaitTime(last_poll_time, 100000));
  ASSERT_GE(last_poll_time*3,
            syncer_thread->CalculateSyncWaitTime(last_poll_time, 100000));

  // Maximum backoff time should be syncer_max_interval.
  int near_threshold = SyncerThread::kDefaultMaxPollIntervalMs / 2 - 1;
  int threshold = SyncerThread::kDefaultMaxPollIntervalMs;
  int over_threshold = SyncerThread::kDefaultMaxPollIntervalMs + 1;
  ASSERT_LE(near_threshold,
            syncer_thread->CalculateSyncWaitTime(near_threshold, 10000));
  ASSERT_GE(SyncerThread::kDefaultMaxPollIntervalMs,
            syncer_thread->CalculateSyncWaitTime(near_threshold, 10000));
  ASSERT_EQ(SyncerThread::kDefaultMaxPollIntervalMs,
            syncer_thread->CalculateSyncWaitTime(threshold, 10000));
  ASSERT_EQ(SyncerThread::kDefaultMaxPollIntervalMs,
            syncer_thread->CalculateSyncWaitTime(over_threshold, 10000));

  // Possible idle time must be capped by syncer_max_interval.
  int over_sync_max_interval =
      SyncerThread::kDefaultMaxPollIntervalMs + 1;
  syncer_polling_interval = TimeDelta::FromSeconds(
      over_sync_max_interval / 100);  // so 1000* is right
  syncer_thread->SetSyncerPollingInterval(syncer_polling_interval);
  ASSERT_EQ(syncer_polling_interval.InSeconds() * 1000,
            syncer_thread->CalculateSyncWaitTime(1000, over_sync_max_interval));
  syncer_polling_interval = TimeDelta::FromSeconds(1);
  syncer_thread->SetSyncerPollingInterval(syncer_polling_interval);
  ASSERT_LE(last_poll_time,
            syncer_thread->CalculateSyncWaitTime(last_poll_time,
                                                over_sync_max_interval));
  ASSERT_GE(last_poll_time * 3,
            syncer_thread->CalculateSyncWaitTime(last_poll_time,
                                                over_sync_max_interval));
}

TEST_F(SyncerThreadTest, CalculatePollingWaitTime) {
  // Set up the environment.
  int user_idle_milliseconds_param = 0;
  scoped_refptr<SyncerThread> syncer_thread(
      SyncerThreadFactory::Create(NULL, NULL, NULL, NULL, NULL));
  syncer_thread->DisableIdleDetection();

  // Notifications disabled should result in a polling interval of
  // kDefaultShortPollInterval.
  {
    AllStatus::Status status = {};
    status.notifications_enabled = 0;
    bool continue_sync_cycle_param = false;

    // No work and no backoff.
    ASSERT_EQ(SyncerThread::kDefaultShortPollIntervalSeconds,
              syncer_thread->CalculatePollingWaitTime(
                  status,
                  0,
                  &user_idle_milliseconds_param,
                  &continue_sync_cycle_param));
    ASSERT_FALSE(continue_sync_cycle_param);

    // In this case the continue_sync_cycle is turned off.
    continue_sync_cycle_param = true;
    ASSERT_EQ(SyncerThread::kDefaultShortPollIntervalSeconds,
              syncer_thread->CalculatePollingWaitTime(
                  status,
                  0,
                  &user_idle_milliseconds_param,
                  &continue_sync_cycle_param));
    ASSERT_FALSE(continue_sync_cycle_param);

    // TODO(brg) : Find a way to test exponential backoff is inoperable.
    // Exponential backoff should be turned on when notifications are disabled
    // but this can not be tested since we can not set the last input info.
  }

  // Notifications enabled should result in a polling interval of
  // SyncerThread::kDefaultLongPollIntervalSeconds.
  {
    AllStatus::Status status = {};
    status.notifications_enabled = 1;
    bool continue_sync_cycle_param = false;

    // No work and no backoff.
    ASSERT_EQ(SyncerThread::kDefaultLongPollIntervalSeconds,
              syncer_thread->CalculatePollingWaitTime(
                  status,
                  0,
                  &user_idle_milliseconds_param,
                  &continue_sync_cycle_param));
    ASSERT_FALSE(continue_sync_cycle_param);

    // In this case the continue_sync_cycle is turned off.
    continue_sync_cycle_param = true;
    ASSERT_EQ(SyncerThread::kDefaultLongPollIntervalSeconds,
              syncer_thread->CalculatePollingWaitTime(
                  status,
                  0,
                  &user_idle_milliseconds_param,
                  &continue_sync_cycle_param));
    ASSERT_FALSE(continue_sync_cycle_param);

    // TODO(brg) : Find a way to test exponential backoff.
    // Exponential backoff should be turned off when notifications are enabled,
    // but this can not be tested since we can not set the last input info.
  }

  // There are two states which can cause a continuation, either the updates
  // available do not match the updates received, or the unsynced count is
  // non-zero.
  {
    AllStatus::Status status = {};
    status.updates_available = 1;
    status.updates_received = 0;
    bool continue_sync_cycle_param = false;

    ASSERT_LE(0, syncer_thread->CalculatePollingWaitTime(
                     status,
                     0,
                     &user_idle_milliseconds_param,
                     &continue_sync_cycle_param));
    ASSERT_TRUE(continue_sync_cycle_param);

    continue_sync_cycle_param = false;
    ASSERT_GE(3, syncer_thread->CalculatePollingWaitTime(
                     status,
                     0,
                     &user_idle_milliseconds_param,
                     &continue_sync_cycle_param));
    ASSERT_TRUE(continue_sync_cycle_param);

    ASSERT_LE(0, syncer_thread->CalculatePollingWaitTime(
                     status,
                     0,
                     &user_idle_milliseconds_param,
                     &continue_sync_cycle_param));
    ASSERT_GE(2, syncer_thread->CalculatePollingWaitTime(
                     status,
                     0,
                     &user_idle_milliseconds_param,
                     &continue_sync_cycle_param));
    ASSERT_TRUE(continue_sync_cycle_param);

    status.updates_received = 1;
    ASSERT_EQ(SyncerThread::kDefaultShortPollIntervalSeconds,
              syncer_thread->CalculatePollingWaitTime(
                  status,
                  10,
                  &user_idle_milliseconds_param,
                  &continue_sync_cycle_param));
    ASSERT_FALSE(continue_sync_cycle_param);
  }

  {
    AllStatus::Status status = {};
    status.unsynced_count = 1;
    bool continue_sync_cycle_param = false;

    ASSERT_LE(0, syncer_thread->CalculatePollingWaitTime(
                     status,
                     0,
                     &user_idle_milliseconds_param,
                     &continue_sync_cycle_param));
    ASSERT_TRUE(continue_sync_cycle_param);

    continue_sync_cycle_param = false;
    ASSERT_GE(2, syncer_thread->CalculatePollingWaitTime(
                     status,
                     0,
                     &user_idle_milliseconds_param,
                     &continue_sync_cycle_param));
    ASSERT_TRUE(continue_sync_cycle_param);

    status.unsynced_count = 0;
    ASSERT_EQ(SyncerThread::kDefaultShortPollIntervalSeconds,
              syncer_thread->CalculatePollingWaitTime(
                  status,
                  4,
                  &user_idle_milliseconds_param,
                  &continue_sync_cycle_param));
    ASSERT_FALSE(continue_sync_cycle_param);
  }

  // Regression for exponential backoff reset when the syncer is nudged.
  {
    AllStatus::Status status = {};
    status.unsynced_count = 1;
    bool continue_sync_cycle_param = false;

    // Expect move from default polling interval to exponential backoff due to
    // unsynced_count != 0.
    ASSERT_LE(0, syncer_thread->CalculatePollingWaitTime(
                     status,
                     3600,
                     &user_idle_milliseconds_param,
                     &continue_sync_cycle_param));
    ASSERT_TRUE(continue_sync_cycle_param);

    continue_sync_cycle_param = false;
    ASSERT_GE(2, syncer_thread->CalculatePollingWaitTime(
                     status,
                     3600,
                     &user_idle_milliseconds_param,
                     &continue_sync_cycle_param));
    ASSERT_TRUE(continue_sync_cycle_param);

    // Expect exponential backoff.
    ASSERT_LE(2, syncer_thread->CalculatePollingWaitTime(
                     status,
                     2,
                     &user_idle_milliseconds_param,
                     &continue_sync_cycle_param));
    ASSERT_GE(6, syncer_thread->CalculatePollingWaitTime(
                     status,
                     2,
                     &user_idle_milliseconds_param,
                     &continue_sync_cycle_param));
    ASSERT_TRUE(continue_sync_cycle_param);

    // A nudge resets the continue_sync_cycle_param value, so our backoff
    // should return to the minimum.
    continue_sync_cycle_param = false;
    ASSERT_LE(0, syncer_thread->CalculatePollingWaitTime(
                     status,
                     3600,
                     &user_idle_milliseconds_param,
                     &continue_sync_cycle_param));
    ASSERT_TRUE(continue_sync_cycle_param);

    continue_sync_cycle_param = false;
    ASSERT_GE(2, syncer_thread->CalculatePollingWaitTime(
                     status,
                     3600,
                     &user_idle_milliseconds_param,
                     &continue_sync_cycle_param));
    ASSERT_TRUE(continue_sync_cycle_param);

    // Setting unsynced_count = 0 returns us to the default polling interval.
    status.unsynced_count = 0;
    ASSERT_EQ(SyncerThread::kDefaultShortPollIntervalSeconds,
              syncer_thread->CalculatePollingWaitTime(
                  status,
                  4,
                  &user_idle_milliseconds_param,
                  &continue_sync_cycle_param));
    ASSERT_FALSE(continue_sync_cycle_param);
  }
}

// This test is disabled. see bug 23336.
#if 0
TEST_F(SyncerThreadWithSyncerTest, Polling) {
  SyncShareIntercept interceptor;
  connection()->SetMidCommitObserver(&interceptor);

  const TimeDelta poll_interval = TimeDelta::FromSeconds(1);
  syncer_thread()->SetSyncerShortPollInterval(poll_interval);
  EXPECT_TRUE(syncer_thread()->Start());

  // Calling Open() should cause the SyncerThread to create a Syncer.
  metadb()->Open();

  TimeDelta two_polls = poll_interval + poll_interval;
  // We could theoretically return immediately from the wait if the interceptor
  // was already signaled for a SyncShare (the first one comes quick).
  interceptor.WaitForSyncShare(1, two_polls);
  EXPECT_FALSE(interceptor.times_sync_occured().empty());

  // Wait for at least 2 more SyncShare operations.
  interceptor.WaitForSyncShare(2, two_polls);
  EXPECT_TRUE(syncer_thread()->Stop(2000));

  // Now analyze the run.
  std::vector<Time> data = interceptor.times_sync_occured();

  EXPECT_GE(data.size(), static_cast<unsigned int>(3));
  for (unsigned int i = 0; i < data.size() - 1; i++) {
    Time optimal_next_sync = data[i] + poll_interval;
    // The pthreads impl uses a different time impl and is slightly (~900usecs)
    // off, so this expectation can fail with --syncer-thread-pthreads.
    EXPECT_TRUE(data[i + 1] >= optimal_next_sync)
        << "difference is "
        << (data[i + 1] - optimal_next_sync).InMicroseconds() << " usecs. "
        << "~900usec delta is OK with --syncer-thread-pthreads";
    // This should be reliable, as there are no blocking or I/O operations
    // except the explicit 2 second wait, so if it takes longer than this
    // there is a problem.
    EXPECT_TRUE(data[i + 1] < optimal_next_sync + poll_interval);
  }
}
#endif

TEST_F(SyncerThreadWithSyncerTest, Nudge) {
  SyncShareIntercept interceptor;
  connection()->SetMidCommitObserver(&interceptor);
  // We don't want a poll to happen during this test (except the first one).
  const TimeDelta poll_interval = TimeDelta::FromMinutes(5);
  syncer_thread()->SetSyncerShortPollInterval(poll_interval);
  EXPECT_TRUE(syncer_thread()->Start());
  metadb()->Open();
  interceptor.WaitForSyncShare(1, poll_interval + poll_interval);

  EXPECT_EQ(static_cast<unsigned int>(1),
            interceptor.times_sync_occured().size());
  // The SyncerThread should be waiting for the poll now.  Nudge it to sync
  // immediately (5ms).
  syncer_thread()->NudgeSyncer(5, SyncerThread::kUnknown);
  interceptor.WaitForSyncShare(1, TimeDelta::FromSeconds(1));
  EXPECT_EQ(static_cast<unsigned int>(2),
      interceptor.times_sync_occured().size());

  // SyncerThread should be waiting again.  Signal it to stop.
  EXPECT_TRUE(syncer_thread()->Stop(2000));
}

}  // namespace browser_sync
