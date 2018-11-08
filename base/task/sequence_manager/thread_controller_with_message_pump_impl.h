// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TASK_SEQUENCE_MANAGER_THREAD_CONTROLLER_WITH_MESSAGE_PUMP_IMPL_H_
#define BASE_TASK_SEQUENCE_MANAGER_THREAD_CONTROLLER_WITH_MESSAGE_PUMP_IMPL_H_

#include <memory>

#include "base/debug/task_annotator.h"
#include "base/message_loop/message_pump.h"
#include "base/optional.h"
#include "base/task/sequence_manager/associated_thread_id.h"
#include "base/task/sequence_manager/moveable_auto_lock.h"
#include "base/task/sequence_manager/sequenced_task_source.h"
#include "base/task/sequence_manager/thread_controller.h"
#include "base/threading/platform_thread.h"
#include "base/threading/sequence_local_storage_map.h"
#include "base/threading/thread_task_runner_handle.h"

namespace base {
namespace sequence_manager {
namespace internal {

// EXPERIMENTAL ThreadController implementation which doesn't use
// MessageLoop or a task runner to schedule their DoWork calls.
// See https://crbug.com/828835.
class BASE_EXPORT ThreadControllerWithMessagePumpImpl
    : public ThreadController,
      public MessagePump::Delegate,
      public RunLoop::Delegate,
      public RunLoop::NestingObserver {
 public:
  ThreadControllerWithMessagePumpImpl(std::unique_ptr<MessagePump> message_pump,
                                      const TickClock* time_source);
  ~ThreadControllerWithMessagePumpImpl() override;

  static std::unique_ptr<ThreadControllerWithMessagePumpImpl> CreateUnbound(
      const TickClock* time_source);

  // ThreadController implementation:
  void SetSequencedTaskSource(SequencedTaskSource* task_source) override;
  void BindToCurrentThread(MessageLoop* message_loop) override;
  void BindToCurrentThread(std::unique_ptr<MessagePump> message_pump) override;
  void SetWorkBatchSize(int work_batch_size) override;
  void WillQueueTask(PendingTask* pending_task) override;
  void ScheduleWork() override;
  void SetNextDelayedDoWork(LazyNow* lazy_now, TimeTicks run_time) override;
  void SetTimerSlack(TimerSlack timer_slack) override;
  const TickClock* GetClock() override;
  bool RunsTasksInCurrentSequence() override;
  void SetDefaultTaskRunner(
      scoped_refptr<SingleThreadTaskRunner> task_runner) override;
  void RestoreDefaultTaskRunner() override;
  void AddNestingObserver(RunLoop::NestingObserver* observer) override;
  void RemoveNestingObserver(RunLoop::NestingObserver* observer) override;
  const scoped_refptr<AssociatedThreadId>& GetAssociatedThread() const override;
  void SetTaskExecutionAllowed(bool allowed) override;
  bool IsTaskExecutionAllowed() const override;
  MessagePump* GetBoundMessagePump() const override;

  // RunLoop::NestingObserver:
  void OnBeginNestedRunLoop() override;
  void OnExitNestedRunLoop() override;

 protected:
  explicit ThreadControllerWithMessagePumpImpl(const TickClock* time_source);

  // MessagePump::Delegate implementation.
  bool DoWork() override;
  bool DoDelayedWork(TimeTicks* next_run_time) override;
  bool DoIdleWork() override;

 private:
  friend class DoWorkScope;
  friend class RunScope;

  // RunLoop::Delegate implementation.
  void Run(bool application_tasks_allowed) override;
  void Quit() override;
  void EnsureWorkScheduled() override;

  bool DoWorkImpl(base::TimeTicks* next_run_time);

  bool InTopLevelDoWork() const;

  struct MainThreadOnly {
    MainThreadOnly();
    ~MainThreadOnly();

    SequencedTaskSource* task_source = nullptr;            // Not owned.
    RunLoop::NestingObserver* nesting_observer = nullptr;  // Not owned.
    std::unique_ptr<ThreadTaskRunnerHandle> thread_task_runner_handle;

    // Indicates that we should yield DoWork ASAP.
    bool quit_do_work = false;

    // Whether high resolution timing is enabled or not.
    bool in_high_res_mode = false;

    // Used to prevent redundant calls to ScheduleWork / ScheduleDelayedWork.
    bool immediate_do_work_posted = false;

    // Number of tasks processed in a single DoWork invocation.
    int work_batch_size = 1;

    // Number of DoWorks on the stack. Must be >= |nesting_depth|.
    int do_work_running_count = 0;

    // Number of nested RunLoops on the stack.
    int nesting_depth = 0;

    // When the next scheduled delayed work should run, if any.
    TimeTicks next_delayed_do_work = TimeTicks::Max();

    bool task_execution_allowed = true;
  };

  MainThreadOnly& main_thread_only() {
    DCHECK_CALLED_ON_VALID_THREAD(associated_thread_->thread_checker);
    return main_thread_only_;
  }

  const MainThreadOnly& main_thread_only() const {
    DCHECK_CALLED_ON_VALID_THREAD(associated_thread_->thread_checker);
    return main_thread_only_;
  }

  // Acquires a |pump_lock_| if necessary when we want to read |pump_|.
  // Non-main thread read access should be guarded by a lock,
  // while main-thread access can be lock-free. Write access is possible
  // only from the main thread and needs a lock.
  Optional<MoveableAutoLock> AcquirePumpReadLockIfNeeded();

  // TODO(altimin): Merge with the one in SequenceManager.
  scoped_refptr<AssociatedThreadId> associated_thread_;
  MainThreadOnly main_thread_only_;

  // Protects |pump_| and |should_schedule_work_after_bind_| as work can be
  // scheduled from another thread before we create the pump.
  // TODO(altimin, crbug.com/901345): Remove this lock.
  base::Lock pump_lock_;
  std::unique_ptr<MessagePump> pump_;
  bool should_schedule_work_after_bind_ = false;

  debug::TaskAnnotator task_annotator_;
  const TickClock* time_source_;  // Not owned.
  scoped_refptr<SingleThreadTaskRunner> task_runner_to_set_;

  // Required to register the current thread as a sequence.
  base::internal::SequenceLocalStorageMap sequence_local_storage_map_;
  std::unique_ptr<
      base::internal::ScopedSetSequenceLocalStorageMapForCurrentThread>
      scoped_set_sequence_local_storage_map_for_current_thread_;

  DISALLOW_COPY_AND_ASSIGN(ThreadControllerWithMessagePumpImpl);
};

}  // namespace internal
}  // namespace sequence_manager
}  // namespace base

#endif  // BASE_TASK_SEQUENCE_MANAGER_THREAD_CONTROLLER_WITH_MESSAGE_PUMP_IMPL_H_
