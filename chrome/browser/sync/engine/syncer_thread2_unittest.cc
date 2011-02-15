// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/synchronization/waitable_event.h"
#include "base/test/test_timeouts.h"
#include "chrome/browser/sync/engine/mock_model_safe_workers.h"
#include "chrome/browser/sync/engine/syncer.h"
#include "chrome/browser/sync/engine/syncer_thread2.h"
#include "chrome/browser/sync/sessions/test_util.h"
#include "chrome/test/sync/engine/test_directory_setter_upper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gmock/include/gmock/gmock.h"

using base::TimeDelta;
using base::TimeTicks;
using testing::_;
using testing::AtLeast;
using testing::DoAll;
using testing::Eq;
using testing::Invoke;
using testing::Mock;
using testing::Return;
using testing::WithArg;

namespace browser_sync {
using sessions::SyncSession;
using sessions::SyncSessionContext;
using sessions::SyncSessionSnapshot;
using syncable::ModelTypeBitSet;
using sync_pb::GetUpdatesCallerInfo;

class MockSyncer : public Syncer {
 public:
  MOCK_METHOD3(SyncShare, void(sessions::SyncSession*, SyncerStep,
                               SyncerStep));
};

namespace s3 {

// Used when tests want to record syncing activity to examine later.
struct SyncShareRecords {
  std::vector<TimeTicks> times;
  std::vector<linked_ptr<SyncSessionSnapshot> > snapshots;
};

// Convenient to use in tests wishing to analyze SyncShare calls over time.
static const size_t kMinNumSamples = 5;

class SyncerThread2Test : public testing::Test {
 public:
  class MockDelayProvider : public SyncerThread::DelayProvider {
   public:
    MOCK_METHOD1(GetDelay, TimeDelta(const TimeDelta&));
  };

  virtual void SetUp() {
    syncdb_.SetUp();
    syncer_ = new MockSyncer();
    delay_ = NULL;
    registrar_.reset(MockModelSafeWorkerRegistrar::PassiveBookmarks());
    context_ = new SyncSessionContext(NULL, syncdb_.manager(),
        registrar_.get(), std::vector<SyncEngineEventListener*>());
    context_->set_notifications_enabled(true);
    context_->set_account_name("Test");
    syncer_thread_.reset(new SyncerThread(context_, syncer_));
    // TODO(tim): Once the SCM is hooked up, remove this.
    syncer_thread_->server_connection_ok_ = true;
  }

  SyncerThread* syncer_thread() { return syncer_thread_.get(); }
  MockSyncer* syncer() { return syncer_; }
  MockDelayProvider* delay() { return delay_; }
  TimeDelta zero() { return TimeDelta::FromSeconds(0); }
  TimeDelta timeout() {
    return TimeDelta::FromMilliseconds(TestTimeouts::action_timeout_ms());
  }

  virtual void TearDown() {
    syncer_thread()->Stop();
    syncdb_.TearDown();
  }

  void AnalyzePollRun(const SyncShareRecords& records, size_t min_num_samples,
      const TimeTicks& optimal_start, const TimeDelta& poll_interval) {
    const std::vector<TimeTicks>& data(records.times);
    EXPECT_GE(data.size(), min_num_samples);
    for (size_t i = 0; i < data.size(); i++) {
      SCOPED_TRACE(testing::Message() << "SyncShare # (" << i << ")");
      TimeTicks optimal_next_sync = optimal_start + poll_interval * i;
      EXPECT_GE(data[i], optimal_next_sync);
      EXPECT_EQ(GetUpdatesCallerInfo::PERIODIC,
                records.snapshots[i]->source.updates_source);
    }
  }

  bool GetBackoffAndResetTest(base::WaitableEvent* done) {
    syncable::ModelTypeBitSet nudge_types;
    syncer_thread()->Start(SyncerThread::NORMAL_MODE);
    syncer_thread()->ScheduleNudge(zero(), NUDGE_SOURCE_LOCAL, nudge_types);
    done->TimedWait(timeout());
    TearDown();
    done->Reset();
    Mock::VerifyAndClearExpectations(syncer());
    bool backing_off = syncer_thread()->IsBackingOff();
    SetUp();
    UseMockDelayProvider();
    EXPECT_CALL(*delay(), GetDelay(_))
        .WillRepeatedly(Return(TimeDelta::FromMilliseconds(1)));
    return backing_off;
  }

  void UseMockDelayProvider() {
    delay_ = new MockDelayProvider();
    syncer_thread_->delay_provider_.reset(delay_);
  }

  void PostSignalTask(base::WaitableEvent* done) {
    syncer_thread_->thread_.message_loop()->PostTask(FROM_HERE,
        NewRunnableFunction(&SyncerThread2Test::SignalWaitableEvent, done));
  }

  void FlushLastTask(base::WaitableEvent* done) {
    PostSignalTask(done);
    done->TimedWait(timeout());
    done->Reset();
  }

  static void SignalWaitableEvent(base::WaitableEvent* event) {
    event->Signal();
  }

  // Compare a ModelTyepBitSet to a TypePayloadMap, ignoring payload values.
  bool CompareModelTypeBitSetToTypePayloadMap(
      const syncable::ModelTypeBitSet& lhs,
      const sessions::TypePayloadMap& rhs) {
    size_t count = 0;
    for (sessions::TypePayloadMap::const_iterator i = rhs.begin();
         i != rhs.end(); ++i, ++count) {
      if (!lhs.test(i->first))
        return false;
    }
    if (lhs.count() != count)
      return false;
    return true;
  }

 private:
  scoped_ptr<SyncerThread> syncer_thread_;
  SyncSessionContext* context_;
  MockSyncer* syncer_;
  MockDelayProvider* delay_;
  scoped_ptr<MockModelSafeWorkerRegistrar> registrar_;
  MockDirectorySetterUpper syncdb_;
};

bool RecordSyncShareImpl(SyncSession* s, SyncShareRecords* record,
                         size_t signal_after) {
  record->times.push_back(TimeTicks::Now());
  record->snapshots.push_back(make_linked_ptr(new SyncSessionSnapshot(
      s->TakeSnapshot())));
  return record->times.size() >= signal_after;
}

ACTION_P4(RecordSyncShareAndPostSignal, record, signal_after, test, event) {
  if (RecordSyncShareImpl(arg0, record, signal_after) && event)
    test->PostSignalTask(event);
}

ACTION_P3(RecordSyncShare, record, signal_after, event) {
  if (RecordSyncShareImpl(arg0, record, signal_after) && event)
    event->Signal();
}

ACTION_P(SignalEvent, event) {
  SyncerThread2Test::SignalWaitableEvent(event);
}

// Test nudge scheduling.
TEST_F(SyncerThread2Test, Nudge) {
  syncer_thread()->Start(SyncerThread::NORMAL_MODE);
  base::WaitableEvent done(false, false);
  SyncShareRecords records;
  syncable::ModelTypeBitSet model_types;
  model_types[syncable::BOOKMARKS] = true;

  EXPECT_CALL(*syncer(), SyncShare(_,_,_))
      .WillOnce(DoAll(Invoke(sessions::test_util::SimulateSuccess),
          WithArg<0>(RecordSyncShare(&records, 1U, &done))))
      .RetiresOnSaturation();
  syncer_thread()->ScheduleNudge(zero(), NUDGE_SOURCE_LOCAL, model_types);
  done.TimedWait(timeout());

  EXPECT_EQ(1U, records.snapshots.size());
  EXPECT_TRUE(CompareModelTypeBitSetToTypePayloadMap(model_types,
      records.snapshots[0]->source.types));
  EXPECT_EQ(GetUpdatesCallerInfo::LOCAL,
            records.snapshots[0]->source.updates_source);

  // Make sure a second, later, nudge is unaffected by first (no coalescing).
  SyncShareRecords records2;
  model_types[syncable::BOOKMARKS] = false;
  model_types[syncable::AUTOFILL] = true;
  EXPECT_CALL(*syncer(), SyncShare(_,_,_))
      .WillOnce(DoAll(Invoke(sessions::test_util::SimulateSuccess),
          WithArg<0>(RecordSyncShare(&records2, 1U, &done))));
  syncer_thread()->ScheduleNudge(zero(), NUDGE_SOURCE_LOCAL, model_types);
  done.TimedWait(timeout());

  EXPECT_EQ(1U, records2.snapshots.size());
  EXPECT_TRUE(CompareModelTypeBitSetToTypePayloadMap(model_types,
      records2.snapshots[0]->source.types));
  EXPECT_EQ(GetUpdatesCallerInfo::LOCAL,
            records2.snapshots[0]->source.updates_source);
}

// Test that nudges are coalesced.
TEST_F(SyncerThread2Test, NudgeCoalescing) {
  syncer_thread()->Start(SyncerThread::NORMAL_MODE);
  base::WaitableEvent done(false, false);
  SyncShareRecords r;
  EXPECT_CALL(*syncer(), SyncShare(_,_,_))
      .WillOnce(DoAll(Invoke(sessions::test_util::SimulateSuccess),
           WithArg<0>(RecordSyncShare(&r, 1U, &done))));
  syncable::ModelTypeBitSet types1, types2, types3;
  types1[syncable::BOOKMARKS] = true;
  types2[syncable::AUTOFILL] = true;
  types3[syncable::THEMES] = true;
  TimeDelta delay = TimeDelta::FromMilliseconds(20);
  TimeTicks optimal_time = TimeTicks::Now() + delay;
  syncer_thread()->ScheduleNudge(delay, NUDGE_SOURCE_UNKNOWN, types1);
  syncer_thread()->ScheduleNudge(zero(), NUDGE_SOURCE_LOCAL, types2);
  syncer_thread()->ScheduleNudge(zero(), NUDGE_SOURCE_NOTIFICATION, types3);
  done.TimedWait(timeout());

  EXPECT_EQ(1U, r.snapshots.size());
  EXPECT_GE(r.times[0], optimal_time);
  EXPECT_TRUE(CompareModelTypeBitSetToTypePayloadMap(types1 | types2 | types3,
      r.snapshots[0]->source.types));
  EXPECT_EQ(GetUpdatesCallerInfo::NOTIFICATION,
            r.snapshots[0]->source.updates_source);

  SyncShareRecords r2;
  EXPECT_CALL(*syncer(), SyncShare(_,_,_))
      .WillOnce(DoAll(Invoke(sessions::test_util::SimulateSuccess),
           WithArg<0>(RecordSyncShare(&r2, 1U, &done))));
  syncer_thread()->ScheduleNudge(zero(), NUDGE_SOURCE_NOTIFICATION, types3);
  done.TimedWait(timeout());
  EXPECT_EQ(1U, r2.snapshots.size());
  EXPECT_TRUE(CompareModelTypeBitSetToTypePayloadMap(types3,
      r2.snapshots[0]->source.types));
  EXPECT_EQ(GetUpdatesCallerInfo::NOTIFICATION,
            r2.snapshots[0]->source.updates_source);
}

// Test nudge scheduling.
TEST_F(SyncerThread2Test, NudgeWithPayloads) {
  syncer_thread()->Start(SyncerThread::NORMAL_MODE);
  base::WaitableEvent done(false, false);
  SyncShareRecords records;
  sessions::TypePayloadMap model_types_with_payloads;
  model_types_with_payloads[syncable::BOOKMARKS] = "test";

  EXPECT_CALL(*syncer(), SyncShare(_,_,_))
      .WillOnce(DoAll(Invoke(sessions::test_util::SimulateSuccess),
          WithArg<0>(RecordSyncShare(&records, 1U, &done))))
      .RetiresOnSaturation();
  syncer_thread()->ScheduleNudgeWithPayloads(zero(), NUDGE_SOURCE_LOCAL,
      model_types_with_payloads);
  done.TimedWait(timeout());

  EXPECT_EQ(1U, records.snapshots.size());
  EXPECT_EQ(model_types_with_payloads, records.snapshots[0]->source.types);
  EXPECT_EQ(GetUpdatesCallerInfo::LOCAL,
            records.snapshots[0]->source.updates_source);

  // Make sure a second, later, nudge is unaffected by first (no coalescing).
  SyncShareRecords records2;
  model_types_with_payloads.erase(syncable::BOOKMARKS);
  model_types_with_payloads[syncable::AUTOFILL] = "test2";
  EXPECT_CALL(*syncer(), SyncShare(_,_,_))
      .WillOnce(DoAll(Invoke(sessions::test_util::SimulateSuccess),
          WithArg<0>(RecordSyncShare(&records2, 1U, &done))));
  syncer_thread()->ScheduleNudgeWithPayloads(zero(), NUDGE_SOURCE_LOCAL,
      model_types_with_payloads);
  done.TimedWait(timeout());

  EXPECT_EQ(1U, records2.snapshots.size());
  EXPECT_EQ(model_types_with_payloads, records2.snapshots[0]->source.types);
  EXPECT_EQ(GetUpdatesCallerInfo::LOCAL,
            records2.snapshots[0]->source.updates_source);
}

// Test that nudges are coalesced.
TEST_F(SyncerThread2Test, NudgeWithPayloadsCoalescing) {
  syncer_thread()->Start(SyncerThread::NORMAL_MODE);
  base::WaitableEvent done(false, false);
  SyncShareRecords r;
  EXPECT_CALL(*syncer(), SyncShare(_,_,_))
      .WillOnce(DoAll(Invoke(sessions::test_util::SimulateSuccess),
           WithArg<0>(RecordSyncShare(&r, 1U, &done))));
  sessions::TypePayloadMap types1, types2, types3;
  types1[syncable::BOOKMARKS] = "test1";
  types2[syncable::AUTOFILL] = "test2";
  types3[syncable::THEMES] = "test3";
  TimeDelta delay = TimeDelta::FromMilliseconds(20);
  TimeTicks optimal_time = TimeTicks::Now() + delay;
  syncer_thread()->ScheduleNudgeWithPayloads(delay, NUDGE_SOURCE_UNKNOWN,
      types1);
  syncer_thread()->ScheduleNudgeWithPayloads(zero(), NUDGE_SOURCE_LOCAL,
      types2);
  syncer_thread()->ScheduleNudgeWithPayloads(zero(), NUDGE_SOURCE_NOTIFICATION,
      types3);
  done.TimedWait(timeout());

  EXPECT_EQ(1U, r.snapshots.size());
  EXPECT_GE(r.times[0], optimal_time);
  sessions::TypePayloadMap coalesced_types;
  sessions::CoalescePayloads(&coalesced_types, types1);
  sessions::CoalescePayloads(&coalesced_types, types2);
  sessions::CoalescePayloads(&coalesced_types, types3);
  EXPECT_EQ(coalesced_types, r.snapshots[0]->source.types);
  EXPECT_EQ(GetUpdatesCallerInfo::NOTIFICATION,
            r.snapshots[0]->source.updates_source);

  SyncShareRecords r2;
  EXPECT_CALL(*syncer(), SyncShare(_,_,_))
      .WillOnce(DoAll(Invoke(sessions::test_util::SimulateSuccess),
           WithArg<0>(RecordSyncShare(&r2, 1U, &done))));
  syncer_thread()->ScheduleNudgeWithPayloads(zero(), NUDGE_SOURCE_NOTIFICATION,
      types3);
  done.TimedWait(timeout());
  EXPECT_EQ(1U, r2.snapshots.size());
  EXPECT_EQ(types3, r2.snapshots[0]->source.types);
  EXPECT_EQ(GetUpdatesCallerInfo::NOTIFICATION,
            r2.snapshots[0]->source.updates_source);
}

// Test that polling works as expected.
TEST_F(SyncerThread2Test, Polling) {
  SyncShareRecords records;
  base::WaitableEvent done(false, false);
  TimeDelta poll_interval(TimeDelta::FromMilliseconds(30));
  syncer_thread()->OnReceivedLongPollIntervalUpdate(poll_interval);
  EXPECT_CALL(*syncer(), SyncShare(_,_,_)).Times(AtLeast(kMinNumSamples))
      .WillRepeatedly(DoAll(Invoke(sessions::test_util::SimulateSuccess),
           WithArg<0>(RecordSyncShare(&records, kMinNumSamples, &done))));

  TimeTicks optimal_start = TimeTicks::Now() + poll_interval;
  syncer_thread()->Start(SyncerThread::NORMAL_MODE);
  done.TimedWait(timeout());
  syncer_thread()->Stop();

  AnalyzePollRun(records, kMinNumSamples, optimal_start, poll_interval);
}

// Test that the short poll interval is used.
TEST_F(SyncerThread2Test, PollNotificationsDisabled) {
  SyncShareRecords records;
  base::WaitableEvent done(false, false);
  TimeDelta poll_interval(TimeDelta::FromMilliseconds(30));
  syncer_thread()->OnReceivedShortPollIntervalUpdate(poll_interval);
  syncer_thread()->set_notifications_enabled(false);
  EXPECT_CALL(*syncer(), SyncShare(_,_,_)).Times(AtLeast(kMinNumSamples))
      .WillRepeatedly(DoAll(Invoke(sessions::test_util::SimulateSuccess),
           WithArg<0>(RecordSyncShare(&records, kMinNumSamples, &done))));

  TimeTicks optimal_start = TimeTicks::Now() + poll_interval;
  syncer_thread()->Start(SyncerThread::NORMAL_MODE);
  done.TimedWait(timeout());
  syncer_thread()->Stop();

  AnalyzePollRun(records, kMinNumSamples, optimal_start, poll_interval);
}

// Test that polling intervals are updated when needed.
TEST_F(SyncerThread2Test, PollIntervalUpdate) {
  SyncShareRecords records;
  base::WaitableEvent done(false, false);
  TimeDelta poll1(TimeDelta::FromMilliseconds(120));
  TimeDelta poll2(TimeDelta::FromMilliseconds(30));
  syncer_thread()->OnReceivedLongPollIntervalUpdate(poll1);
  EXPECT_CALL(*syncer(), SyncShare(_,_,_)).Times(AtLeast(kMinNumSamples))
      .WillOnce(WithArg<0>(
          sessions::test_util::SimulatePollIntervalUpdate(poll2)))
      .WillRepeatedly(DoAll(Invoke(sessions::test_util::SimulateSuccess),
           WithArg<0>(RecordSyncShare(&records, kMinNumSamples, &done))));

  TimeTicks optimal_start = TimeTicks::Now() + poll1 + poll2;
  syncer_thread()->Start(SyncerThread::NORMAL_MODE);
  done.TimedWait(timeout());
  syncer_thread()->Stop();

  AnalyzePollRun(records, kMinNumSamples, optimal_start, poll2);
}

// Test that a sync session is run through to completion.
TEST_F(SyncerThread2Test, HasMoreToSync) {
  syncer_thread()->Start(SyncerThread::NORMAL_MODE);
  base::WaitableEvent done(false, false);
  EXPECT_CALL(*syncer(), SyncShare(_,_,_))
      .WillOnce(Invoke(sessions::test_util::SimulateHasMoreToSync))
      .WillOnce(DoAll(Invoke(sessions::test_util::SimulateSuccess),
                      SignalEvent(&done)));
  syncer_thread()->ScheduleNudge(zero(), NUDGE_SOURCE_LOCAL, ModelTypeBitSet());
  done.TimedWait(timeout());
  // If more nudges are scheduled, they'll be waited on by TearDown, and would
  // cause our expectation to break.
}

// Test that no syncing occurs when throttled.
TEST_F(SyncerThread2Test, ThrottlingDoesThrottle) {
  syncable::ModelTypeBitSet types;
  types[syncable::BOOKMARKS] = true;
  base::WaitableEvent done(false, false);
  TimeDelta poll(TimeDelta::FromMilliseconds(5));
  TimeDelta throttle(TimeDelta::FromMinutes(10));
  syncer_thread()->OnReceivedLongPollIntervalUpdate(poll);
  EXPECT_CALL(*syncer(), SyncShare(_,_,_))
      .WillOnce(WithArg<0>(sessions::test_util::SimulateThrottled(throttle)));

  syncer_thread()->Start(SyncerThread::NORMAL_MODE);
  syncer_thread()->ScheduleNudge(zero(), NUDGE_SOURCE_LOCAL, types);
  FlushLastTask(&done);

  syncer_thread()->Start(SyncerThread::CONFIGURATION_MODE);
  syncer_thread()->ScheduleConfig(types);
  FlushLastTask(&done);
}

TEST_F(SyncerThread2Test, ThrottlingExpires) {
  SyncShareRecords records;
  base::WaitableEvent done(false, false);
  TimeDelta poll(TimeDelta::FromMilliseconds(15));
  TimeDelta throttle1(TimeDelta::FromMilliseconds(150));
  TimeDelta throttle2(TimeDelta::FromMinutes(10));
  syncer_thread()->OnReceivedLongPollIntervalUpdate(poll);

  ::testing::InSequence seq;
  EXPECT_CALL(*syncer(), SyncShare(_,_,_))
      .WillOnce(WithArg<0>(sessions::test_util::SimulateThrottled(throttle1)))
      .RetiresOnSaturation();
  EXPECT_CALL(*syncer(), SyncShare(_,_,_))
      .WillRepeatedly(DoAll(Invoke(sessions::test_util::SimulateSuccess),
           WithArg<0>(RecordSyncShare(&records, kMinNumSamples, &done))));

  TimeTicks optimal_start = TimeTicks::Now() + poll + throttle1;
  syncer_thread()->Start(SyncerThread::NORMAL_MODE);
  done.TimedWait(timeout());
  syncer_thread()->Stop();

  AnalyzePollRun(records, kMinNumSamples, optimal_start, poll);
}

// Test nudges / polls don't run in config mode and config tasks do.
TEST_F(SyncerThread2Test, ConfigurationMode) {
  TimeDelta poll(TimeDelta::FromMilliseconds(15));
  SyncShareRecords records;
  base::WaitableEvent done(false, false);
  base::WaitableEvent* dummy = NULL;
  syncer_thread()->OnReceivedLongPollIntervalUpdate(poll);
  EXPECT_CALL(*syncer(), SyncShare(_,_,_))
      .WillOnce(DoAll(Invoke(sessions::test_util::SimulateSuccess),
           WithArg<0>(RecordSyncShare(&records, 1U, dummy))));
  syncer_thread()->Start(SyncerThread::CONFIGURATION_MODE);
  syncable::ModelTypeBitSet nudge_types;
  nudge_types[syncable::AUTOFILL] = true;
  syncer_thread()->ScheduleNudge(zero(), NUDGE_SOURCE_LOCAL, nudge_types);
  syncer_thread()->ScheduleNudge(zero(), NUDGE_SOURCE_LOCAL, nudge_types);

  syncable::ModelTypeBitSet config_types;
  config_types[syncable::BOOKMARKS] = true;
  // TODO(tim): This will fail once CONFIGURATION tasks are implemented. Update
  // the EXPECT when that happens.
  syncer_thread()->ScheduleConfig(config_types);
  FlushLastTask(&done);
  syncer_thread()->Stop();

  EXPECT_EQ(1U, records.snapshots.size());
  EXPECT_TRUE(CompareModelTypeBitSetToTypePayloadMap(config_types,
      records.snapshots[0]->source.types));
}

// Test that exponential backoff is properly triggered.
TEST_F(SyncerThread2Test, BackoffTriggers) {
  base::WaitableEvent done(false, false);
  UseMockDelayProvider();

  EXPECT_CALL(*syncer(), SyncShare(_,_,_))
      .WillOnce(Invoke(sessions::test_util::SimulateDownloadUpdatesFailed))
      .WillOnce(DoAll(Invoke(sessions::test_util::SimulateSuccess),
          SignalEvent(&done)));
  EXPECT_FALSE(GetBackoffAndResetTest(&done));
  // Note GetBackoffAndResetTest clears mocks and re-instantiates the syncer.
  EXPECT_CALL(*syncer(), SyncShare(_,_,_))
      .WillOnce(Invoke(sessions::test_util::SimulateCommitFailed))
      .WillOnce(DoAll(Invoke(sessions::test_util::SimulateSuccess),
          SignalEvent(&done)));
  EXPECT_FALSE(GetBackoffAndResetTest(&done));
  EXPECT_CALL(*syncer(), SyncShare(_,_,_))
      .WillOnce(Invoke(sessions::test_util::SimulateDownloadUpdatesFailed))
      .WillRepeatedly(DoAll(Invoke(
          sessions::test_util::SimulateDownloadUpdatesFailed),
          SignalEvent(&done)));
  EXPECT_TRUE(GetBackoffAndResetTest(&done));
  EXPECT_CALL(*syncer(), SyncShare(_,_,_))
      .WillOnce(Invoke(sessions::test_util::SimulateCommitFailed))
      .WillRepeatedly(DoAll(Invoke(sessions::test_util::SimulateCommitFailed),
          SignalEvent(&done)));
  EXPECT_TRUE(GetBackoffAndResetTest(&done));
  EXPECT_CALL(*syncer(), SyncShare(_,_,_))
      .WillOnce(Invoke(sessions::test_util::SimulateDownloadUpdatesFailed))
      .WillOnce(Invoke(sessions::test_util::SimulateDownloadUpdatesFailed))
      .WillRepeatedly(DoAll(Invoke(sessions::test_util::SimulateSuccess),
          SignalEvent(&done)));
  EXPECT_FALSE(GetBackoffAndResetTest(&done));
  EXPECT_CALL(*syncer(), SyncShare(_,_,_))
      .WillOnce(Invoke(sessions::test_util::SimulateCommitFailed))
      .WillOnce(Invoke(sessions::test_util::SimulateCommitFailed))
      .WillRepeatedly(DoAll(Invoke(sessions::test_util::SimulateSuccess),
          SignalEvent(&done)));
  EXPECT_FALSE(GetBackoffAndResetTest(&done));
}

// Test that no polls or extraneous nudges occur when in backoff.
TEST_F(SyncerThread2Test, BackoffDropsJobs) {
  SyncShareRecords r;
  TimeDelta poll(TimeDelta::FromMilliseconds(5));
  base::WaitableEvent done(false, false);
  syncable::ModelTypeBitSet types;
  types[syncable::BOOKMARKS] = true;
  syncer_thread()->OnReceivedLongPollIntervalUpdate(poll);
  UseMockDelayProvider();

  EXPECT_CALL(*syncer(), SyncShare(_,_,_)).Times(2)
      .WillRepeatedly(DoAll(Invoke(sessions::test_util::SimulateCommitFailed),
          RecordSyncShareAndPostSignal(&r, 2U, this, &done)));
  EXPECT_CALL(*delay(), GetDelay(_))
      .WillRepeatedly(Return(TimeDelta::FromDays(1)));

  syncer_thread()->Start(SyncerThread::NORMAL_MODE);
  ASSERT_TRUE(done.TimedWait(timeout()));
  done.Reset();

  Mock::VerifyAndClearExpectations(syncer());
  EXPECT_EQ(2U, r.snapshots.size());
  EXPECT_EQ(GetUpdatesCallerInfo::PERIODIC,
            r.snapshots[0]->source.updates_source);
  EXPECT_EQ(GetUpdatesCallerInfo::SYNC_CYCLE_CONTINUATION,
            r.snapshots[1]->source.updates_source);

  EXPECT_CALL(*syncer(), SyncShare(_,_,_)).Times(1)
      .WillOnce(DoAll(Invoke(sessions::test_util::SimulateCommitFailed),
          RecordSyncShareAndPostSignal(&r, 1U, this, &done)));

  // We schedule a nudge with enough delay (10X poll interval) that at least
  // one or two polls would have taken place.  The nudge should succeed.
  syncer_thread()->ScheduleNudge(poll * 10, NUDGE_SOURCE_LOCAL, types);
  ASSERT_TRUE(done.TimedWait(timeout()));
  done.Reset();

  Mock::VerifyAndClearExpectations(syncer());
  Mock::VerifyAndClearExpectations(delay());
  EXPECT_EQ(3U, r.snapshots.size());
  EXPECT_EQ(GetUpdatesCallerInfo::LOCAL,
            r.snapshots[2]->source.updates_source);

  EXPECT_CALL(*syncer(), SyncShare(_,_,_)).Times(0);
  EXPECT_CALL(*delay(), GetDelay(_)).Times(0);

  syncer_thread()->Start(SyncerThread::CONFIGURATION_MODE);
  syncer_thread()->ScheduleConfig(types);
  FlushLastTask(&done);

  syncer_thread()->Start(SyncerThread::NORMAL_MODE);
  syncer_thread()->ScheduleNudge(zero(), NUDGE_SOURCE_LOCAL, types);
  syncer_thread()->ScheduleNudge(zero(), NUDGE_SOURCE_LOCAL, types);
  FlushLastTask(&done);
}

// Test that backoff is shaping traffic properly with consecutive errors.
TEST_F(SyncerThread2Test, BackoffElevation) {
  SyncShareRecords r;
  const TimeDelta poll(TimeDelta::FromMilliseconds(10));
  base::WaitableEvent done(false, false);
  syncer_thread()->OnReceivedLongPollIntervalUpdate(poll);
  UseMockDelayProvider();

  const TimeDelta first = TimeDelta::FromSeconds(1);
  const TimeDelta second = TimeDelta::FromMilliseconds(10);
  const TimeDelta third = TimeDelta::FromMilliseconds(20);
  const TimeDelta fourth = TimeDelta::FromMilliseconds(30);
  const TimeDelta fifth = TimeDelta::FromDays(1);

  EXPECT_CALL(*syncer(), SyncShare(_,_,_)).Times(kMinNumSamples)
      .WillRepeatedly(DoAll(Invoke(sessions::test_util::SimulateCommitFailed),
          RecordSyncShareAndPostSignal(&r, kMinNumSamples, this, &done)));

  EXPECT_CALL(*delay(), GetDelay(Eq(first))).WillOnce(Return(second))
      .RetiresOnSaturation();
  EXPECT_CALL(*delay(), GetDelay(Eq(second))).WillOnce(Return(third))
      .RetiresOnSaturation();
  EXPECT_CALL(*delay(), GetDelay(Eq(third))).WillOnce(Return(fourth))
      .RetiresOnSaturation();
  EXPECT_CALL(*delay(), GetDelay(Eq(fourth))).WillOnce(Return(fifth));

  syncer_thread()->Start(SyncerThread::NORMAL_MODE);
  ASSERT_TRUE(done.TimedWait(timeout()));

  EXPECT_GE(r.times[2] - r.times[1], second);
  EXPECT_GE(r.times[3] - r.times[2], third);
  EXPECT_GE(r.times[4] - r.times[3], fourth);
}

// Test that things go back to normal once a canary task makes forward progress
// following a succession of failures.
TEST_F(SyncerThread2Test, BackoffRelief) {
  SyncShareRecords r;
  const TimeDelta poll(TimeDelta::FromMilliseconds(10));
  base::WaitableEvent done(false, false);
  syncer_thread()->OnReceivedLongPollIntervalUpdate(poll);
  UseMockDelayProvider();

  const TimeDelta backoff = TimeDelta::FromMilliseconds(100);

  EXPECT_CALL(*syncer(), SyncShare(_,_,_))
      .WillOnce(Invoke(sessions::test_util::SimulateCommitFailed))
      .WillOnce(Invoke(sessions::test_util::SimulateCommitFailed))
      .WillRepeatedly(DoAll(Invoke(sessions::test_util::SimulateSuccess),
          RecordSyncShareAndPostSignal(&r, kMinNumSamples, this, &done)));
  EXPECT_CALL(*delay(), GetDelay(_)).WillOnce(Return(backoff));

  // Optimal start for the post-backoff poll party.
  TimeTicks optimal_start = TimeTicks::Now() + poll + backoff;
  syncer_thread()->Start(SyncerThread::NORMAL_MODE);
  done.TimedWait(timeout());
  syncer_thread()->Stop();

  // Check for healthy polling after backoff is relieved.
  // Can't use AnalyzePollRun because first sync is a continuation. Bleh.
  for (size_t i = 0; i < r.times.size(); i++) {
    SCOPED_TRACE(testing::Message() << "SyncShare # (" << i << ")");
    TimeTicks optimal_next_sync = optimal_start + poll * i;
    EXPECT_GE(r.times[i], optimal_next_sync);
    EXPECT_EQ(i == 0 ? GetUpdatesCallerInfo::SYNC_CYCLE_CONTINUATION
                     : GetUpdatesCallerInfo::PERIODIC,
              r.snapshots[i]->source.updates_source);
  }
}

TEST_F(SyncerThread2Test, GetRecommendedDelay) {
  EXPECT_LE(TimeDelta::FromSeconds(0),
            SyncerThread::GetRecommendedDelay(TimeDelta::FromSeconds(0)));
  EXPECT_LE(TimeDelta::FromSeconds(1),
            SyncerThread::GetRecommendedDelay(TimeDelta::FromSeconds(1)));
  EXPECT_LE(TimeDelta::FromSeconds(50),
            SyncerThread::GetRecommendedDelay(TimeDelta::FromSeconds(50)));
  EXPECT_LE(TimeDelta::FromSeconds(10),
            SyncerThread::GetRecommendedDelay(TimeDelta::FromSeconds(10)));
  EXPECT_EQ(TimeDelta::FromSeconds(kMaxBackoffSeconds),
            SyncerThread::GetRecommendedDelay(
                TimeDelta::FromSeconds(kMaxBackoffSeconds)));
  EXPECT_EQ(TimeDelta::FromSeconds(kMaxBackoffSeconds),
            SyncerThread::GetRecommendedDelay(
                TimeDelta::FromSeconds(kMaxBackoffSeconds + 1)));
}

// Test that appropriate syncer steps are requested for each job type.
TEST_F(SyncerThread2Test, SyncerSteps) {
  // Nudges.
  base::WaitableEvent done(false, false);
  EXPECT_CALL(*syncer(), SyncShare(_, SYNCER_BEGIN, SYNCER_END))
      .Times(1);
  syncer_thread()->Start(SyncerThread::NORMAL_MODE);
  syncer_thread()->ScheduleNudge(zero(), NUDGE_SOURCE_LOCAL, ModelTypeBitSet());
  FlushLastTask(&done);
  syncer_thread()->Stop();
  Mock::VerifyAndClearExpectations(syncer());

  // ClearUserData.
  EXPECT_CALL(*syncer(), SyncShare(_, CLEAR_PRIVATE_DATA, SYNCER_END))
      .Times(1);
  syncer_thread()->Start(SyncerThread::NORMAL_MODE);
  syncer_thread()->ScheduleClearUserData();
  FlushLastTask(&done);
  syncer_thread()->Stop();
  Mock::VerifyAndClearExpectations(syncer());

  // Configuration.
  EXPECT_CALL(*syncer(), SyncShare(_, DOWNLOAD_UPDATES, APPLY_UPDATES));
  syncer_thread()->Start(SyncerThread::CONFIGURATION_MODE);
  syncer_thread()->ScheduleConfig(ModelTypeBitSet());
  FlushLastTask(&done);
  syncer_thread()->Stop();
  Mock::VerifyAndClearExpectations(syncer());

  // Poll.
  EXPECT_CALL(*syncer(), SyncShare(_, SYNCER_BEGIN, SYNCER_END))
      .Times(AtLeast(1))
      .WillRepeatedly(SignalEvent(&done));
  const TimeDelta poll(TimeDelta::FromMilliseconds(10));
  syncer_thread()->OnReceivedLongPollIntervalUpdate(poll);
  syncer_thread()->Start(SyncerThread::NORMAL_MODE);
  done.TimedWait(timeout());
  syncer_thread()->Stop();
  Mock::VerifyAndClearExpectations(syncer());
  done.Reset();
}

// Test config tasks don't run during normal mode.
// TODO(tim): Implement this test and then the functionality!
TEST_F(SyncerThread2Test, DISABLED_NoConfigDuringNormal) {
}

// Test that starting the syncer thread without a valid connection doesn't
// break things when a connection is detected.
// Test config tasks don't run during normal mode.
// TODO(tim): Implement this test and then the functionality!
TEST_F(SyncerThread2Test, DISABLED_StartWhenNotConnected) {

}

}  // namespace s3
}  // namespace browser_sync

// SyncerThread won't outlive the test!
DISABLE_RUNNABLE_METHOD_REFCOUNT(browser_sync::s3::SyncerThread2Test);

