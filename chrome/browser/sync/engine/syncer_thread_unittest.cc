// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <list>
#include <map>
#include <set>

#include "base/command_line.h"
#include "base/scoped_ptr.h"
#include "base/time.h"
#include "base/waitable_event.h"
#include "chrome/browser/sync/engine/model_safe_worker.h"
#include "chrome/browser/sync/engine/syncer_thread.h"
#include "chrome/browser/sync/sessions/sync_session_context.h"
#include "chrome/test/sync/engine/mock_server_connection.h"
#include "chrome/test/sync/engine/test_directory_setter_upper.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::TimeTicks;
using base::TimeDelta;

namespace browser_sync {
using sessions::SyncSessionContext;

typedef testing::Test SyncerThreadTest;
typedef SyncerThread::WaitInterval WaitInterval;

class SyncerThreadWithSyncerTest : public testing::Test,
                                   public ModelSafeWorkerRegistrar {
 public:
  SyncerThreadWithSyncerTest() : sync_cycle_ended_event_(false, false) {}
  virtual void SetUp() {
    metadb_.SetUp();
    connection_.reset(new MockConnectionManager(metadb_.manager(),
                                                metadb_.name()));
    allstatus_.reset(new AllStatus());
    worker_ = new ModelSafeWorker();
    SyncSessionContext* context = new SyncSessionContext(connection_.get(),
        metadb_.manager(), this);
    syncer_thread_ = new SyncerThread(context, allstatus_.get());
    syncer_event_hookup_.reset(
        NewEventListenerHookup(syncer_thread_->relay_channel(), this,
            &SyncerThreadWithSyncerTest::HandleSyncerEvent));
    allstatus_->WatchSyncerThread(syncer_thread_);
    syncer_thread_->SetConnected(true);
  }
  virtual void TearDown() {
    syncer_thread_ = NULL;
    allstatus_.reset();
    connection_.reset();
    metadb_.TearDown();
  }

  // ModelSafeWorkerRegistrar implementation.
  virtual void GetWorkers(std::vector<ModelSafeWorker*>* out) {
    out->push_back(worker_.get());
  }

  virtual void GetModelSafeRoutingInfo(ModelSafeRoutingInfo* out) {
    // We're just testing the sync engine here, so we shunt everything to
    // the SyncerThread.
    (*out)[syncable::BOOKMARKS] = GROUP_PASSIVE;
  }

  ManuallyOpenedTestDirectorySetterUpper* metadb() { return &metadb_; }
  MockConnectionManager* connection() { return connection_.get(); }
  SyncerThread* syncer_thread() { return syncer_thread_; }

  // Waits an indefinite amount of sync cycles for the syncer thread to become
  // throttled.  Only call this if a throttle is supposed to occur!
  void WaitForThrottle() {
    while (!syncer_thread()->IsSyncingCurrentlySilenced())
      sync_cycle_ended_event_.Wait();
  }

 private:

  void HandleSyncerEvent(const SyncerEvent& event) {
    if (event.what_happened == SyncerEvent::SYNC_CYCLE_ENDED)
      sync_cycle_ended_event_.Signal();
  }

  ManuallyOpenedTestDirectorySetterUpper metadb_;
  scoped_ptr<MockConnectionManager> connection_;
  scoped_ptr<AllStatus> allstatus_;
  scoped_refptr<SyncerThread> syncer_thread_;
  scoped_refptr<ModelSafeWorker> worker_;
  scoped_ptr<EventListenerHookup> syncer_event_hookup_;
  base::WaitableEvent sync_cycle_ended_event_;
  DISALLOW_COPY_AND_ASSIGN(SyncerThreadWithSyncerTest);
};

class SyncShareIntercept : public MockConnectionManager::ThrottleRequestVisitor,
                           public MockConnectionManager::MidCommitObserver {
 public:
  SyncShareIntercept() : sync_occured_(false, false),
                         allow_multiple_interceptions_(true) {}
  virtual ~SyncShareIntercept() {}
  virtual void Observe() {
    if (!allow_multiple_interceptions_ && !times_sync_occured_.empty())
      FAIL() << "Multiple sync shares occured.";
    times_sync_occured_.push_back(TimeTicks::Now());
    sync_occured_.Signal();
  }

  // ThrottleRequestVisitor implementation.
  virtual void VisitAtomically() {
    // Server has told the client to throttle.  We should not see any syncing.
    allow_multiple_interceptions_ = false;
    times_sync_occured_.clear();
  }

  void WaitForSyncShare(int at_least_this_many, TimeDelta max_wait) {
    while (at_least_this_many-- > 0)
      sync_occured_.TimedWait(max_wait);
  }
  std::vector<TimeTicks> times_sync_occured() const {
    return times_sync_occured_;
  }
 private:
  std::vector<TimeTicks> times_sync_occured_;
  base::WaitableEvent sync_occured_;
  bool allow_multiple_interceptions_;
  DISALLOW_COPY_AND_ASSIGN(SyncShareIntercept);
};

TEST_F(SyncerThreadTest, Construction) {
  SyncSessionContext* context = new SyncSessionContext(NULL, NULL, NULL);
  scoped_refptr<SyncerThread> syncer_thread(new SyncerThread(context, NULL));
}

TEST_F(SyncerThreadTest, StartStop) {
  SyncSessionContext* context = new SyncSessionContext(NULL, NULL, NULL);
  scoped_refptr<SyncerThread> syncer_thread(new SyncerThread(context, NULL));
  EXPECT_TRUE(syncer_thread->Start());
  EXPECT_TRUE(syncer_thread->Stop(2000));

  // Do it again for good measure.  I caught some bugs by adding this so
  // I would recommend keeping it.
  EXPECT_TRUE(syncer_thread->Start());
  EXPECT_TRUE(syncer_thread->Stop(2000));
}

TEST_F(SyncerThreadTest, CalculateSyncWaitTime) {
  SyncSessionContext* context = new SyncSessionContext(NULL, NULL, NULL);
  scoped_refptr<SyncerThread> syncer_thread(new SyncerThread(context, NULL));
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
  ASSERT_TRUE(last_poll_time <=
              syncer_thread->CalculateSyncWaitTime(last_poll_time, 10000));
  ASSERT_TRUE(last_poll_time * 3 >=
              syncer_thread->CalculateSyncWaitTime(last_poll_time, 10000));
  ASSERT_TRUE(last_poll_time <=
              syncer_thread->CalculateSyncWaitTime(last_poll_time, 100000));
  ASSERT_TRUE(last_poll_time * 3 >=
              syncer_thread->CalculateSyncWaitTime(last_poll_time, 100000));

  // Maximum backoff time should be syncer_max_interval.
  int near_threshold = SyncerThread::kDefaultMaxPollIntervalMs / 2 - 1;
  int threshold = SyncerThread::kDefaultMaxPollIntervalMs;
  int over_threshold = SyncerThread::kDefaultMaxPollIntervalMs + 1;
  ASSERT_TRUE(near_threshold <=
              syncer_thread->CalculateSyncWaitTime(near_threshold, 10000));
  ASSERT_TRUE(SyncerThread::kDefaultMaxPollIntervalMs >=
              syncer_thread->CalculateSyncWaitTime(near_threshold, 10000));
  ASSERT_TRUE(SyncerThread::kDefaultMaxPollIntervalMs ==
              syncer_thread->CalculateSyncWaitTime(threshold, 10000));
  ASSERT_TRUE(SyncerThread::kDefaultMaxPollIntervalMs ==
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
  ASSERT_TRUE(last_poll_time <=
              syncer_thread->CalculateSyncWaitTime(last_poll_time,
                                                   over_sync_max_interval));
  ASSERT_TRUE(last_poll_time * 3 >=
              syncer_thread->CalculateSyncWaitTime(last_poll_time,
                                                   over_sync_max_interval));
}

TEST_F(SyncerThreadTest, CalculatePollingWaitTime) {
  // Set up the environment.
  int user_idle_milliseconds_param = 0;
  SyncSessionContext* context = new SyncSessionContext(NULL, NULL, NULL);
  scoped_refptr<SyncerThread> syncer_thread(new SyncerThread(context, NULL));
  syncer_thread->DisableIdleDetection();
  // Hold the lock to appease asserts in code.
  AutoLock lock(syncer_thread->lock_);

  // Notifications disabled should result in a polling interval of
  // kDefaultShortPollInterval.
  {
    AllStatus::Status status = {};
    status.notifications_enabled = 0;
    bool continue_sync_cycle_param = false;

    // No work and no backoff.
    WaitInterval interval = syncer_thread->CalculatePollingWaitTime(
        status,
        0,
        &user_idle_milliseconds_param,
        &continue_sync_cycle_param,
        false);

    ASSERT_EQ(SyncerThread::kDefaultShortPollIntervalSeconds,
              interval.poll_delta.InSeconds());
    ASSERT_EQ(WaitInterval::NORMAL, interval.mode);
    ASSERT_FALSE(interval.had_nudge_during_backoff);
    ASSERT_FALSE(continue_sync_cycle_param);

    // In this case the continue_sync_cycle is turned off.
    continue_sync_cycle_param = true;
    interval = syncer_thread->CalculatePollingWaitTime(
        status,
        0,
        &user_idle_milliseconds_param,
        &continue_sync_cycle_param,
        false);

    ASSERT_EQ(SyncerThread::kDefaultShortPollIntervalSeconds,
        interval.poll_delta.InSeconds());
    ASSERT_EQ(WaitInterval::NORMAL, interval.mode);
    ASSERT_FALSE(interval.had_nudge_during_backoff);
    ASSERT_FALSE(continue_sync_cycle_param);
  }

  // Notifications enabled should result in a polling interval of
  // SyncerThread::kDefaultLongPollIntervalSeconds.
  {
    AllStatus::Status status = {};
    status.notifications_enabled = 1;
    bool continue_sync_cycle_param = false;

    // No work and no backoff.
    WaitInterval interval = syncer_thread->CalculatePollingWaitTime(
        status,
        0,
        &user_idle_milliseconds_param,
        &continue_sync_cycle_param,
        false);

    ASSERT_EQ(SyncerThread::kDefaultLongPollIntervalSeconds,
              interval.poll_delta.InSeconds());
    ASSERT_EQ(WaitInterval::NORMAL, interval.mode);
    ASSERT_FALSE(interval.had_nudge_during_backoff);
    ASSERT_FALSE(continue_sync_cycle_param);

    // In this case the continue_sync_cycle is turned off.
    continue_sync_cycle_param = true;
    interval = syncer_thread->CalculatePollingWaitTime(
        status,
        0,
        &user_idle_milliseconds_param,
        &continue_sync_cycle_param,
        false);

    ASSERT_EQ(SyncerThread::kDefaultLongPollIntervalSeconds,
              interval.poll_delta.InSeconds());
    ASSERT_EQ(WaitInterval::NORMAL, interval.mode);
    ASSERT_FALSE(interval.had_nudge_during_backoff);
    ASSERT_FALSE(continue_sync_cycle_param);
  }

  // There are two states which can cause a continuation, either the updates
  // available do not match the updates received, or the unsynced count is
  // non-zero.
  {
    AllStatus::Status status = {};
    status.updates_available = 1;
    status.updates_received = 0;
    bool continue_sync_cycle_param = false;

    WaitInterval interval = syncer_thread->CalculatePollingWaitTime(
        status,
        0,
        &user_idle_milliseconds_param,
        &continue_sync_cycle_param,
        false);

    ASSERT_LE(0, interval.poll_delta.InSeconds());
    ASSERT_EQ(WaitInterval::NORMAL, interval.mode);
    ASSERT_FALSE(interval.had_nudge_during_backoff);
    ASSERT_TRUE(continue_sync_cycle_param);

    continue_sync_cycle_param = false;
    interval = syncer_thread->CalculatePollingWaitTime(
        status,
        0,
        &user_idle_milliseconds_param,
        &continue_sync_cycle_param,
        false);

    ASSERT_GE(3, interval.poll_delta.InSeconds());
    ASSERT_EQ(WaitInterval::NORMAL, interval.mode);
    ASSERT_FALSE(interval.had_nudge_during_backoff);
    ASSERT_TRUE(continue_sync_cycle_param);

    interval = syncer_thread->CalculatePollingWaitTime(
        status,
        0,
        &user_idle_milliseconds_param,
        &continue_sync_cycle_param,
        false);

    ASSERT_LE(0, interval.poll_delta.InSeconds());
    ASSERT_EQ(WaitInterval::EXPONENTIAL_BACKOFF, interval.mode);
    ASSERT_FALSE(interval.had_nudge_during_backoff);

    interval = syncer_thread->CalculatePollingWaitTime(
        status,
        0,
        &user_idle_milliseconds_param,
        &continue_sync_cycle_param,
        false);

    ASSERT_GE(2, interval.poll_delta.InSeconds());
    ASSERT_EQ(WaitInterval::EXPONENTIAL_BACKOFF, interval.mode);
    ASSERT_FALSE(interval.had_nudge_during_backoff);
    ASSERT_TRUE(continue_sync_cycle_param);

    status.updates_received = 1;
    interval = syncer_thread->CalculatePollingWaitTime(
        status,
        0,
        &user_idle_milliseconds_param,
        &continue_sync_cycle_param,
        false);

    ASSERT_EQ(SyncerThread::kDefaultShortPollIntervalSeconds,
                interval.poll_delta.InSeconds());
    ASSERT_EQ(WaitInterval::NORMAL, interval.mode);
    ASSERT_FALSE(interval.had_nudge_during_backoff);
    ASSERT_FALSE(continue_sync_cycle_param);
  }

  {
    AllStatus::Status status = {};
    status.unsynced_count = 1;
    bool continue_sync_cycle_param = false;

    WaitInterval interval = syncer_thread->CalculatePollingWaitTime(
        status,
        0,
        &user_idle_milliseconds_param,
        &continue_sync_cycle_param,
        false);

    ASSERT_LE(0, interval.poll_delta.InSeconds());
    ASSERT_EQ(WaitInterval::NORMAL, interval.mode);
    ASSERT_FALSE(interval.had_nudge_during_backoff);
    ASSERT_TRUE(continue_sync_cycle_param);

    continue_sync_cycle_param = false;
    interval = syncer_thread->CalculatePollingWaitTime(
        status,
        0,
        &user_idle_milliseconds_param,
        &continue_sync_cycle_param,
        false);

    ASSERT_GE(2, interval.poll_delta.InSeconds());
    ASSERT_EQ(WaitInterval::NORMAL, interval.mode);
    ASSERT_FALSE(interval.had_nudge_during_backoff);
    ASSERT_TRUE(continue_sync_cycle_param);

    status.unsynced_count = 0;
    interval = syncer_thread->CalculatePollingWaitTime(
        status,
        4,
        &user_idle_milliseconds_param,
        &continue_sync_cycle_param,
        false);

    ASSERT_EQ(SyncerThread::kDefaultShortPollIntervalSeconds,
              interval.poll_delta.InSeconds());
    ASSERT_EQ(WaitInterval::NORMAL, interval.mode);
    ASSERT_FALSE(interval.had_nudge_during_backoff);
    ASSERT_FALSE(continue_sync_cycle_param);
  }

  // Regression for exponential backoff reset when the syncer is nudged.
  {
    AllStatus::Status status = {};
    status.unsynced_count = 1;
    bool continue_sync_cycle_param = false;

    // Expect move from default polling interval to exponential backoff due to
    // unsynced_count != 0.
    WaitInterval interval = syncer_thread->CalculatePollingWaitTime(
        status,
        3600,
        &user_idle_milliseconds_param,
        &continue_sync_cycle_param,
        false);

    ASSERT_LE(0, interval.poll_delta.InSeconds());
    ASSERT_EQ(WaitInterval::NORMAL, interval.mode);
    ASSERT_FALSE(interval.had_nudge_during_backoff);
    ASSERT_TRUE(continue_sync_cycle_param);

    continue_sync_cycle_param = false;
    interval = syncer_thread->CalculatePollingWaitTime(
        status,
        3600,
        &user_idle_milliseconds_param,
        &continue_sync_cycle_param,
        false);

    ASSERT_GE(2, interval.poll_delta.InSeconds());
    ASSERT_EQ(WaitInterval::NORMAL, interval.mode);
    ASSERT_FALSE(interval.had_nudge_during_backoff);
    ASSERT_TRUE(continue_sync_cycle_param);

    // Expect exponential backoff.
    interval = syncer_thread->CalculatePollingWaitTime(
        status,
        2,
        &user_idle_milliseconds_param,
        &continue_sync_cycle_param,
        false);

    ASSERT_LE(2, interval.poll_delta.InSeconds());
    ASSERT_EQ(WaitInterval::EXPONENTIAL_BACKOFF, interval.mode);
    ASSERT_FALSE(interval.had_nudge_during_backoff);
    ASSERT_TRUE(continue_sync_cycle_param);

    interval = syncer_thread->CalculatePollingWaitTime(
        status,
        2,
        &user_idle_milliseconds_param,
        &continue_sync_cycle_param,
        false);

    ASSERT_GE(6, interval.poll_delta.InSeconds());
    ASSERT_EQ(WaitInterval::EXPONENTIAL_BACKOFF, interval.mode);
    ASSERT_FALSE(interval.had_nudge_during_backoff);
    ASSERT_TRUE(continue_sync_cycle_param);

    syncer_thread->vault_.current_wait_interval_ = interval;

    interval = syncer_thread->CalculatePollingWaitTime(
        status,
        static_cast<int>(interval.poll_delta.InSeconds()),
        &user_idle_milliseconds_param,
        &continue_sync_cycle_param,
        true);

    // Don't change poll on a failed nudge during backoff.
    ASSERT_TRUE(syncer_thread->vault_.current_wait_interval_.poll_delta ==
              interval.poll_delta);
    ASSERT_EQ(WaitInterval::EXPONENTIAL_BACKOFF, interval.mode);
    ASSERT_TRUE(interval.had_nudge_during_backoff);
    ASSERT_TRUE(continue_sync_cycle_param);

    // If we got a nudge and we weren't in backoff mode, we see exponential
    // backoff.
    syncer_thread->vault_.current_wait_interval_.mode = WaitInterval::NORMAL;
    interval = syncer_thread->CalculatePollingWaitTime(
        status,
        2,
        &user_idle_milliseconds_param,
        &continue_sync_cycle_param,
        true);

    // 5 and 3 are bounds on the backoff randomization formula given input of 2.
    ASSERT_GE(5, interval.poll_delta.InSeconds());
    ASSERT_LE(3, interval.poll_delta.InSeconds());
    ASSERT_EQ(WaitInterval::EXPONENTIAL_BACKOFF, interval.mode);
    ASSERT_FALSE(interval.had_nudge_during_backoff);
    ASSERT_TRUE(continue_sync_cycle_param);

    // And if another interval expires, we get a bigger backoff.
    WaitInterval new_interval = syncer_thread->CalculatePollingWaitTime(
        status,
        static_cast<int>(interval.poll_delta.InSeconds()),
        &user_idle_milliseconds_param,
        &continue_sync_cycle_param,
        false);

    ASSERT_GE(12, new_interval.poll_delta.InSeconds());
    ASSERT_LE(5, new_interval.poll_delta.InSeconds());
    ASSERT_EQ(WaitInterval::EXPONENTIAL_BACKOFF, interval.mode);
    ASSERT_FALSE(new_interval.had_nudge_during_backoff);
    ASSERT_TRUE(continue_sync_cycle_param);

    // A nudge resets the continue_sync_cycle_param value, so our backoff
    // should return to the minimum.
    continue_sync_cycle_param = false;
    interval = syncer_thread->CalculatePollingWaitTime(
        status,
        3600,
        &user_idle_milliseconds_param,
        &continue_sync_cycle_param,
        true);

    ASSERT_LE(0, interval.poll_delta.InSeconds());
    ASSERT_EQ(WaitInterval::NORMAL, interval.mode);
    ASSERT_FALSE(interval.had_nudge_during_backoff);
    ASSERT_TRUE(continue_sync_cycle_param);

    continue_sync_cycle_param = false;
    interval = syncer_thread->CalculatePollingWaitTime(
        status,
        3600,
        &user_idle_milliseconds_param,
        &continue_sync_cycle_param,
        true);

    ASSERT_GE(2, interval.poll_delta.InSeconds());
    ASSERT_EQ(WaitInterval::NORMAL, interval.mode);
    ASSERT_FALSE(interval.had_nudge_during_backoff);
    ASSERT_TRUE(continue_sync_cycle_param);

    // Setting unsynced_count = 0 returns us to the default polling interval.
    status.unsynced_count = 0;
    interval = syncer_thread->CalculatePollingWaitTime(
        status,
        4,
        &user_idle_milliseconds_param,
        &continue_sync_cycle_param,
        true);

    ASSERT_EQ(SyncerThread::kDefaultShortPollIntervalSeconds,
              interval.poll_delta.InSeconds());
    ASSERT_EQ(WaitInterval::NORMAL, interval.mode);
    ASSERT_FALSE(interval.had_nudge_during_backoff);
    ASSERT_FALSE(continue_sync_cycle_param);
  }
}

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
  std::vector<TimeTicks> data = interceptor.times_sync_occured();

  EXPECT_GE(data.size(), static_cast<unsigned int>(3));
  for (unsigned int i = 0; i < data.size() - 1; i++) {
    TimeTicks optimal_next_sync = data[i] + poll_interval;
    EXPECT_TRUE(data[i + 1] >= optimal_next_sync);
    // This should be reliable, as there are no blocking or I/O operations
    // except the explicit 2 second wait, so if it takes longer than this
    // there is a problem.
    EXPECT_TRUE(data[i + 1] < optimal_next_sync + poll_interval);
  }
}

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

TEST_F(SyncerThreadWithSyncerTest, Throttling) {
  SyncShareIntercept interceptor;
  connection()->SetMidCommitObserver(&interceptor);
  const TimeDelta poll_interval = TimeDelta::FromMilliseconds(10);
  syncer_thread()->SetSyncerShortPollInterval(poll_interval);

  EXPECT_TRUE(syncer_thread()->Start());
  metadb()->Open();

  // Wait for some healthy syncing.
  interceptor.WaitForSyncShare(4, poll_interval + poll_interval);

  // Tell the server to throttle a single request, which should be all it takes
  // to silence our syncer (for 2 hours, so we shouldn't hit that in this test).
  // This will atomically visit the interceptor so it can switch to throttled
  // mode and fail on multiple requests.
  connection()->ThrottleNextRequest(&interceptor);

  // Try to trigger a sync (we have a really short poll interval already).
  syncer_thread()->NudgeSyncer(0, SyncerThread::kUnknown);
  syncer_thread()->NudgeSyncer(0, SyncerThread::kUnknown);

  // Wait until the syncer thread reports that it is throttled.  Any further
  // sync share interceptions will result in failure.  If things are broken,
  // we may never halt.
  WaitForThrottle();
  EXPECT_TRUE(syncer_thread()->IsSyncingCurrentlySilenced());

  EXPECT_TRUE(syncer_thread()->Stop(2000));
}

}  // namespace browser_sync
