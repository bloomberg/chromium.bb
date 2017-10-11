// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/scheduler.h"

#include <algorithm>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/test/test_simple_task_runner.h"
#include "gpu/command_buffer/service/sync_point_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gpu {
namespace {

template <typename T>
void RunFunctor(T functor) {
  functor();
}

template <typename T>
base::OnceClosure GetClosure(T functor) {
  return base::BindOnce(&RunFunctor<T>, functor);
}

class SchedulerTest : public testing::Test {
 public:
  SchedulerTest()
      : task_runner_(new base::TestSimpleTaskRunner()),
        sync_point_manager_(new SyncPointManager),
        scheduler_(new Scheduler(task_runner_, sync_point_manager_.get())) {}

 protected:
  base::TestSimpleTaskRunner* task_runner() const { return task_runner_.get(); }

  SyncPointManager* sync_point_manager() const {
    return sync_point_manager_.get();
  }

  Scheduler* scheduler() const { return scheduler_.get(); }

 private:
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  std::unique_ptr<SyncPointManager> sync_point_manager_;
  std::unique_ptr<Scheduler> scheduler_;
};

TEST_F(SchedulerTest, ScheduledTasksRunInOrder) {
  SequenceId sequence_id =
      scheduler()->CreateSequence(SchedulingPriority::kNormal);

  bool ran1 = false;
  scheduler()->ScheduleTask(Scheduler::Task(
      sequence_id, GetClosure([&] { ran1 = true; }), std::vector<SyncToken>()));

  bool ran2 = false;
  scheduler()->ScheduleTask(Scheduler::Task(
      sequence_id, GetClosure([&] { ran2 = true; }), std::vector<SyncToken>()));

  task_runner()->RunPendingTasks();
  EXPECT_TRUE(ran1);

  task_runner()->RunPendingTasks();
  EXPECT_TRUE(ran2);
}

TEST_F(SchedulerTest, ContinuedTasksRunFirst) {
  SequenceId sequence_id =
      scheduler()->CreateSequence(SchedulingPriority::kNormal);

  bool ran1 = false;
  bool continued1 = false;
  scheduler()->ScheduleTask(Scheduler::Task(
      sequence_id, GetClosure([&] {
        scheduler()->ContinueTask(sequence_id,
                                  GetClosure([&] { continued1 = true; }));
        ran1 = true;
      }),
      std::vector<SyncToken>()));

  bool ran2 = false;
  scheduler()->ScheduleTask(Scheduler::Task(
      sequence_id, GetClosure([&] { ran2 = true; }), std::vector<SyncToken>()));

  task_runner()->RunPendingTasks();
  EXPECT_TRUE(ran1);
  EXPECT_FALSE(continued1);
  EXPECT_FALSE(ran2);

  task_runner()->RunPendingTasks();
  EXPECT_TRUE(continued1);
  EXPECT_FALSE(ran2);

  task_runner()->RunPendingTasks();
  EXPECT_TRUE(ran2);
}

TEST_F(SchedulerTest, SequencesRunInPriorityOrder) {
  SequenceId sequence_id1 =
      scheduler()->CreateSequence(SchedulingPriority::kLow);
  bool ran1 = false;
  scheduler()->ScheduleTask(Scheduler::Task(sequence_id1,
                                            GetClosure([&] { ran1 = true; }),
                                            std::vector<SyncToken>()));

  SequenceId sequence_id2 =
      scheduler()->CreateSequence(SchedulingPriority::kNormal);
  bool ran2 = false;
  scheduler()->ScheduleTask(Scheduler::Task(sequence_id2,
                                            GetClosure([&] { ran2 = true; }),
                                            std::vector<SyncToken>()));

  SequenceId sequence_id3 =
      scheduler()->CreateSequence(SchedulingPriority::kHigh);
  bool ran3 = false;
  scheduler()->ScheduleTask(Scheduler::Task(sequence_id3,
                                            GetClosure([&] { ran3 = true; }),
                                            std::vector<SyncToken>()));

  task_runner()->RunPendingTasks();
  EXPECT_TRUE(ran3);

  task_runner()->RunPendingTasks();
  EXPECT_TRUE(ran2);

  task_runner()->RunPendingTasks();
  EXPECT_TRUE(ran1);
}

TEST_F(SchedulerTest, SequencesOfSamePriorityRunInOrder) {
  SequenceId sequence_id1 =
      scheduler()->CreateSequence(SchedulingPriority::kNormal);
  bool ran1 = false;
  scheduler()->ScheduleTask(Scheduler::Task(sequence_id1,
                                            GetClosure([&] { ran1 = true; }),
                                            std::vector<SyncToken>()));

  SequenceId sequence_id2 =
      scheduler()->CreateSequence(SchedulingPriority::kNormal);
  bool ran2 = false;
  scheduler()->ScheduleTask(Scheduler::Task(sequence_id2,
                                            GetClosure([&] { ran2 = true; }),
                                            std::vector<SyncToken>()));

  SequenceId sequence_id3 =
      scheduler()->CreateSequence(SchedulingPriority::kNormal);
  bool ran3 = false;
  scheduler()->ScheduleTask(Scheduler::Task(sequence_id3,
                                            GetClosure([&] { ran3 = true; }),
                                            std::vector<SyncToken>()));

  SequenceId sequence_id4 =
      scheduler()->CreateSequence(SchedulingPriority::kNormal);
  bool ran4 = false;
  scheduler()->ScheduleTask(Scheduler::Task(sequence_id4,
                                            GetClosure([&] { ran4 = true; }),
                                            std::vector<SyncToken>()));

  task_runner()->RunPendingTasks();
  EXPECT_TRUE(ran1);

  task_runner()->RunPendingTasks();
  EXPECT_TRUE(ran2);

  task_runner()->RunPendingTasks();
  EXPECT_TRUE(ran3);

  task_runner()->RunPendingTasks();
  EXPECT_TRUE(ran4);
}

TEST_F(SchedulerTest, SequenceWaitsForFence) {
  SequenceId sequence_id1 =
      scheduler()->CreateSequence(SchedulingPriority::kHigh);
  SequenceId sequence_id2 =
      scheduler()->CreateSequence(SchedulingPriority::kNormal);

  CommandBufferNamespace namespace_id = CommandBufferNamespace::GPU_IO;
  CommandBufferId command_buffer_id = CommandBufferId::FromUnsafeValue(1);

  scoped_refptr<SyncPointClientState> release_state =
      sync_point_manager()->CreateSyncPointClientState(
          namespace_id, command_buffer_id, sequence_id2);

  uint64_t release = 1;
  bool ran2 = false;
  scheduler()->ScheduleTask(Scheduler::Task(sequence_id2, GetClosure([&] {
                                              release_state->ReleaseFenceSync(
                                                  release);
                                              ran2 = true;
                                            }),
                                            std::vector<SyncToken>()));

  SyncToken sync_token(namespace_id, 0, command_buffer_id, release);

  bool ran1 = false;
  scheduler()->ScheduleTask(Scheduler::Task(
      sequence_id1, GetClosure([&] { ran1 = true; }), {sync_token}));

  task_runner()->RunPendingTasks();
  EXPECT_FALSE(ran1);
  EXPECT_TRUE(ran2);
  EXPECT_TRUE(sync_point_manager()->IsSyncTokenReleased(sync_token));

  task_runner()->RunPendingTasks();
  EXPECT_TRUE(ran1);

  release_state->Destroy();
}

TEST_F(SchedulerTest, SequenceDoesNotWaitForInvalidFence) {
  SequenceId sequence_id1 =
      scheduler()->CreateSequence(SchedulingPriority::kNormal);

  SequenceId sequence_id2 =
      scheduler()->CreateSequence(SchedulingPriority::kNormal);
  CommandBufferNamespace namespace_id = CommandBufferNamespace::GPU_IO;
  CommandBufferId command_buffer_id = CommandBufferId::FromUnsafeValue(1);
  scoped_refptr<SyncPointClientState> release_state =
      sync_point_manager()->CreateSyncPointClientState(
          namespace_id, command_buffer_id, sequence_id2);

  uint64_t release = 1;
  SyncToken sync_token(namespace_id, 0, command_buffer_id, release);

  bool ran1 = false;
  scheduler()->ScheduleTask(Scheduler::Task(
      sequence_id1, GetClosure([&] { ran1 = true; }), {sync_token}));

  // Release task is scheduled after wait task so release is treated as non-
  // existent.
  bool ran2 = false;
  scheduler()->ScheduleTask(Scheduler::Task(sequence_id2, GetClosure([&] {
                                              release_state->ReleaseFenceSync(
                                                  release);
                                              ran2 = true;
                                            }),
                                            std::vector<SyncToken>()));

  task_runner()->RunPendingTasks();
  EXPECT_TRUE(ran1);
  EXPECT_FALSE(ran2);
  EXPECT_FALSE(sync_point_manager()->IsSyncTokenReleased(sync_token));

  task_runner()->RunPendingTasks();
  EXPECT_TRUE(ran2);
  EXPECT_TRUE(sync_point_manager()->IsSyncTokenReleased(sync_token));

  release_state->Destroy();
}

TEST_F(SchedulerTest, ReleaseSequenceIsPrioritized) {
  SequenceId sequence_id1 =
      scheduler()->CreateSequence(SchedulingPriority::kNormal);

  bool ran1 = false;
  scheduler()->ScheduleTask(Scheduler::Task(sequence_id1,
                                            GetClosure([&] { ran1 = true; }),
                                            std::vector<SyncToken>()));

  SequenceId sequence_id2 =
      scheduler()->CreateSequence(SchedulingPriority::kLow);
  CommandBufferNamespace namespace_id = CommandBufferNamespace::GPU_IO;
  CommandBufferId command_buffer_id = CommandBufferId::FromUnsafeValue(1);
  scoped_refptr<SyncPointClientState> release_state =
      sync_point_manager()->CreateSyncPointClientState(
          namespace_id, command_buffer_id, sequence_id2);

  uint64_t release = 1;
  bool ran2 = false;
  scheduler()->ScheduleTask(Scheduler::Task(sequence_id2, GetClosure([&] {
                                              release_state->ReleaseFenceSync(
                                                  release);
                                              ran2 = true;
                                            }),
                                            std::vector<SyncToken>()));

  bool ran3 = false;
  SyncToken sync_token(namespace_id, 0, command_buffer_id, release);
  SequenceId sequence_id3 =
      scheduler()->CreateSequence(SchedulingPriority::kHigh);
  scheduler()->ScheduleTask(Scheduler::Task(
      sequence_id3, GetClosure([&] { ran3 = true; }), {sync_token}));

  task_runner()->RunPendingTasks();
  EXPECT_FALSE(ran1);
  EXPECT_TRUE(ran2);
  EXPECT_FALSE(ran3);
  EXPECT_TRUE(sync_point_manager()->IsSyncTokenReleased(sync_token));

  task_runner()->RunPendingTasks();
  EXPECT_FALSE(ran1);
  EXPECT_TRUE(ran3);

  task_runner()->RunPendingTasks();
  EXPECT_TRUE(ran1);

  release_state->Destroy();
}

TEST_F(SchedulerTest, ReleaseSequenceShouldYield) {
  SequenceId sequence_id1 =
      scheduler()->CreateSequence(SchedulingPriority::kLow);
  CommandBufferNamespace namespace_id = CommandBufferNamespace::GPU_IO;
  CommandBufferId command_buffer_id = CommandBufferId::FromUnsafeValue(1);
  scoped_refptr<SyncPointClientState> release_state =
      sync_point_manager()->CreateSyncPointClientState(
          namespace_id, command_buffer_id, sequence_id1);

  uint64_t release = 1;
  bool ran1 = false;
  scheduler()->ScheduleTask(
      Scheduler::Task(sequence_id1, GetClosure([&] {
                        EXPECT_FALSE(scheduler()->ShouldYield(sequence_id1));
                        release_state->ReleaseFenceSync(release);
                        EXPECT_TRUE(scheduler()->ShouldYield(sequence_id1));
                        ran1 = true;
                      }),
                      std::vector<SyncToken>()));

  bool ran2 = false;
  SyncToken sync_token(namespace_id, 0, command_buffer_id, release);
  SequenceId sequence_id2 =
      scheduler()->CreateSequence(SchedulingPriority::kHigh);
  scheduler()->ScheduleTask(Scheduler::Task(
      sequence_id2, GetClosure([&] { ran2 = true; }), {sync_token}));

  task_runner()->RunPendingTasks();
  EXPECT_TRUE(ran1);
  EXPECT_FALSE(ran2);
  EXPECT_TRUE(sync_point_manager()->IsSyncTokenReleased(sync_token));

  task_runner()->RunPendingTasks();
  EXPECT_TRUE(ran2);

  release_state->Destroy();
}

TEST_F(SchedulerTest, ReentrantEnableSequenceShouldNotDeadlock) {
  SequenceId sequence_id1 =
      scheduler()->CreateSequence(SchedulingPriority::kHigh);
  CommandBufferNamespace namespace_id = CommandBufferNamespace::GPU_IO;
  CommandBufferId command_buffer_id1 = CommandBufferId::FromUnsafeValue(1);
  scoped_refptr<SyncPointClientState> release_state1 =
      sync_point_manager()->CreateSyncPointClientState(
          namespace_id, command_buffer_id1, sequence_id1);

  SequenceId sequence_id2 =
      scheduler()->CreateSequence(SchedulingPriority::kNormal);
  CommandBufferId command_buffer_id2 = CommandBufferId::FromUnsafeValue(2);
  scoped_refptr<SyncPointClientState> release_state2 =
      sync_point_manager()->CreateSyncPointClientState(
          namespace_id, command_buffer_id2, sequence_id2);

  uint64_t release = 1;
  SyncToken sync_token(namespace_id, 0, command_buffer_id2, release);

  bool ran1, ran2 = false;

  // Schedule task on sequence 2 first so that the sync token wait isn't a nop.
  // BeginProcessingOrderNumber for this task will run the EnableSequence
  // callback. This should not deadlock.
  scheduler()->ScheduleTask(Scheduler::Task(sequence_id2,
                                            GetClosure([&] { ran2 = true; }),
                                            std::vector<SyncToken>()));

  // This will run first because of the higher priority and no scheduling sync
  // token dependencies.
  scheduler()->ScheduleTask(Scheduler::Task(
      sequence_id1, GetClosure([&] {
        ran1 = true;
        release_state1->Wait(
            sync_token,
            base::Bind(&Scheduler::EnableSequence,
                       base::Unretained(scheduler()), sequence_id1));
        scheduler()->DisableSequence(sequence_id1);
      }),
      std::vector<SyncToken>()));

  task_runner()->RunPendingTasks();
  EXPECT_TRUE(ran1);
  EXPECT_FALSE(ran2);
  EXPECT_FALSE(sync_point_manager()->IsSyncTokenReleased(sync_token));

  task_runner()->RunPendingTasks();
  EXPECT_TRUE(ran2);
  EXPECT_FALSE(sync_point_manager()->IsSyncTokenReleased(sync_token));

  release_state1->Destroy();
  release_state2->Destroy();
}

TEST_F(SchedulerTest, WaitOnSelfShouldNotBlockSequence) {
  SequenceId sequence_id =
      scheduler()->CreateSequence(SchedulingPriority::kHigh);
  CommandBufferNamespace namespace_id = CommandBufferNamespace::GPU_IO;

  CommandBufferId command_buffer_id = CommandBufferId::FromUnsafeValue(1);
  scoped_refptr<SyncPointClientState> release_state =
      sync_point_manager()->CreateSyncPointClientState(
          namespace_id, command_buffer_id, sequence_id);

  // Dummy order number to avoid the wait_order_num <= processed_order_num + 1
  // check in SyncPointOrderData::ValidateReleaseOrderNum.
  sync_point_manager()->GenerateOrderNumber();

  uint64_t release = 1;
  SyncToken sync_token(namespace_id, 0, command_buffer_id, release);
  bool ran = false;
  scheduler()->ScheduleTask(Scheduler::Task(
      sequence_id, GetClosure([&]() { ran = true; }), {sync_token}));
  task_runner()->RunPendingTasks();
  EXPECT_TRUE(ran);
  EXPECT_FALSE(sync_point_manager()->IsSyncTokenReleased(sync_token));

  release_state->Destroy();
}

TEST_F(SchedulerTest, ClientWaitIsPrioritized) {
  SequenceId sequence_id1 =
      scheduler()->CreateSequence(SchedulingPriority::kNormal);

  bool ran1 = false;
  scheduler()->ScheduleTask(Scheduler::Task(sequence_id1,
                                            GetClosure([&] { ran1 = true; }),
                                            std::vector<SyncToken>()));

  SequenceId sequence_id2 =
      scheduler()->CreateSequence(SchedulingPriority::kLow);
  CommandBufferId command_buffer_id = CommandBufferId::FromUnsafeValue(1);
  bool ran2 = false;
  scheduler()->ScheduleTask(Scheduler::Task(sequence_id2,
                                            GetClosure([&] { ran2 = true; }),
                                            std::vector<SyncToken>()));

  bool ran3 = false;
  SequenceId sequence_id3 =
      scheduler()->CreateSequence(SchedulingPriority::kHigh);
  scheduler()->ScheduleTask(Scheduler::Task(sequence_id3,
                                            GetClosure([&] { ran3 = true; }),
                                            std::vector<SyncToken>()));

  scheduler()->RaisePriorityForClientWait(sequence_id2, command_buffer_id);

  task_runner()->RunPendingTasks();
  EXPECT_FALSE(ran1);
  EXPECT_TRUE(ran2);
  EXPECT_FALSE(ran3);

  task_runner()->RunPendingTasks();
  EXPECT_FALSE(ran1);
  EXPECT_TRUE(ran3);

  task_runner()->RunPendingTasks();
  EXPECT_TRUE(ran1);

  ran1 = ran2 = ran3 = false;
  scheduler()->ScheduleTask(Scheduler::Task(sequence_id1,
                                            GetClosure([&] { ran1 = true; }),
                                            std::vector<SyncToken>()));
  scheduler()->ScheduleTask(Scheduler::Task(sequence_id2,
                                            GetClosure([&] { ran2 = true; }),
                                            std::vector<SyncToken>()));
  scheduler()->ScheduleTask(Scheduler::Task(sequence_id3,
                                            GetClosure([&] { ran3 = true; }),
                                            std::vector<SyncToken>()));

  scheduler()->ResetPriorityForClientWait(sequence_id2, command_buffer_id);

  task_runner()->RunPendingTasks();
  EXPECT_FALSE(ran1);
  EXPECT_FALSE(ran2);
  EXPECT_TRUE(ran3);

  task_runner()->RunPendingTasks();
  EXPECT_TRUE(ran1);
  EXPECT_FALSE(ran2);

  task_runner()->RunPendingTasks();
  EXPECT_TRUE(ran2);
}

}  // namespace
}  // namespace gpu
