// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <list>
#include <map>
#include <set>

#include "base/lock.h"
#include "base/scoped_ptr.h"
#include "base/time.h"
#include "base/waitable_event.h"
#include "chrome/browser/sync/engine/model_safe_worker.h"
#include "chrome/browser/sync/engine/syncer_thread.h"
#include "chrome/browser/sync/engine/syncer_types.h"
#include "chrome/browser/sync/sessions/sync_session_context.h"
#include "chrome/browser/sync/util/channel.h"
#include "chrome/test/sync/engine/mock_connection_manager.h"
#include "chrome/test/sync/engine/test_directory_setter_upper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::TimeTicks;
using base::TimeDelta;
using base::WaitableEvent;
using testing::_;
using testing::AnyNumber;
using testing::Field;

namespace browser_sync {
using sessions::SyncSessionContext;

typedef testing::Test SyncerThreadTest;
typedef SyncerThread::WaitInterval WaitInterval;

ACTION_P(SignalEvent, event) {
  event->Signal();
}

class ListenerMock : public ChannelEventHandler<SyncerEvent> {
 public:
  MOCK_METHOD1(HandleChannelEvent, void(const SyncerEvent&));
};

class SyncerThreadWithSyncerTest : public testing::Test,
                                   public ModelSafeWorkerRegistrar,
                                   public ChannelEventHandler<SyncerEvent> {
 public:
  SyncerThreadWithSyncerTest()
      : max_wait_time_(TimeDelta::FromSeconds(10)),
        sync_cycle_ended_event_(false, false) {}
  virtual void SetUp() {
    metadb_.SetUp();
    connection_.reset(new MockConnectionManager(metadb_.manager(),
                                                metadb_.name()));
    allstatus_.reset(new AllStatus());
    worker_ = new ModelSafeWorker();
    SyncSessionContext* context = new SyncSessionContext(connection_.get(),
        NULL, metadb_.manager(), this);
    syncer_thread_ = new SyncerThread(context, allstatus_.get());
    syncer_event_hookup_.reset(
        syncer_thread_->relay_channel()->AddObserver(this));
    allstatus_->WatchSyncerThread(syncer_thread_);
    syncer_thread_->SetConnected(true);
    syncable::ModelTypeBitSet expected_types;
    expected_types[syncable::BOOKMARKS] = true;
    connection_->ExpectGetUpdatesRequestTypes(expected_types);
  }
  virtual void TearDown() {
    syncer_event_hookup_.reset();
    allstatus_.reset();
    syncer_thread_ = NULL;
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
  bool WaitForThrottle() {
    int max_cycles = 5;
    while (max_cycles && !syncer_thread()->IsSyncingCurrentlySilenced()) {
      sync_cycle_ended_event_.TimedWait(max_wait_time_);
      max_cycles--;
    }

    return syncer_thread()->IsSyncingCurrentlySilenced();
  }

  void WaitForDisconnect() {
    // Wait for the SyncerThread to detect loss of connection, up to a max of
    // 10 seconds to timeout the test.
    AutoLock lock(syncer_thread()->lock_);
    TimeTicks start = TimeTicks::Now();
    TimeDelta ten_seconds = TimeDelta::FromSeconds(10);
    while (syncer_thread()->vault_.connected_) {
      syncer_thread()->vault_field_changed_.TimedWait(ten_seconds);
      if (TimeTicks::Now() - start > ten_seconds)
        break;
    }
    EXPECT_FALSE(syncer_thread()->vault_.connected_);
  }

  bool Pause(ListenerMock* listener) {
    WaitableEvent event(false, false);
    {
      AutoLock lock(syncer_thread()->lock_);
      EXPECT_CALL(*listener, HandleChannelEvent(
          Field(&SyncerEvent::what_happened, SyncerEvent::PAUSED))).
          WillOnce(SignalEvent(&event));
    }
    if (!syncer_thread()->RequestPause())
      return false;
    return event.TimedWait(max_wait_time_);
  }

  bool Resume(ListenerMock* listener) {
    WaitableEvent event(false, false);
    {
      AutoLock lock(syncer_thread()->lock_);
      EXPECT_CALL(*listener, HandleChannelEvent(
          Field(&SyncerEvent::what_happened, SyncerEvent::RESUMED))).
          WillOnce(SignalEvent(&event));
    }
    if (!syncer_thread()->RequestResume())
      return false;
    return event.TimedWait(max_wait_time_);
  }

  void PreventThreadFromPolling() {
    const TimeDelta poll_interval = TimeDelta::FromMinutes(5);
    syncer_thread()->SetSyncerShortPollInterval(poll_interval);
  }

 private:

  void HandleChannelEvent(const SyncerEvent& event) {
    if (event.what_happened == SyncerEvent::SYNC_CYCLE_ENDED)
      sync_cycle_ended_event_.Signal();
  }

 protected:
  TimeDelta max_wait_time_;

 private:
  ManuallyOpenedTestDirectorySetterUpper metadb_;
  scoped_ptr<MockConnectionManager> connection_;
  scoped_ptr<AllStatus> allstatus_;
  scoped_refptr<SyncerThread> syncer_thread_;
  scoped_refptr<ModelSafeWorker> worker_;
  scoped_ptr<ChannelHookup<SyncerEvent> > syncer_event_hookup_;
  base::WaitableEvent sync_cycle_ended_event_;
  DISALLOW_COPY_AND_ASSIGN(SyncerThreadWithSyncerTest);
};

class SyncShareIntercept
    : public MockConnectionManager::ResponseCodeOverrideRequestor,
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

  // ResponseCodeOverrideRequestor implementation. This assumes any override
  // requested is intended to silence the SyncerThread.
  virtual void OnOverrideComplete() {
    // We should not see any syncing.
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

  void Reset() {
    allow_multiple_interceptions_ = true;
    times_sync_occured_.clear();
    sync_occured_.Reset();
  }
 private:
  std::vector<TimeTicks> times_sync_occured_;
  base::WaitableEvent sync_occured_;
  bool allow_multiple_interceptions_;
  DISALLOW_COPY_AND_ASSIGN(SyncShareIntercept);
};

TEST_F(SyncerThreadTest, Construction) {
  SyncSessionContext* context = new SyncSessionContext(NULL, NULL, NULL, NULL);
  scoped_refptr<SyncerThread> syncer_thread(new SyncerThread(context, NULL));
}

TEST_F(SyncerThreadTest, StartStop) {
  SyncSessionContext* context = new SyncSessionContext(NULL, NULL, NULL, NULL);
  scoped_refptr<SyncerThread> syncer_thread(new SyncerThread(context, NULL));
  EXPECT_TRUE(syncer_thread->Start());
  EXPECT_TRUE(syncer_thread->Stop(2000));

  // Do it again for good measure.  I caught some bugs by adding this so
  // I would recommend keeping it.
  EXPECT_TRUE(syncer_thread->Start());
  EXPECT_TRUE(syncer_thread->Stop(2000));
}

TEST_F(SyncerThreadTest, CalculateSyncWaitTime) {
  SyncSessionContext* context = new SyncSessionContext(NULL, NULL, NULL, NULL);
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
  SyncSessionContext* context = new SyncSessionContext(NULL, NULL, NULL, NULL);
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
  PreventThreadFromPolling();
  EXPECT_TRUE(syncer_thread()->Start());
  metadb()->Open();
  const TimeDelta poll_interval = TimeDelta::FromMinutes(5);
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
  ASSERT_TRUE(WaitForThrottle());
  EXPECT_TRUE(syncer_thread()->IsSyncingCurrentlySilenced());

  EXPECT_TRUE(syncer_thread()->Stop(2000));
}

TEST_F(SyncerThreadWithSyncerTest, StopSyncPermanently) {
  // The SyncerThread should request an exit from the Syncer and set
  // conditions for termination.
  const TimeDelta poll_interval = TimeDelta::FromMilliseconds(10);
  syncer_thread()->SetSyncerShortPollInterval(poll_interval);

  ListenerMock listener;
  WaitableEvent sync_cycle_ended_event(false, false);
  WaitableEvent syncer_thread_exiting_event(false, false);
  scoped_ptr<ChannelHookup<SyncerEvent> > hookup;
  hookup.reset(syncer_thread()->relay_channel()->AddObserver(&listener));

  EXPECT_CALL(listener, HandleChannelEvent(
      Field(&SyncerEvent::what_happened, SyncerEvent::STATUS_CHANGED))).
      Times(AnyNumber());

  EXPECT_CALL(listener, HandleChannelEvent(
      Field(&SyncerEvent::what_happened, SyncerEvent::SYNC_CYCLE_ENDED))).
      Times(AnyNumber()).
      WillOnce(SignalEvent(&sync_cycle_ended_event));

  EXPECT_CALL(listener, HandleChannelEvent(
      Field(&SyncerEvent::what_happened,
          SyncerEvent::STOP_SYNCING_PERMANENTLY)));
  EXPECT_CALL(listener, HandleChannelEvent(
      Field(&SyncerEvent::what_happened, SyncerEvent::SYNCER_THREAD_EXITING))).
      WillOnce(SignalEvent(&syncer_thread_exiting_event));

  EXPECT_TRUE(syncer_thread()->Start());
  metadb()->Open();
  ASSERT_TRUE(sync_cycle_ended_event.TimedWait(max_wait_time_));

  connection()->set_store_birthday("NotYourLuckyDay");
  ASSERT_TRUE(syncer_thread_exiting_event.TimedWait(max_wait_time_));
  EXPECT_TRUE(syncer_thread()->Stop(0));
}

TEST_F(SyncerThreadWithSyncerTest, AuthInvalid) {
  SyncShareIntercept interceptor;
  connection()->SetMidCommitObserver(&interceptor);
  const TimeDelta poll_interval = TimeDelta::FromMilliseconds(1);

  syncer_thread()->SetSyncerShortPollInterval(poll_interval);
  EXPECT_TRUE(syncer_thread()->Start());
  metadb()->Open();

  // Wait for some healthy syncing.
  interceptor.WaitForSyncShare(2, TimeDelta::FromSeconds(10));
  EXPECT_GE(interceptor.times_sync_occured().size(), 2U);

  // Atomically start returning auth invalid and set the interceptor to fail
  // on any sync.
  connection()->FailWithAuthInvalid(&interceptor);
  WaitForDisconnect();

  // Try to trigger a sync (the interceptor will assert if one occurs).
  syncer_thread()->NudgeSyncer(0, SyncerThread::kUnknown);
  syncer_thread()->NudgeSyncer(0, SyncerThread::kUnknown);

  // Wait several poll intervals but don't expect any syncing besides the cycle
  // that lost the connection.
  interceptor.WaitForSyncShare(1, TimeDelta::FromSeconds(1));
  EXPECT_EQ(1U, interceptor.times_sync_occured().size());

  // Simulate a valid re-authentication and expect resumption of syncing.
  interceptor.Reset();
  ASSERT_TRUE(interceptor.times_sync_occured().empty());
  connection()->StopFailingWithAuthInvalid(NULL);
  ServerConnectionEvent e = {ServerConnectionEvent::STATUS_CHANGED,
                             HttpResponse::SERVER_CONNECTION_OK,
                             true};
  connection()->channel()->NotifyListeners(e);

  interceptor.WaitForSyncShare(1, TimeDelta::FromSeconds(10));
  EXPECT_FALSE(interceptor.times_sync_occured().empty());

  EXPECT_TRUE(syncer_thread()->Stop(2000));
}

// TODO(skrul): The "Pause" and "PauseWhenNotConnected" tests are
// marked FLAKY because they sometimes fail on the Windows buildbots.
// I have been unable to reproduce this hang after extensive testing
// on a local Windows machine so these tests will remain flaky in
// order to help diagnose the problem.
TEST_F(SyncerThreadWithSyncerTest, FLAKY_Pause) {
  WaitableEvent sync_cycle_ended_event(false, false);
  WaitableEvent paused_event(false, false);
  WaitableEvent resumed_event(false, false);
  PreventThreadFromPolling();

  ListenerMock listener;
  scoped_ptr<ChannelHookup<SyncerEvent> > hookup;
  hookup.reset(syncer_thread()->relay_channel()->AddObserver(&listener));

  EXPECT_CALL(listener, HandleChannelEvent(
      Field(&SyncerEvent::what_happened, SyncerEvent::STATUS_CHANGED))).
      Times(AnyNumber());

  // Wait for the initial sync to complete.
  EXPECT_CALL(listener, HandleChannelEvent(
      Field(&SyncerEvent::what_happened, SyncerEvent::SYNC_CYCLE_ENDED))).
      WillOnce(SignalEvent(&sync_cycle_ended_event));
  EXPECT_CALL(listener, HandleChannelEvent(
      Field(&SyncerEvent::what_happened, SyncerEvent::SYNCER_THREAD_EXITING)));
  ASSERT_TRUE(syncer_thread()->Start());
  metadb()->Open();
  ASSERT_TRUE(sync_cycle_ended_event.TimedWait(max_wait_time_));

  // Request a pause.
  ASSERT_TRUE(Pause(&listener));

  // Resuming the pause.
  ASSERT_TRUE(Resume(&listener));

  // Not paused, should fail.
  EXPECT_FALSE(syncer_thread()->RequestResume());

  // Request a pause.
  ASSERT_TRUE(Pause(&listener));

  // Nudge the syncer, this should do nothing while we are paused.
  syncer_thread()->NudgeSyncer(0, SyncerThread::kUnknown);

  // Resuming will cause the nudge to be processed and a sync cycle to run.
  EXPECT_CALL(listener, HandleChannelEvent(
      Field(&SyncerEvent::what_happened, SyncerEvent::SYNC_CYCLE_ENDED))).
      WillOnce(SignalEvent(&sync_cycle_ended_event));
  ASSERT_TRUE(Resume(&listener));
  ASSERT_TRUE(sync_cycle_ended_event.TimedWait(max_wait_time_));

  EXPECT_TRUE(syncer_thread()->Stop(2000));
}

TEST_F(SyncerThreadWithSyncerTest, StartWhenNotConnected) {
  WaitableEvent sync_cycle_ended_event(false, false);
  WaitableEvent event(false, false);
  ListenerMock listener;
  scoped_ptr<ChannelHookup<SyncerEvent> > hookup;
  hookup.reset(syncer_thread()->relay_channel()->AddObserver(&listener));
  PreventThreadFromPolling();

  EXPECT_CALL(listener, HandleChannelEvent(
      Field(&SyncerEvent::what_happened, SyncerEvent::STATUS_CHANGED))).
      Times(AnyNumber());
  EXPECT_CALL(listener, HandleChannelEvent(
      Field(&SyncerEvent::what_happened, SyncerEvent::SYNCER_THREAD_EXITING)));

  connection()->SetServerNotReachable();
  metadb()->Open();

  // Syncer thread will always go through once cycle at the start,
  // then it will wait for a connection.
  EXPECT_CALL(listener, HandleChannelEvent(
      Field(&SyncerEvent::what_happened, SyncerEvent::SYNC_CYCLE_ENDED))).
      WillOnce(SignalEvent(&sync_cycle_ended_event));
  EXPECT_CALL(listener, HandleChannelEvent(
      Field(&SyncerEvent::what_happened, SyncerEvent::WAITING_FOR_CONNECTION))).
      WillOnce(SignalEvent(&event));
  ASSERT_TRUE(syncer_thread()->Start());
  ASSERT_TRUE(sync_cycle_ended_event.TimedWait(max_wait_time_));
  ASSERT_TRUE(event.TimedWait(max_wait_time_));

  // Connect, will put the syncer thread into its usually poll wait.
  EXPECT_CALL(listener, HandleChannelEvent(
      Field(&SyncerEvent::what_happened, SyncerEvent::CONNECTED))).
      WillOnce(SignalEvent(&event));
  connection()->SetServerReachable();
  ASSERT_TRUE(event.TimedWait(max_wait_time_));

  // Nudge the syncer to complete a cycle.
  EXPECT_CALL(listener, HandleChannelEvent(
      Field(&SyncerEvent::what_happened, SyncerEvent::SYNC_CYCLE_ENDED))).
      WillOnce(SignalEvent(&sync_cycle_ended_event));
  syncer_thread()->NudgeSyncer(0, SyncerThread::kUnknown);
  ASSERT_TRUE(sync_cycle_ended_event.TimedWait(max_wait_time_));

  EXPECT_TRUE(syncer_thread()->Stop(2000));
}

// TODO(skrul): See TODO comment on the "Pause" test above for an
// explanation of the usage of FLAKY here.
// TODO(pinkerton): disabled due to hanging on test bots http://crbug.com/39070
TEST_F(SyncerThreadWithSyncerTest, DISABLED_PauseWhenNotConnected) {
  WaitableEvent sync_cycle_ended_event(false, false);
  WaitableEvent event(false, false);
  ListenerMock listener;
  scoped_ptr<ChannelHookup<SyncerEvent> > hookup;
  hookup.reset(syncer_thread()->relay_channel()->AddObserver(&listener));
  PreventThreadFromPolling();

  EXPECT_CALL(listener, HandleChannelEvent(
      Field(&SyncerEvent::what_happened, SyncerEvent::STATUS_CHANGED))).
      Times(AnyNumber());

  // Put the thread into a "waiting for connection" state.
  connection()->SetServerNotReachable();
  EXPECT_CALL(listener, HandleChannelEvent(
      Field(&SyncerEvent::what_happened, SyncerEvent::SYNC_CYCLE_ENDED))).
      WillOnce(SignalEvent(&sync_cycle_ended_event));
  EXPECT_CALL(listener, HandleChannelEvent(
      Field(&SyncerEvent::what_happened, SyncerEvent::WAITING_FOR_CONNECTION))).
      WillOnce(SignalEvent(&event));
  metadb()->Open();
  ASSERT_TRUE(syncer_thread()->Start());
  ASSERT_TRUE(sync_cycle_ended_event.TimedWait(max_wait_time_));
  ASSERT_TRUE(event.TimedWait(max_wait_time_));

  // Pause and resume the thread while waiting for a connection.
  ASSERT_TRUE(Pause(&listener));
  ASSERT_TRUE(Resume(&listener));

  // Make a connection and let the syncer cycle.
  EXPECT_CALL(listener, HandleChannelEvent(
      Field(&SyncerEvent::what_happened, SyncerEvent::SYNC_CYCLE_ENDED))).
      WillOnce(SignalEvent(&sync_cycle_ended_event));
  EXPECT_CALL(listener, HandleChannelEvent(
      Field(&SyncerEvent::what_happened, SyncerEvent::CONNECTED))).
      WillOnce(SignalEvent(&event));
  connection()->SetServerReachable();
  ASSERT_TRUE(event.TimedWait(max_wait_time_));
  syncer_thread()->NudgeSyncer(0, SyncerThread::kUnknown);
  ASSERT_TRUE(sync_cycle_ended_event.TimedWait(max_wait_time_));

  // Disconnect and get into the waiting for a connection state.
  EXPECT_CALL(listener, HandleChannelEvent(
      Field(&SyncerEvent::what_happened, SyncerEvent::WAITING_FOR_CONNECTION))).
      WillOnce(SignalEvent(&event));
  connection()->SetServerNotReachable();
  ASSERT_TRUE(event.TimedWait(max_wait_time_));

  // Pause so we can test getting a connection while paused.
  ASSERT_TRUE(Pause(&listener));

  // Get a connection then resume.
  EXPECT_CALL(listener, HandleChannelEvent(
      Field(&SyncerEvent::what_happened, SyncerEvent::CONNECTED))).
      WillOnce(SignalEvent(&event));
  connection()->SetServerReachable();
  ASSERT_TRUE(event.TimedWait(max_wait_time_));

  ASSERT_TRUE(Resume(&listener));

  // Cycle the syncer to show we are not longer paused.
  EXPECT_CALL(listener, HandleChannelEvent(
      Field(&SyncerEvent::what_happened, SyncerEvent::SYNC_CYCLE_ENDED))).
      WillOnce(SignalEvent(&sync_cycle_ended_event));
  EXPECT_CALL(listener, HandleChannelEvent(
      Field(&SyncerEvent::what_happened, SyncerEvent::SYNCER_THREAD_EXITING)));

  syncer_thread()->NudgeSyncer(0, SyncerThread::kUnknown);
  ASSERT_TRUE(sync_cycle_ended_event.TimedWait(max_wait_time_));

  EXPECT_TRUE(syncer_thread()->Stop(2000));
}

TEST_F(SyncerThreadWithSyncerTest, PauseResumeWhenNotRunning) {
  WaitableEvent sync_cycle_ended_event(false, false);
  WaitableEvent event(false, false);
  ListenerMock listener;
  scoped_ptr<ChannelHookup<SyncerEvent> > hookup;
  hookup.reset(syncer_thread()->relay_channel()->AddObserver(&listener));
  PreventThreadFromPolling();

  EXPECT_CALL(listener, HandleChannelEvent(
      Field(&SyncerEvent::what_happened, SyncerEvent::STATUS_CHANGED))).
      Times(AnyNumber());

  // Pause and resume the syncer while not running
  ASSERT_TRUE(Pause(&listener));
  ASSERT_TRUE(Resume(&listener));

  // Pause the thread then start the syncer.
  ASSERT_TRUE(Pause(&listener));
  metadb()->Open();
  ASSERT_TRUE(syncer_thread()->Start());

  // Resume and let the syncer cycle.
  EXPECT_CALL(listener, HandleChannelEvent(
      Field(&SyncerEvent::what_happened, SyncerEvent::SYNC_CYCLE_ENDED))).
      WillOnce(SignalEvent(&sync_cycle_ended_event));
  EXPECT_CALL(listener, HandleChannelEvent(
      Field(&SyncerEvent::what_happened, SyncerEvent::SYNCER_THREAD_EXITING)));

  ASSERT_TRUE(Resume(&listener));
  ASSERT_TRUE(sync_cycle_ended_event.TimedWait(max_wait_time_));
  EXPECT_TRUE(syncer_thread()->Stop(2000));
}

}  // namespace browser_sync
