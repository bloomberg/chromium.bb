// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_SCHEDULER_RENDERER_SCHEDULER_IMPL_H_
#define CONTENT_RENDERER_SCHEDULER_RENDERER_SCHEDULER_IMPL_H_

#include "base/atomicops.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_checker.h"
#include "content/renderer/scheduler/renderer_scheduler.h"
#include "content/renderer/scheduler/single_thread_idle_task_runner.h"
#include "content/renderer/scheduler/task_queue_manager.h"

namespace content {

class RendererTaskQueueSelector;

class CONTENT_EXPORT RendererSchedulerImpl : public RendererScheduler {
 public:
  RendererSchedulerImpl(
      scoped_refptr<base::SingleThreadTaskRunner> main_task_runner);
  ~RendererSchedulerImpl() override;

  // RendererScheduler implementation:
  scoped_refptr<base::SingleThreadTaskRunner> DefaultTaskRunner() override;
  scoped_refptr<base::SingleThreadTaskRunner> CompositorTaskRunner() override;
  scoped_refptr<SingleThreadIdleTaskRunner> IdleTaskRunner() override;
  void WillBeginFrame(const cc::BeginFrameArgs& args) override;
  void DidCommitFrameToCompositor() override;
  void DidReceiveInputEventOnCompositorThread() override;
  bool ShouldYieldForHighPriorityWork() override;
  void Shutdown() override;

 protected:
  // Virtual for testing.
  virtual base::TimeTicks Now() const;

 private:
  friend class RendererSchedulerImplTest;

  enum QueueId {
    DEFAULT_TASK_QUEUE,
    COMPOSITOR_TASK_QUEUE,
    IDLE_TASK_QUEUE,
    CONTROL_TASK_QUEUE,
    // Must be the last entry.
    TASK_QUEUE_COUNT,
  };

  enum Policy {
    NORMAL_PRIORITY_POLICY,
    COMPOSITOR_PRIORITY_POLICY,
  };

  class PollableNeedsUpdateFlag {
   public:
    PollableNeedsUpdateFlag(base::Lock* write_lock);
    ~PollableNeedsUpdateFlag();

    // Set the flag. May only be called if |write_lock| is held.
    void SetLocked(bool value);

    // Returns true iff the flag was set to true.
    bool IsSet() const;

   private:
    base::subtle::Atomic32 flag_;
    base::Lock* write_lock_;  // Not owned.
    base::ThreadChecker thread_checker_;

    DISALLOW_COPY_AND_ASSIGN(PollableNeedsUpdateFlag);
  };

  // The time we should stay in CompositorPriority mode for after a touch event.
  static const int kCompositorPriorityAfterTouchMillis = 100;

  // IdleTaskDeadlineSupplier Implementation:
  void CurrentIdleTaskDeadlineCallback(base::TimeTicks* deadline_out) const;

  // Returns the current scheduler policy. Must be called from the main thread.
  Policy SchedulerPolicy() const;

  // Posts a call to UpdatePolicy on the control runner to be run after |delay|
  void PostUpdatePolicyOnControlRunner(base::TimeDelta delay);

  // Update the policy if a new signal has arrived. Must be called from the main
  // thread.
  void MaybeUpdatePolicy();

  // Updates the scheduler policy. Must be called from the main thread.
  void UpdatePolicy();

  // Start and end an idle period.
  void StartIdlePeriod();
  void EndIdlePeriod();

  base::ThreadChecker main_thread_checker_;
  scoped_ptr<RendererTaskQueueSelector> renderer_task_queue_selector_;
  scoped_ptr<TaskQueueManager> task_queue_manager_;
  scoped_refptr<base::SingleThreadTaskRunner> control_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> default_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner_;
  scoped_refptr<SingleThreadIdleTaskRunner> idle_task_runner_;

  base::Closure update_policy_closure_;
  base::Closure end_idle_period_closure_;

  // Don't access current_policy_ directly, instead use SchedulerPolicy().
  Policy current_policy_;

  base::TimeTicks estimated_next_frame_begin_;

  // The incoming_signals_lock_ mutex protects access to last_input_time_
  // and write access to policy_may_need_update_.
  base::Lock incoming_signals_lock_;
  base::TimeTicks last_input_time_;
  PollableNeedsUpdateFlag policy_may_need_update_;

  base::WeakPtr<RendererSchedulerImpl> weak_renderer_scheduler_ptr_;
  base::WeakPtrFactory<RendererSchedulerImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(RendererSchedulerImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_SCHEDULER_RENDERER_SCHEDULER_IMPL_H_
