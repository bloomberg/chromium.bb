// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop.h"
#include "base/test/test_timeouts.h"
#include "chrome/browser/sync/engine/sync_scheduler.h"
#include "chrome/browser/sync/engine/syncer.h"
#include "chrome/browser/sync/sessions/test_util.h"
#include "chrome/browser/sync/test/engine/fake_model_safe_worker_registrar.h"
#include "chrome/browser/sync/test/engine/mock_connection_manager.h"
#include "chrome/browser/sync/test/engine/test_directory_setter_upper.h"
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

// Used when tests want to record syncing activity to examine later.
struct SyncShareRecords {
  std::vector<TimeTicks> times;
  std::vector<linked_ptr<SyncSessionSnapshot> > snapshots;
};

void QuitLoopNow() {
  // We use QuitNow() instead of Quit() as the latter may get stalled
  // indefinitely in the presence of repeated timers with low delays
  // and a slow test (e.g., ThrottlingDoesThrottle [which has a poll
  // delay of 5ms] run under TSAN on the trybots).
  MessageLoop::current()->QuitNow();
}

void RunLoop() {
  MessageLoop::current()->Run();
}

void PumpLoop() {
  // Do it this way instead of RunAllPending to pump loop exactly once
  // (necessary in the presence of timers; see comment in
  // QuitLoopNow).
  MessageLoop::current()->PostTask(FROM_HERE, base::Bind(&QuitLoopNow));
  RunLoop();
}

// Convenient to use in tests wishing to analyze SyncShare calls over time.
static const size_t kMinNumSamples = 5;
class SyncSchedulerTest : public testing::Test {
 public:
  SyncSchedulerTest()
      : weak_ptr_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)),
        context_(NULL),
        syncer_(NULL),
        delay_(NULL) {}

  class MockDelayProvider : public SyncScheduler::DelayProvider {
   public:
    MOCK_METHOD1(GetDelay, TimeDelta(const TimeDelta&));
  };

  virtual void SetUp() {
    syncdb_.SetUp();
    syncer_ = new MockSyncer();
    delay_ = NULL;
    ModelSafeRoutingInfo routing_info;
    routing_info[syncable::BOOKMARKS] = GROUP_UI;
    routing_info[syncable::AUTOFILL] = GROUP_DB;
    routing_info[syncable::THEMES] = GROUP_UI;
    routing_info[syncable::NIGORI] = GROUP_PASSIVE;
    registrar_.reset(new FakeModelSafeWorkerRegistrar(routing_info));
    connection_.reset(new MockConnectionManager(syncdb_.manager(), "Test"));
    connection_->SetServerReachable();
    context_ = new SyncSessionContext(connection_.get(), syncdb_.manager(),
        registrar_.get(), std::vector<SyncEngineEventListener*>(), NULL);
    context_->set_notifications_enabled(true);
    context_->set_account_name("Test");
    scheduler_.reset(
        new SyncScheduler("TestSyncScheduler", context_, syncer_));
  }

  SyncScheduler* scheduler() { return scheduler_.get(); }
  MockSyncer* syncer() { return syncer_; }
  MockDelayProvider* delay() { return delay_; }
  MockConnectionManager* connection() { return connection_.get(); }
  TimeDelta zero() { return TimeDelta::FromSeconds(0); }
  TimeDelta timeout() {
    return TimeDelta::FromMilliseconds(TestTimeouts::action_timeout_ms());
  }

  virtual void TearDown() {
    PumpLoop();
    scheduler_.reset();
    PumpLoop();
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

  void DoQuitLoopNow() {
    QuitLoopNow();
  }

  void StartSyncScheduler(SyncScheduler::Mode mode) {
    scheduler()->Start(
        mode,
        base::Bind(&SyncSchedulerTest::DoQuitLoopNow,
                   weak_ptr_factory_.GetWeakPtr()));
  }

  // This stops the scheduler synchronously.
  void StopSyncScheduler() {
    scheduler()->RequestStop(base::Bind(&SyncSchedulerTest::DoQuitLoopNow,
                             weak_ptr_factory_.GetWeakPtr()));
    RunLoop();
  }

  bool GetBackoffAndResetTest() {
    syncable::ModelTypeBitSet nudge_types;
    StartSyncScheduler(SyncScheduler::NORMAL_MODE);
    RunLoop();

    scheduler()->ScheduleNudge(
        zero(), NUDGE_SOURCE_LOCAL, nudge_types, FROM_HERE);
    RunLoop();

    bool backing_off = scheduler()->IsBackingOff();
    StopSyncScheduler();

    syncdb_.TearDown();

    Mock::VerifyAndClearExpectations(syncer());

    TearDown();
    SetUp();
    UseMockDelayProvider();
    EXPECT_CALL(*delay(), GetDelay(_))
        .WillRepeatedly(Return(TimeDelta::FromMilliseconds(1)));
    return backing_off;
  }

  void UseMockDelayProvider() {
    delay_ = new MockDelayProvider();
    scheduler_->delay_provider_.reset(delay_);
  }

  // Compare a ModelTypeBitSet to a ModelTypePayloadMap, ignoring
  // payload values.
  bool CompareModelTypeBitSetToModelTypePayloadMap(
      const syncable::ModelTypeBitSet& lhs,
      const syncable::ModelTypePayloadMap& rhs) {
    size_t count = 0;
    for (syncable::ModelTypePayloadMap::const_iterator i = rhs.begin();
         i != rhs.end(); ++i, ++count) {
      if (!lhs.test(i->first))
        return false;
    }
    if (lhs.count() != count)
      return false;
    return true;
  }

  SyncSessionContext* context() { return context_; }

 private:
  base::WeakPtrFactory<SyncSchedulerTest> weak_ptr_factory_;
  MessageLoop message_loop_;
  scoped_ptr<SyncScheduler> scheduler_;
  scoped_ptr<MockConnectionManager> connection_;
  SyncSessionContext* context_;
  MockSyncer* syncer_;
  MockDelayProvider* delay_;
  scoped_ptr<FakeModelSafeWorkerRegistrar> registrar_;
  MockDirectorySetterUpper syncdb_;
};

void RecordSyncShareImpl(SyncSession* s, SyncShareRecords* record) {
  record->times.push_back(TimeTicks::Now());
  record->snapshots.push_back(make_linked_ptr(new SyncSessionSnapshot(
      s->TakeSnapshot())));
}

ACTION_P(RecordSyncShare, record) {
  RecordSyncShareImpl(arg0, record);
  QuitLoopNow();
}

ACTION_P2(RecordSyncShareMultiple, record, quit_after) {
  RecordSyncShareImpl(arg0, record);
  EXPECT_LE(record->times.size(), quit_after);
  if (record->times.size() >= quit_after) {
    QuitLoopNow();
  }
}

ACTION(AddFailureAndQuitLoopNow) {
  ADD_FAILURE();
  QuitLoopNow();
}

ACTION(QuitLoopNowAction) {
  QuitLoopNow();
}

// Test nudge scheduling.
TEST_F(SyncSchedulerTest, Nudge) {
  SyncShareRecords records;
  syncable::ModelTypeBitSet model_types;
  model_types[syncable::BOOKMARKS] = true;

  EXPECT_CALL(*syncer(), SyncShare(_,_,_))
      .WillOnce(DoAll(Invoke(sessions::test_util::SimulateSuccess),
                      WithArg<0>(RecordSyncShare(&records))))
      .RetiresOnSaturation();

  StartSyncScheduler(SyncScheduler::NORMAL_MODE);
  RunLoop();

  scheduler()->ScheduleNudge(
      zero(), NUDGE_SOURCE_LOCAL, model_types, FROM_HERE);
  RunLoop();

  ASSERT_EQ(1U, records.snapshots.size());
  EXPECT_TRUE(CompareModelTypeBitSetToModelTypePayloadMap(model_types,
      records.snapshots[0]->source.types));
  EXPECT_EQ(GetUpdatesCallerInfo::LOCAL,
            records.snapshots[0]->source.updates_source);

  Mock::VerifyAndClearExpectations(syncer());

  // Make sure a second, later, nudge is unaffected by first (no coalescing).
  SyncShareRecords records2;
  model_types[syncable::BOOKMARKS] = false;
  model_types[syncable::AUTOFILL] = true;
  EXPECT_CALL(*syncer(), SyncShare(_,_,_))
      .WillOnce(DoAll(Invoke(sessions::test_util::SimulateSuccess),
                      WithArg<0>(RecordSyncShare(&records2))));
  scheduler()->ScheduleNudge(
      zero(), NUDGE_SOURCE_LOCAL, model_types, FROM_HERE);
  RunLoop();

  ASSERT_EQ(1U, records2.snapshots.size());
  EXPECT_TRUE(CompareModelTypeBitSetToModelTypePayloadMap(model_types,
      records2.snapshots[0]->source.types));
  EXPECT_EQ(GetUpdatesCallerInfo::LOCAL,
            records2.snapshots[0]->source.updates_source);
}

// Make sure a regular config command is scheduled fine in the absence of any
// errors.
TEST_F(SyncSchedulerTest, Config) {
  SyncShareRecords records;
  syncable::ModelTypeBitSet model_types;
  model_types[syncable::BOOKMARKS] = true;

  EXPECT_CALL(*syncer(), SyncShare(_,_,_))
      .WillOnce(DoAll(Invoke(sessions::test_util::SimulateSuccess),
                      WithArg<0>(RecordSyncShare(&records))));

  StartSyncScheduler(SyncScheduler::CONFIGURATION_MODE);
  RunLoop();

  scheduler()->ScheduleConfig(
      model_types, GetUpdatesCallerInfo::RECONFIGURATION);
  RunLoop();

  ASSERT_EQ(1U, records.snapshots.size());
  EXPECT_TRUE(CompareModelTypeBitSetToModelTypePayloadMap(model_types,
      records.snapshots[0]->source.types));
  EXPECT_EQ(GetUpdatesCallerInfo::RECONFIGURATION,
            records.snapshots[0]->source.updates_source);
}

// Simulate a failure and make sure the config request is retried.
TEST_F(SyncSchedulerTest, ConfigWithBackingOff) {
  UseMockDelayProvider();
  EXPECT_CALL(*delay(), GetDelay(_))
      .WillRepeatedly(Return(TimeDelta::FromMilliseconds(1)));
  SyncShareRecords records;
  syncable::ModelTypeBitSet model_types;
  model_types[syncable::BOOKMARKS] = true;

  EXPECT_CALL(*syncer(), SyncShare(_,_,_))
      .WillOnce(DoAll(Invoke(sessions::test_util::SimulateCommitFailed),
                      WithArg<0>(RecordSyncShare(&records))))
      .WillOnce(DoAll(Invoke(sessions::test_util::SimulateSuccess),
                      WithArg<0>(RecordSyncShare(&records))));

  StartSyncScheduler(SyncScheduler::CONFIGURATION_MODE);
  RunLoop();

  ASSERT_EQ(0U, records.snapshots.size());
  scheduler()->ScheduleConfig(
      model_types, GetUpdatesCallerInfo::RECONFIGURATION);
  RunLoop();

  ASSERT_EQ(1U, records.snapshots.size());
  RunLoop();

  ASSERT_EQ(2U, records.snapshots.size());
  EXPECT_TRUE(CompareModelTypeBitSetToModelTypePayloadMap(model_types,
      records.snapshots[1]->source.types));
  EXPECT_EQ(GetUpdatesCallerInfo::SYNC_CYCLE_CONTINUATION,
            records.snapshots[1]->source.updates_source);
}

// Issue 2 config commands. Second one right after the first has failed
// and make sure LATEST is executed.
TEST_F(SyncSchedulerTest, MultipleConfigWithBackingOff) {
  syncable::ModelTypeBitSet model_types1, model_types2;
  model_types1[syncable::BOOKMARKS] = true;
  model_types2[syncable::AUTOFILL] = true;
  UseMockDelayProvider();
  EXPECT_CALL(*delay(), GetDelay(_))
      .WillRepeatedly(Return(TimeDelta::FromMilliseconds(30)));
  SyncShareRecords records;

  EXPECT_CALL(*syncer(), SyncShare(_,_,_))
      .WillOnce(DoAll(Invoke(sessions::test_util::SimulateCommitFailed),
                      WithArg<0>(RecordSyncShare(&records))))
      .WillOnce(DoAll(Invoke(sessions::test_util::SimulateCommitFailed),
                      WithArg<0>(RecordSyncShare(&records))))
      .WillOnce(DoAll(Invoke(sessions::test_util::SimulateSuccess),
                      WithArg<0>(RecordSyncShare(&records))));

  StartSyncScheduler(SyncScheduler::CONFIGURATION_MODE);
  RunLoop();

  ASSERT_EQ(0U, records.snapshots.size());
  scheduler()->ScheduleConfig(
      model_types1, GetUpdatesCallerInfo::RECONFIGURATION);
  RunLoop();

  ASSERT_EQ(1U, records.snapshots.size());
  scheduler()->ScheduleConfig(
      model_types2, GetUpdatesCallerInfo::RECONFIGURATION);
  RunLoop();

  ASSERT_EQ(2U, records.snapshots.size());
  RunLoop();

  ASSERT_EQ(3U, records.snapshots.size());
  EXPECT_TRUE(CompareModelTypeBitSetToModelTypePayloadMap(model_types2,
      records.snapshots[2]->source.types));
  EXPECT_EQ(GetUpdatesCallerInfo::RECONFIGURATION,
            records.snapshots[2]->source.updates_source);
}

// Issue a nudge when the config has failed. Make sure both the config and
// nudge are executed.
TEST_F(SyncSchedulerTest, NudgeWithConfigWithBackingOff) {
  syncable::ModelTypeBitSet model_types;
  model_types[syncable::BOOKMARKS] = true;
  UseMockDelayProvider();
  EXPECT_CALL(*delay(), GetDelay(_))
      .WillRepeatedly(Return(TimeDelta::FromMilliseconds(50)));
  SyncShareRecords records;

  EXPECT_CALL(*syncer(), SyncShare(_,_,_))
      .WillOnce(DoAll(Invoke(sessions::test_util::SimulateCommitFailed),
                      WithArg<0>(RecordSyncShare(&records))))
      .WillOnce(DoAll(Invoke(sessions::test_util::SimulateCommitFailed),
                      WithArg<0>(RecordSyncShare(&records))))
      .WillOnce(DoAll(Invoke(sessions::test_util::SimulateSuccess),
                      WithArg<0>(RecordSyncShare(&records))))
      .WillOnce(DoAll(Invoke(sessions::test_util::SimulateSuccess),
                      WithArg<0>(RecordSyncShare(&records))));

  StartSyncScheduler(SyncScheduler::CONFIGURATION_MODE);
  RunLoop();

  ASSERT_EQ(0U, records.snapshots.size());
  scheduler()->ScheduleConfig(
      model_types, GetUpdatesCallerInfo::RECONFIGURATION);
  RunLoop();

  ASSERT_EQ(1U, records.snapshots.size());
  scheduler()->ScheduleNudge(
      zero(), NUDGE_SOURCE_LOCAL, model_types, FROM_HERE);
  RunLoop();

  ASSERT_EQ(2U, records.snapshots.size());
  RunLoop();

  // Now change the mode so nudge can execute.
  ASSERT_EQ(3U, records.snapshots.size());
  StartSyncScheduler(SyncScheduler::NORMAL_MODE);
  RunLoop();

  ASSERT_EQ(4U, records.snapshots.size());

  EXPECT_TRUE(CompareModelTypeBitSetToModelTypePayloadMap(model_types,
      records.snapshots[2]->source.types));
  EXPECT_EQ(GetUpdatesCallerInfo::SYNC_CYCLE_CONTINUATION,
            records.snapshots[2]->source.updates_source);

  EXPECT_TRUE(CompareModelTypeBitSetToModelTypePayloadMap(model_types,
      records.snapshots[3]->source.types));
  EXPECT_EQ(GetUpdatesCallerInfo::LOCAL,
            records.snapshots[3]->source.updates_source);

}

// Test that nudges are coalesced.
TEST_F(SyncSchedulerTest, NudgeCoalescing) {
  StartSyncScheduler(SyncScheduler::NORMAL_MODE);
  RunLoop();

  SyncShareRecords r;
  EXPECT_CALL(*syncer(), SyncShare(_,_,_))
      .WillOnce(DoAll(Invoke(sessions::test_util::SimulateSuccess),
                      WithArg<0>(RecordSyncShare(&r))));
  syncable::ModelTypeBitSet types1, types2, types3;
  types1[syncable::BOOKMARKS] = true;
  types2[syncable::AUTOFILL] = true;
  types3[syncable::THEMES] = true;
  TimeDelta delay = zero();
  TimeTicks optimal_time = TimeTicks::Now() + delay;
  scheduler()->ScheduleNudge(
      delay, NUDGE_SOURCE_UNKNOWN, types1, FROM_HERE);
  scheduler()->ScheduleNudge(
      zero(), NUDGE_SOURCE_LOCAL, types2, FROM_HERE);
  RunLoop();

  ASSERT_EQ(1U, r.snapshots.size());
  EXPECT_GE(r.times[0], optimal_time);
  EXPECT_TRUE(CompareModelTypeBitSetToModelTypePayloadMap(
      types1 | types2, r.snapshots[0]->source.types));
  EXPECT_EQ(GetUpdatesCallerInfo::LOCAL,
            r.snapshots[0]->source.updates_source);

  Mock::VerifyAndClearExpectations(syncer());

  SyncShareRecords r2;
  EXPECT_CALL(*syncer(), SyncShare(_,_,_))
      .WillOnce(DoAll(Invoke(sessions::test_util::SimulateSuccess),
                      WithArg<0>(RecordSyncShare(&r2))));
  scheduler()->ScheduleNudge(
      zero(), NUDGE_SOURCE_NOTIFICATION, types3, FROM_HERE);
  RunLoop();

  ASSERT_EQ(1U, r2.snapshots.size());
  EXPECT_TRUE(CompareModelTypeBitSetToModelTypePayloadMap(types3,
      r2.snapshots[0]->source.types));
  EXPECT_EQ(GetUpdatesCallerInfo::NOTIFICATION,
            r2.snapshots[0]->source.updates_source);
}

// Test nudge scheduling.
TEST_F(SyncSchedulerTest, NudgeWithPayloads) {
  StartSyncScheduler(SyncScheduler::NORMAL_MODE);
  RunLoop();

  SyncShareRecords records;
  syncable::ModelTypePayloadMap model_types_with_payloads;
  model_types_with_payloads[syncable::BOOKMARKS] = "test";

  EXPECT_CALL(*syncer(), SyncShare(_,_,_))
      .WillOnce(DoAll(Invoke(sessions::test_util::SimulateSuccess),
                      WithArg<0>(RecordSyncShare(&records))))
      .RetiresOnSaturation();
  scheduler()->ScheduleNudgeWithPayloads(
      zero(), NUDGE_SOURCE_LOCAL, model_types_with_payloads, FROM_HERE);
  RunLoop();

  ASSERT_EQ(1U, records.snapshots.size());
  EXPECT_EQ(model_types_with_payloads, records.snapshots[0]->source.types);
  EXPECT_EQ(GetUpdatesCallerInfo::LOCAL,
            records.snapshots[0]->source.updates_source);

  Mock::VerifyAndClearExpectations(syncer());

  // Make sure a second, later, nudge is unaffected by first (no coalescing).
  SyncShareRecords records2;
  model_types_with_payloads.erase(syncable::BOOKMARKS);
  model_types_with_payloads[syncable::AUTOFILL] = "test2";
  EXPECT_CALL(*syncer(), SyncShare(_,_,_))
      .WillOnce(DoAll(Invoke(sessions::test_util::SimulateSuccess),
                      WithArg<0>(RecordSyncShare(&records2))));
  scheduler()->ScheduleNudgeWithPayloads(
      zero(), NUDGE_SOURCE_LOCAL, model_types_with_payloads, FROM_HERE);
  RunLoop();

  ASSERT_EQ(1U, records2.snapshots.size());
  EXPECT_EQ(model_types_with_payloads, records2.snapshots[0]->source.types);
  EXPECT_EQ(GetUpdatesCallerInfo::LOCAL,
            records2.snapshots[0]->source.updates_source);
}

// Test that nudges are coalesced.
TEST_F(SyncSchedulerTest, NudgeWithPayloadsCoalescing) {
  StartSyncScheduler(SyncScheduler::NORMAL_MODE);
  RunLoop();

  SyncShareRecords r;
  EXPECT_CALL(*syncer(), SyncShare(_,_,_))
      .WillOnce(DoAll(Invoke(sessions::test_util::SimulateSuccess),
                      WithArg<0>(RecordSyncShare(&r))));
  syncable::ModelTypePayloadMap types1, types2, types3;
  types1[syncable::BOOKMARKS] = "test1";
  types2[syncable::AUTOFILL] = "test2";
  types3[syncable::THEMES] = "test3";
  TimeDelta delay = zero();
  TimeTicks optimal_time = TimeTicks::Now() + delay;
  scheduler()->ScheduleNudgeWithPayloads(
      delay, NUDGE_SOURCE_UNKNOWN, types1, FROM_HERE);
  scheduler()->ScheduleNudgeWithPayloads(
      zero(), NUDGE_SOURCE_LOCAL, types2, FROM_HERE);
  RunLoop();

  ASSERT_EQ(1U, r.snapshots.size());
  EXPECT_GE(r.times[0], optimal_time);
  syncable::ModelTypePayloadMap coalesced_types;
  syncable::CoalescePayloads(&coalesced_types, types1);
  syncable::CoalescePayloads(&coalesced_types, types2);
  EXPECT_EQ(coalesced_types, r.snapshots[0]->source.types);
  EXPECT_EQ(GetUpdatesCallerInfo::LOCAL,
            r.snapshots[0]->source.updates_source);

  Mock::VerifyAndClearExpectations(syncer());

  SyncShareRecords r2;
  EXPECT_CALL(*syncer(), SyncShare(_,_,_))
      .WillOnce(DoAll(Invoke(sessions::test_util::SimulateSuccess),
                      WithArg<0>(RecordSyncShare(&r2))));
  scheduler()->ScheduleNudgeWithPayloads(
      zero(), NUDGE_SOURCE_NOTIFICATION, types3, FROM_HERE);
  RunLoop();

  ASSERT_EQ(1U, r2.snapshots.size());
  EXPECT_EQ(types3, r2.snapshots[0]->source.types);
  EXPECT_EQ(GetUpdatesCallerInfo::NOTIFICATION,
            r2.snapshots[0]->source.updates_source);
}

// Test that polling works as expected.
TEST_F(SyncSchedulerTest, Polling) {
  SyncShareRecords records;
  TimeDelta poll_interval(TimeDelta::FromMilliseconds(30));
  EXPECT_CALL(*syncer(), SyncShare(_,_,_)).Times(AtLeast(kMinNumSamples))
      .WillRepeatedly(DoAll(Invoke(sessions::test_util::SimulateSuccess),
           WithArg<0>(RecordSyncShareMultiple(&records, kMinNumSamples))));

  scheduler()->OnReceivedLongPollIntervalUpdate(poll_interval);

  TimeTicks optimal_start = TimeTicks::Now() + poll_interval;
  StartSyncScheduler(SyncScheduler::NORMAL_MODE);
  RunLoop();

  // Run again to wait for polling.
  RunLoop();

  StopSyncScheduler();
  AnalyzePollRun(records, kMinNumSamples, optimal_start, poll_interval);
}

// Test that the short poll interval is used.
TEST_F(SyncSchedulerTest, PollNotificationsDisabled) {
  SyncShareRecords records;
  TimeDelta poll_interval(TimeDelta::FromMilliseconds(30));
  EXPECT_CALL(*syncer(), SyncShare(_,_,_)).Times(AtLeast(kMinNumSamples))
      .WillRepeatedly(DoAll(Invoke(sessions::test_util::SimulateSuccess),
           WithArg<0>(RecordSyncShareMultiple(&records, kMinNumSamples))));

  scheduler()->OnReceivedShortPollIntervalUpdate(poll_interval);
  scheduler()->set_notifications_enabled(false);

  TimeTicks optimal_start = TimeTicks::Now() + poll_interval;
  StartSyncScheduler(SyncScheduler::NORMAL_MODE);
  RunLoop();

  // Run again to wait for polling.
  RunLoop();

  StopSyncScheduler();
  AnalyzePollRun(records, kMinNumSamples, optimal_start, poll_interval);
}

// Test that polling intervals are updated when needed.
TEST_F(SyncSchedulerTest, PollIntervalUpdate) {
  SyncShareRecords records;
  TimeDelta poll1(TimeDelta::FromMilliseconds(120));
  TimeDelta poll2(TimeDelta::FromMilliseconds(30));
  scheduler()->OnReceivedLongPollIntervalUpdate(poll1);
  EXPECT_CALL(*syncer(), SyncShare(_,_,_)).Times(AtLeast(kMinNumSamples))
      .WillOnce(WithArg<0>(
          sessions::test_util::SimulatePollIntervalUpdate(poll2)))
      .WillRepeatedly(
          DoAll(Invoke(sessions::test_util::SimulateSuccess),
                WithArg<0>(
                    RecordSyncShareMultiple(&records, kMinNumSamples))));

  TimeTicks optimal_start = TimeTicks::Now() + poll1 + poll2;
  StartSyncScheduler(SyncScheduler::NORMAL_MODE);
  RunLoop();

  // Run again to wait for polling.
  RunLoop();

  StopSyncScheduler();
  AnalyzePollRun(records, kMinNumSamples, optimal_start, poll2);
}

// Test that the sessions commit delay is updated when needed.
TEST_F(SyncSchedulerTest, SessionsCommitDelay) {
  SyncShareRecords records;
  TimeDelta delay1(TimeDelta::FromMilliseconds(120));
  TimeDelta delay2(TimeDelta::FromMilliseconds(30));
  scheduler()->OnReceivedSessionsCommitDelay(delay1);

  EXPECT_CALL(*syncer(), SyncShare(_,_,_))
      .WillOnce(
          DoAll(
              WithArg<0>(
                  sessions::test_util::SimulateSessionsCommitDelayUpdate(
                      delay2)),
              Invoke(sessions::test_util::SimulateSuccess),
              QuitLoopNowAction()));

  EXPECT_EQ(delay1, scheduler()->sessions_commit_delay());
  StartSyncScheduler(SyncScheduler::NORMAL_MODE);
  RunLoop();

  EXPECT_EQ(delay1, scheduler()->sessions_commit_delay());
  syncable::ModelTypeBitSet model_types;
  model_types[syncable::BOOKMARKS] = true;
  scheduler()->ScheduleNudge(
      zero(), NUDGE_SOURCE_LOCAL, model_types, FROM_HERE);
  RunLoop();

  EXPECT_EQ(delay2, scheduler()->sessions_commit_delay());
  StopSyncScheduler();
}

// Test that a sync session is run through to completion.
TEST_F(SyncSchedulerTest, HasMoreToSync) {
  EXPECT_CALL(*syncer(), SyncShare(_,_,_))
      .WillOnce(Invoke(sessions::test_util::SimulateHasMoreToSync))
      .WillOnce(DoAll(Invoke(sessions::test_util::SimulateSuccess),
                      QuitLoopNowAction()));
  StartSyncScheduler(SyncScheduler::NORMAL_MODE);
  RunLoop();

  scheduler()->ScheduleNudge(
      zero(), NUDGE_SOURCE_LOCAL, ModelTypeBitSet(), FROM_HERE);
  RunLoop();
  // If more nudges are scheduled, they'll be waited on by TearDown, and would
  // cause our expectation to break.
}

// Test that no syncing occurs when throttled.
TEST_F(SyncSchedulerTest, ThrottlingDoesThrottle) {
  syncable::ModelTypeBitSet types;
  types[syncable::BOOKMARKS] = true;
  TimeDelta poll(TimeDelta::FromMilliseconds(5));
  TimeDelta throttle(TimeDelta::FromMinutes(10));
  scheduler()->OnReceivedLongPollIntervalUpdate(poll);
  EXPECT_CALL(*syncer(), SyncShare(_,_,_))
      .WillOnce(WithArg<0>(sessions::test_util::SimulateThrottled(throttle)))
      .WillRepeatedly(AddFailureAndQuitLoopNow());

  StartSyncScheduler(SyncScheduler::NORMAL_MODE);
  RunLoop();

  scheduler()->ScheduleNudge(
      zero(), NUDGE_SOURCE_LOCAL, types, FROM_HERE);
  PumpLoop();

  StartSyncScheduler(SyncScheduler::CONFIGURATION_MODE);
  RunLoop();

  scheduler()->ScheduleConfig(
      types, GetUpdatesCallerInfo::RECONFIGURATION);
  PumpLoop();
}

TEST_F(SyncSchedulerTest, ThrottlingExpires) {
  SyncShareRecords records;
  TimeDelta poll(TimeDelta::FromMilliseconds(15));
  TimeDelta throttle1(TimeDelta::FromMilliseconds(150));
  TimeDelta throttle2(TimeDelta::FromMinutes(10));
  scheduler()->OnReceivedLongPollIntervalUpdate(poll);

  ::testing::InSequence seq;
  EXPECT_CALL(*syncer(), SyncShare(_,_,_))
      .WillOnce(WithArg<0>(sessions::test_util::SimulateThrottled(throttle1)))
      .RetiresOnSaturation();
  EXPECT_CALL(*syncer(), SyncShare(_,_,_))
      .WillRepeatedly(DoAll(Invoke(sessions::test_util::SimulateSuccess),
           WithArg<0>(RecordSyncShareMultiple(&records, kMinNumSamples))));

  TimeTicks optimal_start = TimeTicks::Now() + poll + throttle1;
  StartSyncScheduler(SyncScheduler::NORMAL_MODE);
  RunLoop();

  // Run again to wait for polling.
  RunLoop();

  StopSyncScheduler();
  AnalyzePollRun(records, kMinNumSamples, optimal_start, poll);
}

// Test nudges / polls don't run in config mode and config tasks do.
TEST_F(SyncSchedulerTest, ConfigurationMode) {
  TimeDelta poll(TimeDelta::FromMilliseconds(15));
  SyncShareRecords records;
  scheduler()->OnReceivedLongPollIntervalUpdate(poll);
  EXPECT_CALL(*syncer(), SyncShare(_,_,_))
      .WillOnce((Invoke(sessions::test_util::SimulateSuccess),
           WithArg<0>(RecordSyncShare(&records))));

  StartSyncScheduler(SyncScheduler::CONFIGURATION_MODE);
  RunLoop();

  syncable::ModelTypeBitSet nudge_types;
  nudge_types[syncable::AUTOFILL] = true;
  scheduler()->ScheduleNudge(
      zero(), NUDGE_SOURCE_LOCAL, nudge_types, FROM_HERE);
  scheduler()->ScheduleNudge(
      zero(), NUDGE_SOURCE_LOCAL, nudge_types, FROM_HERE);

  syncable::ModelTypeBitSet config_types;
  config_types[syncable::BOOKMARKS] = true;

  scheduler()->ScheduleConfig(
      config_types, GetUpdatesCallerInfo::RECONFIGURATION);
  RunLoop();

  ASSERT_EQ(1U, records.snapshots.size());
  EXPECT_TRUE(CompareModelTypeBitSetToModelTypePayloadMap(config_types,
      records.snapshots[0]->source.types));
}

// Test that exponential backoff is properly triggered.
TEST_F(SyncSchedulerTest, BackoffTriggers) {
  UseMockDelayProvider();

  EXPECT_CALL(*syncer(), SyncShare(_,_,_))
      .WillOnce(Invoke(sessions::test_util::SimulateDownloadUpdatesFailed))
      .WillOnce(DoAll(Invoke(sessions::test_util::SimulateSuccess),
                      QuitLoopNowAction()));
  EXPECT_FALSE(GetBackoffAndResetTest());
  // Note GetBackoffAndResetTest clears mocks and re-instantiates the syncer.
  EXPECT_CALL(*syncer(), SyncShare(_,_,_))
      .WillOnce(Invoke(sessions::test_util::SimulateCommitFailed))
      .WillOnce(DoAll(Invoke(sessions::test_util::SimulateSuccess),
                      QuitLoopNowAction()));
  EXPECT_FALSE(GetBackoffAndResetTest());
  EXPECT_CALL(*syncer(), SyncShare(_,_,_))
      .WillOnce(Invoke(sessions::test_util::SimulateDownloadUpdatesFailed))
      .WillRepeatedly(DoAll(Invoke(
          sessions::test_util::SimulateDownloadUpdatesFailed),
                            QuitLoopNowAction()));
  EXPECT_TRUE(GetBackoffAndResetTest());
  EXPECT_CALL(*syncer(), SyncShare(_,_,_))
      .WillOnce(Invoke(sessions::test_util::SimulateCommitFailed))
      .WillRepeatedly(DoAll(Invoke(sessions::test_util::SimulateCommitFailed),
                            QuitLoopNowAction()));
  EXPECT_TRUE(GetBackoffAndResetTest());
  EXPECT_CALL(*syncer(), SyncShare(_,_,_))
      .WillOnce(Invoke(sessions::test_util::SimulateDownloadUpdatesFailed))
      .WillOnce(Invoke(sessions::test_util::SimulateDownloadUpdatesFailed))
      .WillRepeatedly(DoAll(Invoke(sessions::test_util::SimulateSuccess),
                            QuitLoopNowAction()));
  EXPECT_FALSE(GetBackoffAndResetTest());
  EXPECT_CALL(*syncer(), SyncShare(_,_,_))
      .WillOnce(Invoke(sessions::test_util::SimulateCommitFailed))
      .WillOnce(Invoke(sessions::test_util::SimulateCommitFailed))
      .WillRepeatedly(DoAll(Invoke(sessions::test_util::SimulateSuccess),
                            QuitLoopNowAction()));
  EXPECT_FALSE(GetBackoffAndResetTest());
}

// Test that no polls or extraneous nudges occur when in backoff.
TEST_F(SyncSchedulerTest, BackoffDropsJobs) {
  SyncShareRecords r;
  TimeDelta poll(TimeDelta::FromMilliseconds(5));
  syncable::ModelTypeBitSet types;
  types[syncable::BOOKMARKS] = true;
  scheduler()->OnReceivedLongPollIntervalUpdate(poll);
  UseMockDelayProvider();

  EXPECT_CALL(*syncer(), SyncShare(_,_,_)).Times(2)
      .WillRepeatedly(DoAll(Invoke(sessions::test_util::SimulateCommitFailed),
          RecordSyncShareMultiple(&r, 2U)));
  EXPECT_CALL(*delay(), GetDelay(_)).
      WillRepeatedly(Return(TimeDelta::FromDays(1)));

  StartSyncScheduler(SyncScheduler::NORMAL_MODE);
  RunLoop();

  // Run again to wait for polling.
  RunLoop();

  // Pump loop to get rid of nudge.
  PumpLoop();

  Mock::VerifyAndClearExpectations(syncer());
  ASSERT_EQ(2U, r.snapshots.size());
  EXPECT_EQ(GetUpdatesCallerInfo::PERIODIC,
            r.snapshots[0]->source.updates_source);
  EXPECT_EQ(GetUpdatesCallerInfo::SYNC_CYCLE_CONTINUATION,
            r.snapshots[1]->source.updates_source);

  EXPECT_CALL(*syncer(), SyncShare(_,_,_)).Times(1)
      .WillOnce(DoAll(Invoke(sessions::test_util::SimulateCommitFailed),
          RecordSyncShare(&r)));

  // We schedule a nudge with enough delay (10X poll interval) that at least
  // one or two polls would have taken place.  The nudge should succeed.
  scheduler()->ScheduleNudge(
      poll * 10, NUDGE_SOURCE_LOCAL, types, FROM_HERE);
  RunLoop();

  Mock::VerifyAndClearExpectations(syncer());
  Mock::VerifyAndClearExpectations(delay());
  ASSERT_EQ(3U, r.snapshots.size());
  EXPECT_EQ(GetUpdatesCallerInfo::LOCAL,
            r.snapshots[2]->source.updates_source);

  EXPECT_CALL(*syncer(), SyncShare(_,_,_)).Times(0);
  EXPECT_CALL(*delay(), GetDelay(_)).Times(0);

  StartSyncScheduler(SyncScheduler::CONFIGURATION_MODE);
  RunLoop();

  scheduler()->ScheduleConfig(
      types, GetUpdatesCallerInfo::RECONFIGURATION);
  PumpLoop();

  StartSyncScheduler(SyncScheduler::NORMAL_MODE);
  RunLoop();

  scheduler()->ScheduleNudge(
      zero(), NUDGE_SOURCE_LOCAL, types, FROM_HERE);
  scheduler()->ScheduleNudge(
      zero(), NUDGE_SOURCE_LOCAL, types, FROM_HERE);
  PumpLoop();
}

// Test that backoff is shaping traffic properly with consecutive errors.
TEST_F(SyncSchedulerTest, BackoffElevation) {
  SyncShareRecords r;
  const TimeDelta poll(TimeDelta::FromMilliseconds(10));
  scheduler()->OnReceivedLongPollIntervalUpdate(poll);
  UseMockDelayProvider();

  const TimeDelta first = TimeDelta::FromSeconds(1);
  const TimeDelta second = TimeDelta::FromMilliseconds(10);
  const TimeDelta third = TimeDelta::FromMilliseconds(20);
  const TimeDelta fourth = TimeDelta::FromMilliseconds(30);
  const TimeDelta fifth = TimeDelta::FromDays(1);

  EXPECT_CALL(*syncer(), SyncShare(_,_,_)).Times(kMinNumSamples)
      .WillRepeatedly(DoAll(Invoke(sessions::test_util::SimulateCommitFailed),
          RecordSyncShareMultiple(&r, kMinNumSamples)));

  EXPECT_CALL(*delay(), GetDelay(Eq(first))).WillOnce(Return(second))
      .RetiresOnSaturation();
  EXPECT_CALL(*delay(), GetDelay(Eq(second))).WillOnce(Return(third))
      .RetiresOnSaturation();
  EXPECT_CALL(*delay(), GetDelay(Eq(third))).WillOnce(Return(fourth))
      .RetiresOnSaturation();
  EXPECT_CALL(*delay(), GetDelay(Eq(fourth))).WillOnce(Return(fifth));

  StartSyncScheduler(SyncScheduler::NORMAL_MODE);
  RunLoop();

  // Run again to wait for polling.
  RunLoop();

  ASSERT_EQ(kMinNumSamples, r.snapshots.size());
  EXPECT_GE(r.times[2] - r.times[1], second);
  EXPECT_GE(r.times[3] - r.times[2], third);
  EXPECT_GE(r.times[4] - r.times[3], fourth);
}

// Test that things go back to normal once a canary task makes forward progress
// following a succession of failures.
TEST_F(SyncSchedulerTest, BackoffRelief) {
  SyncShareRecords r;
  const TimeDelta poll(TimeDelta::FromMilliseconds(10));
  scheduler()->OnReceivedLongPollIntervalUpdate(poll);
  UseMockDelayProvider();

  const TimeDelta backoff = TimeDelta::FromMilliseconds(100);

  EXPECT_CALL(*syncer(), SyncShare(_,_,_))
      .WillOnce(Invoke(sessions::test_util::SimulateCommitFailed))
      .WillOnce(Invoke(sessions::test_util::SimulateCommitFailed))
      .WillRepeatedly(DoAll(Invoke(sessions::test_util::SimulateSuccess),
          RecordSyncShareMultiple(&r, kMinNumSamples)));
  EXPECT_CALL(*delay(), GetDelay(_)).WillOnce(Return(backoff));

  // Optimal start for the post-backoff poll party.
  TimeTicks optimal_start = TimeTicks::Now() + poll + backoff;
  StartSyncScheduler(SyncScheduler::NORMAL_MODE);
  RunLoop();

  // Run again to wait for polling.
  RunLoop();

  StopSyncScheduler();

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

TEST_F(SyncSchedulerTest, GetRecommendedDelay) {
  EXPECT_LE(TimeDelta::FromSeconds(0),
            SyncScheduler::GetRecommendedDelay(TimeDelta::FromSeconds(0)));
  EXPECT_LE(TimeDelta::FromSeconds(1),
            SyncScheduler::GetRecommendedDelay(TimeDelta::FromSeconds(1)));
  EXPECT_LE(TimeDelta::FromSeconds(50),
            SyncScheduler::GetRecommendedDelay(TimeDelta::FromSeconds(50)));
  EXPECT_LE(TimeDelta::FromSeconds(10),
            SyncScheduler::GetRecommendedDelay(TimeDelta::FromSeconds(10)));
  EXPECT_EQ(TimeDelta::FromSeconds(kMaxBackoffSeconds),
            SyncScheduler::GetRecommendedDelay(
                TimeDelta::FromSeconds(kMaxBackoffSeconds)));
  EXPECT_EQ(TimeDelta::FromSeconds(kMaxBackoffSeconds),
            SyncScheduler::GetRecommendedDelay(
                TimeDelta::FromSeconds(kMaxBackoffSeconds + 1)));
}

// Test that appropriate syncer steps are requested for each job type.
TEST_F(SyncSchedulerTest, SyncerSteps) {
  // Nudges.
  EXPECT_CALL(*syncer(), SyncShare(_, SYNCER_BEGIN, SYNCER_END))
      .Times(1);
  StartSyncScheduler(SyncScheduler::NORMAL_MODE);
  RunLoop();

  scheduler()->ScheduleNudge(
      zero(), NUDGE_SOURCE_LOCAL, ModelTypeBitSet(), FROM_HERE);
  PumpLoop();
  // Pump again to run job.
  PumpLoop();

  StopSyncScheduler();
  Mock::VerifyAndClearExpectations(syncer());

  // ClearUserData.
  EXPECT_CALL(*syncer(), SyncShare(_, CLEAR_PRIVATE_DATA, CLEAR_PRIVATE_DATA))
      .Times(1);
  StartSyncScheduler(SyncScheduler::NORMAL_MODE);
  RunLoop();

  scheduler()->ScheduleClearUserData();
  PumpLoop();
  PumpLoop();

  StopSyncScheduler();
  Mock::VerifyAndClearExpectations(syncer());

  // Configuration.
  EXPECT_CALL(*syncer(), SyncShare(_, DOWNLOAD_UPDATES, APPLY_UPDATES));
  StartSyncScheduler(SyncScheduler::CONFIGURATION_MODE);
  RunLoop();

  scheduler()->ScheduleConfig(
      ModelTypeBitSet(), GetUpdatesCallerInfo::RECONFIGURATION);
  PumpLoop();
  PumpLoop();

  StopSyncScheduler();
  Mock::VerifyAndClearExpectations(syncer());

  // Cleanup disabled types.
  EXPECT_CALL(*syncer(),
              SyncShare(_, CLEANUP_DISABLED_TYPES, CLEANUP_DISABLED_TYPES));
  StartSyncScheduler(SyncScheduler::NORMAL_MODE);
  RunLoop();

  scheduler()->ScheduleCleanupDisabledTypes();
  // Only need to pump once, as ScheduleCleanupDisabledTypes()
  // schedules the job directly.
  PumpLoop();

  StopSyncScheduler();
  Mock::VerifyAndClearExpectations(syncer());

  // Poll.
  EXPECT_CALL(*syncer(), SyncShare(_, SYNCER_BEGIN, SYNCER_END))
      .Times(AtLeast(1))
      .WillRepeatedly(QuitLoopNowAction());
  const TimeDelta poll(TimeDelta::FromMilliseconds(10));
  scheduler()->OnReceivedLongPollIntervalUpdate(poll);

  StartSyncScheduler(SyncScheduler::NORMAL_MODE);
  RunLoop();

  // Run again to wait for polling.
  RunLoop();

  StopSyncScheduler();
  Mock::VerifyAndClearExpectations(syncer());
}

// Test config tasks don't run during normal mode.
// TODO(tim): Implement this test and then the functionality!
TEST_F(SyncSchedulerTest, DISABLED_NoConfigDuringNormal) {
}

// Test that starting the syncer thread without a valid connection doesn't
// break things when a connection is detected.
TEST_F(SyncSchedulerTest, StartWhenNotConnected) {
  connection()->SetServerNotReachable();
  EXPECT_CALL(*syncer(), SyncShare(_,_,_)).WillOnce(QuitLoopNowAction());
  StartSyncScheduler(SyncScheduler::NORMAL_MODE);
  RunLoop();

  scheduler()->ScheduleNudge(
      zero(), NUDGE_SOURCE_LOCAL, ModelTypeBitSet(), FROM_HERE);
  // Should save the nudge for until after the server is reachable.
  PumpLoop();

  connection()->SetServerReachable();
  PumpLoop();
}

TEST_F(SyncSchedulerTest, SetsPreviousRoutingInfo) {
  ModelSafeRoutingInfo info;
  EXPECT_TRUE(info == context()->previous_session_routing_info());
  ModelSafeRoutingInfo expected;
  context()->registrar()->GetModelSafeRoutingInfo(&expected);
  ASSERT_FALSE(expected.empty());
  EXPECT_CALL(*syncer(), SyncShare(_,_,_)).Times(1);

  StartSyncScheduler(SyncScheduler::NORMAL_MODE);
  RunLoop();

  scheduler()->ScheduleNudge(
      zero(), NUDGE_SOURCE_LOCAL, ModelTypeBitSet(), FROM_HERE);
  PumpLoop();
  // Pump again to run job.
  PumpLoop();

  StopSyncScheduler();

  EXPECT_TRUE(expected == context()->previous_session_routing_info());
}

}  // namespace browser_sync
