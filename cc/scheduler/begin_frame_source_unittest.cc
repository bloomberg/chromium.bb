// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/scheduler/begin_frame_source.h"

#include <stdint.h>

#include "base/memory/ptr_util.h"
#include "base/test/test_simple_task_runner.h"
#include "cc/test/begin_frame_args_test.h"
#include "cc/test/begin_frame_source_test.h"
#include "cc/test/scheduler_test_common.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Mock;
using testing::StrictMock;

namespace cc {
namespace {

// BeginFrameObserverBase testing ---------------------------------------
class MockMinimalBeginFrameObserverBase : public BeginFrameObserverBase {
 public:
  MOCK_METHOD1(OnBeginFrameDerivedImpl, bool(const BeginFrameArgs&));
  MOCK_METHOD1(OnBeginFrameSourcePausedChanged, void(bool));
  int64_t dropped_begin_frame_args() const { return dropped_begin_frame_args_; }
};

TEST(BeginFrameObserverBaseTest, OnBeginFrameImplementation) {
  using ::testing::Return;
  MockMinimalBeginFrameObserverBase obs;
  ::testing::InSequence ordered;  // These calls should be ordered

  // Initial conditions
  EXPECT_EQ(BeginFrameArgs(), obs.LastUsedBeginFrameArgs());
  EXPECT_EQ(0, obs.dropped_begin_frame_args());

#ifndef NDEBUG
  EXPECT_DEATH({ obs.OnBeginFrame(BeginFrameArgs()); }, "");
#endif

  BeginFrameArgs args1 =
      CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 100, 200, 300);
  EXPECT_CALL(obs, OnBeginFrameDerivedImpl(args1)).WillOnce(Return(true));
  obs.OnBeginFrame(args1);
  EXPECT_EQ(args1, obs.LastUsedBeginFrameArgs());
  EXPECT_EQ(0, obs.dropped_begin_frame_args());

#ifndef NDEBUG
  EXPECT_DEATH({
                 obs.OnBeginFrame(CreateBeginFrameArgsForTesting(
                     BEGINFRAME_FROM_HERE, 50, 200, 300));
               },
               "");
#endif

  // Returning false shouldn't update the LastUsedBeginFrameArgs value.
  BeginFrameArgs args2 =
      CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 200, 300, 400);
  EXPECT_CALL(obs, OnBeginFrameDerivedImpl(args2)).WillOnce(Return(false));
  obs.OnBeginFrame(args2);
  EXPECT_EQ(args1, obs.LastUsedBeginFrameArgs());
  EXPECT_EQ(1, obs.dropped_begin_frame_args());

  BeginFrameArgs args3 =
      CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 150, 300, 400);
  EXPECT_CALL(obs, OnBeginFrameDerivedImpl(args3)).WillOnce(Return(true));
  obs.OnBeginFrame(args3);
  EXPECT_EQ(args3, obs.LastUsedBeginFrameArgs());
  EXPECT_EQ(1, obs.dropped_begin_frame_args());
}

// BeginFrameSource testing ----------------------------------------------
TEST(BeginFrameSourceBaseTest, ObserverManipulation) {
  MockBeginFrameObserver obs;
  MockBeginFrameObserver otherObs;
  FakeBeginFrameSource source;

  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(obs, false);
  source.AddObserver(&obs);
  EXPECT_TRUE(source.has_observers());

#ifndef NDEBUG
  // Adding an observer when it already exists should DCHECK fail.
  EXPECT_DEATH({ source.AddObserver(&obs); }, "");

  // Removing wrong observer should DCHECK fail.
  EXPECT_DEATH({ source.RemoveObserver(&otherObs); }, "");

  // Removing an observer when there is no observer should DCHECK fail.
  EXPECT_DEATH(
      {
        source.RemoveObserver(&obs);
        source.RemoveObserver(&obs);
      },
      "");
#endif

  source.RemoveObserver(&obs);
  EXPECT_FALSE(source.has_observers());

  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(otherObs, false);
  source.AddObserver(&otherObs);
  EXPECT_TRUE(source.has_observers());
  source.RemoveObserver(&otherObs);
  EXPECT_FALSE(source.has_observers());
}

TEST(BeginFrameSourceBaseTest, Observer) {
  FakeBeginFrameSource source;
  MockBeginFrameObserver obs;
  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(obs, false);
  source.AddObserver(&obs);
  EXPECT_BEGIN_FRAME_USED(obs, 100, 200, 300);
  EXPECT_BEGIN_FRAME_DROP(obs, 400, 600, 300);
  EXPECT_BEGIN_FRAME_DROP(obs, 450, 650, 300);
  EXPECT_BEGIN_FRAME_USED(obs, 700, 900, 300);

  SEND_BEGIN_FRAME_USED(source, 100, 200, 300);
  SEND_BEGIN_FRAME_DROP(source, 400, 600, 300);
  SEND_BEGIN_FRAME_DROP(source, 450, 650, 300);
  SEND_BEGIN_FRAME_USED(source, 700, 900, 300);
}

TEST(BeginFrameSourceBaseTest, NoObserver) {
  FakeBeginFrameSource source;
  SEND_BEGIN_FRAME_DROP(source, 100, 200, 300);
}

TEST(BeginFrameSourceBaseTest, SetBeginFrameSourcePaused) {
  FakeBeginFrameSource source;
  MockBeginFrameObserver obs;
  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(obs, false);
  source.AddObserver(&obs);

  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(obs, true);
  source.SetBeginFrameSourcePaused(true);
  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(obs, false);
  source.SetBeginFrameSourcePaused(false);
}

TEST(BeginFrameSourceBaseTest, MultipleObservers) {
  FakeBeginFrameSource source;
  StrictMock<MockBeginFrameObserver> obs1, obs2;

  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(obs1, false);
  source.AddObserver(&obs1);

  EXPECT_BEGIN_FRAME_USED(obs1, 100, 200, 100);
  SEND_BEGIN_FRAME_USED(source, 100, 200, 100);

  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(obs2, false);
  source.AddObserver(&obs2);

  EXPECT_BEGIN_FRAME_USED(obs1, 200, 300, 100);
  EXPECT_BEGIN_FRAME_USED(obs2, 200, 300, 100);
  SEND_BEGIN_FRAME_USED(source, 200, 300, 100);

  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(obs1, true);
  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(obs2, true);
  source.SetBeginFrameSourcePaused(true);

  source.RemoveObserver(&obs1);

  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(obs2, false);
  source.SetBeginFrameSourcePaused(false);

  EXPECT_BEGIN_FRAME_USED(obs2, 300, 400, 100);
  SEND_BEGIN_FRAME_USED(source, 300, 400, 100);

  source.RemoveObserver(&obs2);
}

class LoopingBeginFrameObserver : public BeginFrameObserverBase {
 public:
  BeginFrameSource* source_;

  void AsValueInto(base::trace_event::TracedValue* dict) const override {
    dict->SetString("type", "LoopingBeginFrameObserver");
    dict->BeginDictionary("source");
    source_->AsValueInto(dict);
    dict->EndDictionary();
  }

 protected:
  // BeginFrameObserverBase
  bool OnBeginFrameDerivedImpl(const BeginFrameArgs& args) override {
    return true;
  }

  void OnBeginFrameSourcePausedChanged(bool paused) override {}
};

TEST(BeginFrameSourceBaseTest, DetectAsValueIntoLoop) {
  LoopingBeginFrameObserver obs;
  FakeBeginFrameSource source;

  obs.source_ = &source;
  source.AddObserver(&obs);

  std::unique_ptr<base::trace_event::TracedValue> state(
      new base::trace_event::TracedValue());
  source.AsValueInto(state.get());
}

// BackToBackBeginFrameSource testing -----------------------------------------
class TestBackToBackBeginFrameSource : public BackToBackBeginFrameSource {
 public:
  TestBackToBackBeginFrameSource(base::SimpleTestTickClock* now_src,
                                 base::SingleThreadTaskRunner* task_runner)
      : BackToBackBeginFrameSource(task_runner), now_src_(now_src) {}

 protected:
  base::TimeTicks Now() override { return now_src_->NowTicks(); }

  // Not owned.
  base::SimpleTestTickClock* now_src_;
};

class BackToBackBeginFrameSourceTest : public ::testing::Test {
 public:
  static const int64_t kDeadline;
  static const int64_t kInterval;

  std::unique_ptr<base::SimpleTestTickClock> now_src_;
  scoped_refptr<OrderedSimpleTaskRunner> task_runner_;
  std::unique_ptr<TestBackToBackBeginFrameSource> source_;
  std::unique_ptr<MockBeginFrameObserver> obs_;

  void SetUp() override {
    now_src_.reset(new base::SimpleTestTickClock());
    now_src_->Advance(base::TimeDelta::FromMicroseconds(1000));
    task_runner_ =
        make_scoped_refptr(new OrderedSimpleTaskRunner(now_src_.get(), false));
    source_.reset(
        new TestBackToBackBeginFrameSource(now_src_.get(), task_runner_.get()));
    obs_ =
        base::WrapUnique(new ::testing::StrictMock<MockBeginFrameObserver>());
  }

  void TearDown() override { obs_.reset(); }
};

const int64_t BackToBackBeginFrameSourceTest::kDeadline =
    BeginFrameArgs::DefaultInterval().ToInternalValue();

const int64_t BackToBackBeginFrameSourceTest::kInterval =
    BeginFrameArgs::DefaultInterval().ToInternalValue();

TEST_F(BackToBackBeginFrameSourceTest, AddObserverSendsBeginFrame) {
  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(*obs_, false);
  source_->AddObserver(obs_.get());
  EXPECT_TRUE(task_runner_->HasPendingTasks());
  EXPECT_BEGIN_FRAME_USED(*obs_, 1000, 1000 + kDeadline, kInterval);
  task_runner_->RunPendingTasks();

  EXPECT_BEGIN_FRAME_USED(*obs_, 1100, 1100 + kDeadline, kInterval);
  now_src_->Advance(base::TimeDelta::FromMicroseconds(100));
  source_->DidFinishFrame(obs_.get(), 0);
  task_runner_->RunPendingTasks();
}

TEST_F(BackToBackBeginFrameSourceTest,
       DidFinishFrameThenRemoveObserverProducesNoFrame) {
  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(*obs_, false);
  source_->AddObserver(obs_.get());
  EXPECT_BEGIN_FRAME_USED(*obs_, 1000, 1000 + kDeadline, kInterval);
  task_runner_->RunPendingTasks();

  source_->RemoveObserver(obs_.get());
  source_->DidFinishFrame(obs_.get(), 0);

  EXPECT_FALSE(task_runner_->HasPendingTasks());
}

TEST_F(BackToBackBeginFrameSourceTest,
       RemoveObserverThenDidFinishFrameProducesNoFrame) {
  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(*obs_, false);
  source_->AddObserver(obs_.get());
  EXPECT_BEGIN_FRAME_USED(*obs_, 1000, 1000 + kDeadline, kInterval);
  task_runner_->RunPendingTasks();

  now_src_->Advance(base::TimeDelta::FromMicroseconds(100));
  source_->DidFinishFrame(obs_.get(), 0);
  source_->RemoveObserver(obs_.get());

  EXPECT_TRUE(task_runner_->HasPendingTasks());
  task_runner_->RunPendingTasks();
}

TEST_F(BackToBackBeginFrameSourceTest,
       TogglingObserverThenDidFinishFrameProducesCorrectFrame) {
  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(*obs_, false);
  source_->AddObserver(obs_.get());
  EXPECT_BEGIN_FRAME_USED(*obs_, 1000, 1000 + kDeadline, kInterval);
  task_runner_->RunPendingTasks();

  now_src_->Advance(base::TimeDelta::FromMicroseconds(100));
  source_->RemoveObserver(obs_.get());

  now_src_->Advance(base::TimeDelta::FromMicroseconds(10));
  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(*obs_, false);
  source_->AddObserver(obs_.get());

  now_src_->Advance(base::TimeDelta::FromMicroseconds(10));
  source_->DidFinishFrame(obs_.get(), 0);

  now_src_->Advance(base::TimeDelta::FromMicroseconds(10));
  EXPECT_BEGIN_FRAME_USED(*obs_, 1130, 1130 + kDeadline, kInterval);
  EXPECT_TRUE(task_runner_->HasPendingTasks());
  task_runner_->RunPendingTasks();
}

TEST_F(BackToBackBeginFrameSourceTest,
       DidFinishFrameThenTogglingObserverProducesCorrectFrame) {
  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(*obs_, false);
  source_->AddObserver(obs_.get());
  EXPECT_BEGIN_FRAME_USED(*obs_, 1000, 1000 + kDeadline, kInterval);
  task_runner_->RunPendingTasks();

  now_src_->Advance(base::TimeDelta::FromMicroseconds(100));
  source_->DidFinishFrame(obs_.get(), 0);

  now_src_->Advance(base::TimeDelta::FromMicroseconds(10));
  source_->RemoveObserver(obs_.get());

  now_src_->Advance(base::TimeDelta::FromMicroseconds(10));
  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(*obs_, false);
  source_->AddObserver(obs_.get());

  now_src_->Advance(base::TimeDelta::FromMicroseconds(10));
  EXPECT_BEGIN_FRAME_USED(*obs_, 1130, 1130 + kDeadline, kInterval);
  EXPECT_TRUE(task_runner_->HasPendingTasks());
  task_runner_->RunPendingTasks();
}

TEST_F(BackToBackBeginFrameSourceTest, DidFinishFrameNoObserver) {
  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(*obs_, false);
  source_->AddObserver(obs_.get());
  source_->RemoveObserver(obs_.get());
  source_->DidFinishFrame(obs_.get(), 0);
  EXPECT_FALSE(task_runner_->RunPendingTasks());
}

TEST_F(BackToBackBeginFrameSourceTest, DidFinishFrameRemainingFrames) {
  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(*obs_, false);
  source_->AddObserver(obs_.get());
  EXPECT_BEGIN_FRAME_USED(*obs_, 1000, 1000 + kDeadline, kInterval);
  task_runner_->RunPendingTasks();

  now_src_->Advance(base::TimeDelta::FromMicroseconds(100));

  source_->DidFinishFrame(obs_.get(), 3);
  EXPECT_FALSE(task_runner_->HasPendingTasks());
  source_->DidFinishFrame(obs_.get(), 2);
  EXPECT_FALSE(task_runner_->HasPendingTasks());
  source_->DidFinishFrame(obs_.get(), 1);
  EXPECT_FALSE(task_runner_->HasPendingTasks());

  EXPECT_BEGIN_FRAME_USED(*obs_, 1100, 1100 + kDeadline, kInterval);
  source_->DidFinishFrame(obs_.get(), 0);
  EXPECT_EQ(base::TimeDelta(), task_runner_->DelayToNextTaskTime());
  task_runner_->RunPendingTasks();
}

TEST_F(BackToBackBeginFrameSourceTest, DidFinishFrameMultipleCallsIdempotent) {
  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(*obs_, false);
  source_->AddObserver(obs_.get());
  EXPECT_BEGIN_FRAME_USED(*obs_, 1000, 1000 + kDeadline, kInterval);
  task_runner_->RunPendingTasks();

  now_src_->Advance(base::TimeDelta::FromMicroseconds(100));
  source_->DidFinishFrame(obs_.get(), 0);
  source_->DidFinishFrame(obs_.get(), 0);
  source_->DidFinishFrame(obs_.get(), 0);
  EXPECT_BEGIN_FRAME_USED(*obs_, 1100, 1100 + kDeadline, kInterval);
  task_runner_->RunPendingTasks();

  now_src_->Advance(base::TimeDelta::FromMicroseconds(100));
  source_->DidFinishFrame(obs_.get(), 0);
  source_->DidFinishFrame(obs_.get(), 0);
  source_->DidFinishFrame(obs_.get(), 0);
  EXPECT_BEGIN_FRAME_USED(*obs_, 1200, 1200 + kDeadline, kInterval);
  task_runner_->RunPendingTasks();
}

TEST_F(BackToBackBeginFrameSourceTest, DelayInPostedTaskProducesCorrectFrame) {
  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(*obs_, false);
  source_->AddObserver(obs_.get());
  EXPECT_BEGIN_FRAME_USED(*obs_, 1000, 1000 + kDeadline, kInterval);
  task_runner_->RunPendingTasks();

  now_src_->Advance(base::TimeDelta::FromMicroseconds(100));
  source_->DidFinishFrame(obs_.get(), 0);
  now_src_->Advance(base::TimeDelta::FromMicroseconds(50));
  EXPECT_BEGIN_FRAME_USED(*obs_, 1150, 1150 + kDeadline, kInterval);

  EXPECT_TRUE(task_runner_->HasPendingTasks());
  task_runner_->RunPendingTasks();
}

TEST_F(BackToBackBeginFrameSourceTest, MultipleObserversSynchronized) {
  StrictMock<MockBeginFrameObserver> obs1, obs2;

  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(obs1, false);
  source_->AddObserver(&obs1);
  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(obs2, false);
  source_->AddObserver(&obs2);

  EXPECT_BEGIN_FRAME_USED(obs1, 1000, 1000 + kDeadline, kInterval);
  EXPECT_BEGIN_FRAME_USED(obs2, 1000, 1000 + kDeadline, kInterval);
  task_runner_->RunPendingTasks();

  now_src_->Advance(base::TimeDelta::FromMicroseconds(100));
  source_->DidFinishFrame(&obs1, 0);
  source_->DidFinishFrame(&obs2, 0);
  EXPECT_BEGIN_FRAME_USED(obs1, 1100, 1100 + kDeadline, kInterval);
  EXPECT_BEGIN_FRAME_USED(obs2, 1100, 1100 + kDeadline, kInterval);
  task_runner_->RunPendingTasks();

  now_src_->Advance(base::TimeDelta::FromMicroseconds(100));
  source_->DidFinishFrame(&obs1, 0);
  source_->DidFinishFrame(&obs2, 0);
  EXPECT_TRUE(task_runner_->HasPendingTasks());
  source_->RemoveObserver(&obs1);
  source_->RemoveObserver(&obs2);
  task_runner_->RunPendingTasks();
}

TEST_F(BackToBackBeginFrameSourceTest, MultipleObserversInterleaved) {
  StrictMock<MockBeginFrameObserver> obs1, obs2;

  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(obs1, false);
  source_->AddObserver(&obs1);
  EXPECT_BEGIN_FRAME_USED(obs1, 1000, 1000 + kDeadline, kInterval);
  task_runner_->RunPendingTasks();

  now_src_->Advance(base::TimeDelta::FromMicroseconds(100));
  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(obs2, false);
  source_->AddObserver(&obs2);
  EXPECT_BEGIN_FRAME_USED(obs2, 1100, 1100 + kDeadline, kInterval);
  task_runner_->RunPendingTasks();

  now_src_->Advance(base::TimeDelta::FromMicroseconds(100));
  source_->DidFinishFrame(&obs1, 0);
  EXPECT_BEGIN_FRAME_USED(obs1, 1200, 1200 + kDeadline, kInterval);
  task_runner_->RunPendingTasks();

  source_->DidFinishFrame(&obs1, 0);
  source_->RemoveObserver(&obs1);

  now_src_->Advance(base::TimeDelta::FromMicroseconds(100));
  source_->DidFinishFrame(&obs2, 0);
  EXPECT_BEGIN_FRAME_USED(obs2, 1300, 1300 + kDeadline, kInterval);
  task_runner_->RunPendingTasks();

  source_->DidFinishFrame(&obs2, 0);
  source_->RemoveObserver(&obs2);
}

// SyntheticBeginFrameSource testing ------------------------------------------
class SyntheticBeginFrameSourceTest : public ::testing::Test {
 public:
  std::unique_ptr<base::SimpleTestTickClock> now_src_;
  scoped_refptr<OrderedSimpleTaskRunner> task_runner_;
  std::unique_ptr<TestSyntheticBeginFrameSource> source_;
  std::unique_ptr<MockBeginFrameObserver> obs_;

  void SetUp() override {
    now_src_.reset(new base::SimpleTestTickClock());
    now_src_->Advance(base::TimeDelta::FromMicroseconds(1000));
    task_runner_ =
        make_scoped_refptr(new OrderedSimpleTaskRunner(now_src_.get(), false));
    source_.reset(new TestSyntheticBeginFrameSource(
        now_src_.get(), task_runner_.get(),
        base::TimeDelta::FromMicroseconds(10000)));
    obs_ = base::WrapUnique(new MockBeginFrameObserver());
  }

  void TearDown() override { obs_.reset(); }
};

TEST_F(SyntheticBeginFrameSourceTest,
       AddObserverCallsOnBeginFrameWithMissedTick) {
  now_src_->Advance(base::TimeDelta::FromMicroseconds(9010));
  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(*obs_, false);
  EXPECT_BEGIN_FRAME_USED_MISSED(*obs_, 10000, 20000, 10000);
  source_->AddObserver(obs_.get());  // Should cause the last tick to be sent
  // No tasks should need to be run for this to occur.
}

TEST_F(SyntheticBeginFrameSourceTest, AddObserverCallsCausesOnBeginFrame) {
  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(*obs_, false);
  EXPECT_BEGIN_FRAME_USED_MISSED(*obs_, 0, 10000, 10000);
  source_->AddObserver(obs_.get());
  EXPECT_EQ(10000, task_runner_->NextTaskTime().ToInternalValue());

  EXPECT_BEGIN_FRAME_USED(*obs_, 10000, 20000, 10000);
  now_src_->Advance(base::TimeDelta::FromMicroseconds(9010));
  task_runner_->RunPendingTasks();
}

TEST_F(SyntheticBeginFrameSourceTest, BasicOperation) {
  task_runner_->SetAutoAdvanceNowToPendingTasks(true);

  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(*obs_, false);
  EXPECT_BEGIN_FRAME_USED_MISSED(*obs_, 0, 10000, 10000);
  source_->AddObserver(obs_.get());
  EXPECT_BEGIN_FRAME_USED(*obs_, 10000, 20000, 10000);
  EXPECT_BEGIN_FRAME_USED(*obs_, 20000, 30000, 10000);
  EXPECT_BEGIN_FRAME_USED(*obs_, 30000, 40000, 10000);
  task_runner_->RunUntilTime(base::TimeTicks::FromInternalValue(30001));

  source_->RemoveObserver(obs_.get());
  // No new frames....
  task_runner_->RunUntilTime(base::TimeTicks::FromInternalValue(60000));
}

TEST_F(SyntheticBeginFrameSourceTest, VSyncChanges) {
  task_runner_->SetAutoAdvanceNowToPendingTasks(true);
  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(*obs_, false);
  EXPECT_BEGIN_FRAME_USED_MISSED(*obs_, 0, 10000, 10000);
  source_->AddObserver(obs_.get());

  EXPECT_BEGIN_FRAME_USED(*obs_, 10000, 20000, 10000);
  EXPECT_BEGIN_FRAME_USED(*obs_, 20000, 30000, 10000);
  EXPECT_BEGIN_FRAME_USED(*obs_, 30000, 40000, 10000);
  task_runner_->RunUntilTime(base::TimeTicks::FromInternalValue(30001));

  // Update the vsync information
  source_->OnUpdateVSyncParameters(base::TimeTicks::FromInternalValue(27500),
                                   base::TimeDelta::FromMicroseconds(10001));

  EXPECT_BEGIN_FRAME_USED(*obs_, 40000, 47502, 10001);
  EXPECT_BEGIN_FRAME_USED(*obs_, 47502, 57503, 10001);
  EXPECT_BEGIN_FRAME_USED(*obs_, 57503, 67504, 10001);
  task_runner_->RunUntilTime(base::TimeTicks::FromInternalValue(60000));
}

TEST_F(SyntheticBeginFrameSourceTest, MultipleObservers) {
  StrictMock<MockBeginFrameObserver> obs1, obs2;

  // now_src_ starts off at 1000.
  task_runner_->RunForPeriod(base::TimeDelta::FromMicroseconds(9010));
  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(obs1, false);
  EXPECT_BEGIN_FRAME_USED_MISSED(obs1, 10000, 20000, 10000);
  source_->AddObserver(&obs1);  // Should cause the last tick to be sent
  // No tasks should need to be run for this to occur.

  EXPECT_BEGIN_FRAME_USED(obs1, 20000, 30000, 10000);
  task_runner_->RunForPeriod(base::TimeDelta::FromMicroseconds(10000));

  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(obs2, false);
  EXPECT_BEGIN_FRAME_USED_MISSED(obs2, 20000, 30000, 10000);
  source_->AddObserver(&obs2);  // Should cause the last tick to be sent
  // No tasks should need to be run for this to occur.

  EXPECT_BEGIN_FRAME_USED(obs1, 30000, 40000, 10000);
  EXPECT_BEGIN_FRAME_USED(obs2, 30000, 40000, 10000);
  task_runner_->RunForPeriod(base::TimeDelta::FromMicroseconds(10000));

  source_->RemoveObserver(&obs1);

  EXPECT_BEGIN_FRAME_USED(obs2, 40000, 50000, 10000);
  task_runner_->RunForPeriod(base::TimeDelta::FromMicroseconds(10000));

  source_->RemoveObserver(&obs2);
  task_runner_->RunUntilTime(base::TimeTicks::FromInternalValue(50000));
  EXPECT_FALSE(task_runner_->HasPendingTasks());
}

TEST_F(SyntheticBeginFrameSourceTest, DoubleTick) {
  StrictMock<MockBeginFrameObserver> obs;

  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(obs, false);
  EXPECT_BEGIN_FRAME_USED_MISSED(obs, 0, 10000, 10000);
  source_->AddObserver(&obs);

  source_->OnUpdateVSyncParameters(base::TimeTicks::FromInternalValue(5000),
                                   base::TimeDelta::FromInternalValue(10000));
  now_src_->Advance(base::TimeDelta::FromInternalValue(4000));

  // No begin frame received.
  task_runner_->RunPendingTasks();

  // Begin frame received.
  source_->OnUpdateVSyncParameters(base::TimeTicks::FromInternalValue(10000),
                                   base::TimeDelta::FromInternalValue(10000));
  now_src_->Advance(base::TimeDelta::FromInternalValue(5000));
  EXPECT_BEGIN_FRAME_USED(obs, 10000, 20000, 10000);
  task_runner_->RunPendingTasks();
}

TEST_F(SyntheticBeginFrameSourceTest, DoubleTickMissedFrame) {
  StrictMock<MockBeginFrameObserver> obs;

  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(obs, false);
  EXPECT_BEGIN_FRAME_USED_MISSED(obs, 0, 10000, 10000);
  source_->AddObserver(&obs);
  source_->RemoveObserver(&obs);

  source_->OnUpdateVSyncParameters(base::TimeTicks::FromInternalValue(5000),
                                   base::TimeDelta::FromInternalValue(10000));
  now_src_->Advance(base::TimeDelta::FromInternalValue(4000));

  // No missed frame received.
  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(obs, false);
  source_->AddObserver(&obs);
  source_->RemoveObserver(&obs);

  // Missed frame received.
  source_->OnUpdateVSyncParameters(base::TimeTicks::FromInternalValue(10000),
                                   base::TimeDelta::FromInternalValue(10000));
  now_src_->Advance(base::TimeDelta::FromInternalValue(5000));
  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(obs, false);
  EXPECT_BEGIN_FRAME_USED_MISSED(obs, 10000, 20000, 10000);
  source_->AddObserver(&obs);
  source_->RemoveObserver(&obs);
}

}  // namespace
}  // namespace cc
