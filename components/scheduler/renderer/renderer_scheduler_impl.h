// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SCHEDULER_RENDERER_RENDERER_SCHEDULER_IMPL_H_
#define COMPONENTS_SCHEDULER_RENDERER_RENDERER_SCHEDULER_IMPL_H_

#include "base/atomicops.h"
#include "base/synchronization/lock.h"
#include "components/scheduler/child/idle_helper.h"
#include "components/scheduler/child/pollable_thread_safe_flag.h"
#include "components/scheduler/child/scheduler_helper.h"
#include "components/scheduler/renderer/deadline_task_runner.h"
#include "components/scheduler/renderer/renderer_scheduler.h"
#include "components/scheduler/scheduler_export.h"

namespace base {
namespace trace_event {
class ConvertableToTraceFormat;
}
}

namespace scheduler {

class SCHEDULER_EXPORT RendererSchedulerImpl : public RendererScheduler,
                                               public IdleHelper::Delegate {
 public:
  RendererSchedulerImpl(
      scoped_refptr<NestableSingleThreadTaskRunner> main_task_runner);
  ~RendererSchedulerImpl() override;

  // RendererScheduler implementation:
  scoped_refptr<base::SingleThreadTaskRunner> DefaultTaskRunner() override;
  scoped_refptr<SingleThreadIdleTaskRunner> IdleTaskRunner() override;
  scoped_refptr<base::SingleThreadTaskRunner> CompositorTaskRunner() override;
  scoped_refptr<base::SingleThreadTaskRunner> LoadingTaskRunner() override;
  scoped_refptr<base::SingleThreadTaskRunner> TimerTaskRunner() override;
  void WillBeginFrame(const cc::BeginFrameArgs& args) override;
  void BeginFrameNotExpectedSoon() override;
  void DidCommitFrameToCompositor() override;
  void DidHandleInputEventOnCompositorThread(
      const blink::WebInputEvent& web_input_event,
      InputEventState event_state) override;
  void DidHandleInputEventOnMainThread(
      const blink::WebInputEvent& web_input_event) override;
  void DidAnimateForInputOnCompositorThread() override;
  void OnRendererHidden() override;
  void OnRendererVisible() override;
  void OnPageLoadStarted() override;
  bool IsHighPriorityWorkAnticipated() override;
  bool ShouldYieldForHighPriorityWork() override;
  bool CanExceedIdleDeadlineIfRequired() const override;
  void AddTaskObserver(base::MessageLoop::TaskObserver* task_observer) override;
  void RemoveTaskObserver(
      base::MessageLoop::TaskObserver* task_observer) override;
  void Shutdown() override;
  void SuspendTimerQueue() override;
  void ResumeTimerQueue() override;

  SchedulerHelper* GetSchedulerHelperForTesting();
  base::TimeTicks CurrentIdleTaskDeadlineForTesting() const;

 private:
  friend class RendererSchedulerImplTest;
  friend class RendererSchedulerImplForTest;

  // Keep RendererSchedulerImpl::TaskQueueIdToString in sync with this enum.
  enum QueueId {
    IDLE_TASK_QUEUE = SchedulerHelper::TASK_QUEUE_COUNT,
    COMPOSITOR_TASK_QUEUE,
    LOADING_TASK_QUEUE,
    TIMER_TASK_QUEUE,
    // Must be the last entry.
    TASK_QUEUE_COUNT,
    FIRST_QUEUE_ID = SchedulerHelper::FIRST_QUEUE_ID,
  };

  // Keep RendererSchedulerImpl::PolicyToString in sync with this enum.
  enum class Policy {
    NORMAL,
    COMPOSITOR_PRIORITY,
    COMPOSITOR_PRIORITY_WITHOUT_TIMERS,
    TOUCHSTART_PRIORITY,
    LOADING_PRIORITY,
    // Must be the last entry.
    POLICY_COUNT,
    FIRST_POLICY = NORMAL,
  };

  class PollableNeedsUpdateFlag {
   public:
    PollableNeedsUpdateFlag(base::Lock* write_lock);
    ~PollableNeedsUpdateFlag();

    // Set the flag. May only be called if |write_lock| is held.
    void SetWhileLocked(bool value);

    // Returns true iff the flag is set to true.
    bool IsSet() const;

   private:
    base::subtle::Atomic32 flag_;
    base::Lock* write_lock_;  // Not owned.

    DISALLOW_COPY_AND_ASSIGN(PollableNeedsUpdateFlag);
  };

  // IdleHelper::Delegate implementation:
  bool CanEnterLongIdlePeriod(
      base::TimeTicks now,
      base::TimeDelta* next_long_idle_period_delay_out) override;
  void IsNotQuiescent() override {}
  void OnIdlePeriodStarted() override;
  void OnIdlePeriodEnded() override;

  void EndIdlePeriod();

  // Returns the serialized scheduler state for tracing.
  scoped_refptr<base::trace_event::ConvertableToTraceFormat> AsValue(
      base::TimeTicks optional_now) const;
  scoped_refptr<base::trace_event::ConvertableToTraceFormat> AsValueLocked(
      base::TimeTicks optional_now) const;
  static const char* TaskQueueIdToString(QueueId queue_id);
  static const char* PolicyToString(Policy policy);

  static bool ShouldPrioritizeInputEvent(
      const blink::WebInputEvent& web_input_event);

  // The time we should stay in a priority-escalated mode after an input event.
  static const int kPriorityEscalationAfterInputMillis = 100;

  // The amount of time which idle periods can continue being scheduled when the
  // renderer has been hidden, before going to sleep for good.
  static const int kEndIdleWhenHiddenDelayMillis = 10000;

  // The amount of time for which loading tasks will be prioritized over
  // other tasks during the initial page load.
  static const int kRailsInitialLoadingPrioritizationMillis = 1000;

  // For the purposes of deciding whether or not it's safe to turn timers on
  // only in idle periods, we regard the system as being as being "idle period"
  // starved if there hasn't been an idle period in the last 100 ms.
  static const int kIdlePeriodStarvationThresholdMillis = 100;

  // Schedules an immediate PolicyUpdate, if there isn't one already pending and
  // sets |policy_may_need_update_|. Note |any_thread_lock_| must be
  // locked.
  void EnsureUrgentPolicyUpdatePostedOnMainThread(
      const tracked_objects::Location& from_here);

  // Update the policy if a new signal has arrived. Must be called from the main
  // thread.
  void MaybeUpdatePolicy();

  // Locks |any_thread_lock_| and updates the scheduler policy.  May early
  // out if the policy is unchanged. Must be called from the main thread.
  void UpdatePolicy();

  // Like UpdatePolicy, except it doesn't early out.
  void ForceUpdatePolicy();

  enum class UpdateType {
    MAY_EARLY_OUT_IF_POLICY_UNCHANGED,
    FORCE_UPDATE,
  };

  // The implelemtation of UpdatePolicy & ForceUpdatePolicy.  It is allowed to
  // early out if |update_type| is MAY_EARLY_OUT_IF_POLICY_UNCHANGED.
  virtual void UpdatePolicyLocked(UpdateType update_type);

  // Returns the amount of time left in the current input escalated priority
  // policy.  Can be called from any thread.
  base::TimeDelta TimeLeftInInputEscalatedPolicy(base::TimeTicks now) const;

  // Helper for computing the new policy. |new_policy_duration| will be filled
  // with the amount of time after which the policy should be updated again. If
  // the duration is zero, a new policy update will not be scheduled. Must be
  // called with |any_thread_lock_| held. Can be called from any thread.
  Policy ComputeNewPolicy(base::TimeTicks now,
                          base::TimeDelta* new_policy_duration) const;

  // Works out if compositor tasks would be prioritized based on the current
  // input signals.  Can be called from any thread.
  bool InputSignalsSuggestCompositorPriority(base::TimeTicks now) const;

  // An input event of some sort happened, the policy may need updating.
  void UpdateForInputEventOnCompositorThread(blink::WebInputEvent::Type type,
                                             InputEventState input_event_state);

  // Returns true if there has been at least one idle period in the last
  // |kIdlePeriodStarvationThresholdMillis|.
  bool HadAnIdlePeriodRecently(base::TimeTicks now) const;

  SchedulerHelper helper_;
  IdleHelper idle_helper_;

  const scoped_refptr<base::SingleThreadTaskRunner> control_task_runner_;
  const scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner_;
  const scoped_refptr<base::SingleThreadTaskRunner> loading_task_runner_;
  const scoped_refptr<base::SingleThreadTaskRunner> timer_task_runner_;

  base::Closure update_policy_closure_;
  DeadlineTaskRunner delayed_update_policy_runner_;
  CancelableClosureHolder end_renderer_hidden_idle_period_closure_;

  // We have decided to improve thread safety at the cost of some boilerplate
  // (the accessors) for the following data members.

  struct MainThreadOnly {
    MainThreadOnly();

    Policy current_policy_;
    base::TimeTicks current_policy_expiration_time_;
    base::TimeTicks estimated_next_frame_begin_;
    int timer_queue_suspend_count_;  // TIMER_TASK_QUEUE suspended if non-zero.
    bool renderer_hidden_;
    bool was_shutdown_;
  };

  struct AnyThread {
    AnyThread();

    base::TimeTicks last_input_signal_time_;
    base::TimeTicks last_idle_period_end_time_;
    base::TimeTicks rails_loading_priority_deadline_;
    int pending_main_thread_input_event_count_;
    bool awaiting_touch_start_response_;
    bool in_idle_period_;
    bool begin_main_frame_on_critical_path_;
  };

  struct CompositorThreadOnly {
    CompositorThreadOnly();
    ~CompositorThreadOnly();

    blink::WebInputEvent::Type last_input_type_;
    scoped_ptr<base::ThreadChecker> compositor_thread_checker_;

    void CheckOnValidThread() {
#if DCHECK_IS_ON()
      // We don't actually care which thread this called from, just so long as
      // its consistent.
      if (!compositor_thread_checker_)
        compositor_thread_checker_.reset(new base::ThreadChecker());
      DCHECK(compositor_thread_checker_->CalledOnValidThread());
#endif
    }
  };

  // Don't access main_thread_only_, instead use MainThreadOnly().
  MainThreadOnly main_thread_only_;
  MainThreadOnly& MainThreadOnly() {
    helper_.CheckOnValidThread();
    return main_thread_only_;
  }
  const struct MainThreadOnly& MainThreadOnly() const {
    helper_.CheckOnValidThread();
    return main_thread_only_;
  }

  mutable base::Lock any_thread_lock_;
  // Don't access any_thread_, instead use AnyThread().
  AnyThread any_thread_;
  AnyThread& AnyThread() {
    any_thread_lock_.AssertAcquired();
    return any_thread_;
  }
  const struct AnyThread& AnyThread() const {
    any_thread_lock_.AssertAcquired();
    return any_thread_;
  }

  // Don't access compositor_thread_only_, instead use CompositorThreadOnly().
  CompositorThreadOnly compositor_thread_only_;
  CompositorThreadOnly& CompositorThreadOnly() {
    compositor_thread_only_.CheckOnValidThread();
    return compositor_thread_only_;
  }

  PollableThreadSafeFlag policy_may_need_update_;
  base::WeakPtrFactory<RendererSchedulerImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(RendererSchedulerImpl);
};

}  // namespace scheduler

#endif  // COMPONENTS_SCHEDULER_RENDERER_RENDERER_SCHEDULER_IMPL_H_
