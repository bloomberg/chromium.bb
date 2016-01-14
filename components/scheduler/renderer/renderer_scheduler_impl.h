// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SCHEDULER_RENDERER_RENDERER_SCHEDULER_IMPL_H_
#define COMPONENTS_SCHEDULER_RENDERER_RENDERER_SCHEDULER_IMPL_H_

#include "base/atomicops.h"
#include "base/macros.h"
#include "base/synchronization/lock.h"
#include "components/scheduler/base/pollable_thread_safe_flag.h"
#include "components/scheduler/child/idle_helper.h"
#include "components/scheduler/child/scheduler_helper.h"
#include "components/scheduler/renderer/deadline_task_runner.h"
#include "components/scheduler/renderer/idle_time_estimator.h"
#include "components/scheduler/renderer/render_widget_signals.h"
#include "components/scheduler/renderer/renderer_scheduler.h"
#include "components/scheduler/renderer/task_cost_estimator.h"
#include "components/scheduler/renderer/throttling_helper.h"
#include "components/scheduler/renderer/user_model.h"
#include "components/scheduler/scheduler_export.h"

namespace base {
namespace trace_event {
class ConvertableToTraceFormat;
}
}

namespace scheduler {
class RenderWidgetSchedulingState;
class ThrottlingHelper;

class SCHEDULER_EXPORT RendererSchedulerImpl
    : public RendererScheduler,
      public IdleHelper::Delegate,
      public SchedulerHelper::Observer,
      public RenderWidgetSignals::Observer {
 public:
  RendererSchedulerImpl(scoped_refptr<SchedulerTqmDelegate> main_task_runner);
  ~RendererSchedulerImpl() override;

  // RendererScheduler implementation:
  scoped_ptr<blink::WebThread> CreateMainThread() override;
  scoped_refptr<base::SingleThreadTaskRunner> DefaultTaskRunner() override;
  scoped_refptr<SingleThreadIdleTaskRunner> IdleTaskRunner() override;
  scoped_refptr<base::SingleThreadTaskRunner> CompositorTaskRunner() override;
  scoped_refptr<base::SingleThreadTaskRunner> LoadingTaskRunner() override;
  scoped_refptr<TaskQueue> TimerTaskRunner() override;
  scoped_refptr<TaskQueue> NewLoadingTaskRunner(const char* name) override;
  scoped_refptr<TaskQueue> NewTimerTaskRunner(const char* name) override;
  scoped_ptr<RenderWidgetSchedulingState> NewRenderWidgetSchedulingState()
      override;
  void WillBeginFrame(const cc::BeginFrameArgs& args) override;
  void BeginFrameNotExpectedSoon() override;
  void DidCommitFrameToCompositor() override;
  void DidHandleInputEventOnCompositorThread(
      const blink::WebInputEvent& web_input_event,
      InputEventState event_state) override;
  void DidHandleInputEventOnMainThread(
      const blink::WebInputEvent& web_input_event) override;
  void DidAnimateForInputOnCompositorThread() override;
  void OnRendererBackgrounded() override;
  void OnRendererForegrounded() override;
  void AddPendingNavigation() override;
  void RemovePendingNavigation() override;
  void OnNavigationStarted() override;
  bool IsHighPriorityWorkAnticipated() override;
  bool ShouldYieldForHighPriorityWork() override;
  bool CanExceedIdleDeadlineIfRequired() const override;
  void AddTaskObserver(base::MessageLoop::TaskObserver* task_observer) override;
  void RemoveTaskObserver(
      base::MessageLoop::TaskObserver* task_observer) override;
  void Shutdown() override;
  void SuspendTimerQueue() override;
  void ResumeTimerQueue() override;
  void SetTimerQueueSuspensionWhenBackgroundedEnabled(bool enabled) override;
  double CurrentTimeSeconds() const override;
  double MonotonicallyIncreasingTimeSeconds() const override;

  // RenderWidgetSignals::Observer implementation:
  void SetAllRenderWidgetsHidden(bool hidden) override;
  void SetHasVisibleRenderWidgetWithTouchHandler(
      bool has_visible_render_widget_with_touch_handler) override;

  // TaskQueueManager::Observer implementation:
  void OnUnregisterTaskQueue(const scoped_refptr<TaskQueue>& queue) override;

  // Returns a task runner where tasks run at the highest possible priority.
  scoped_refptr<TaskQueue> ControlTaskRunner();

  void RegisterTimeDomain(TimeDomain* time_domain);
  void UnregisterTimeDomain(TimeDomain* time_domain);

  void SetExpensiveTaskBlockingAllowed(bool allowed);

  // Test helpers.
  SchedulerHelper* GetSchedulerHelperForTesting();
  TaskCostEstimator* GetLoadingTaskCostEstimatorForTesting();
  TaskCostEstimator* GetTimerTaskCostEstimatorForTesting();
  IdleTimeEstimator* GetIdleTimeEstimatorForTesting();
  base::TimeTicks CurrentIdleTaskDeadlineForTesting() const;

  base::TickClock* tick_clock() const;

  RealTimeDomain* real_time_domain() const {
    return helper_.real_time_domain();
  }

  ThrottlingHelper* throttling_helper() { return &throttling_helper_; }

 private:
  friend class RendererSchedulerImplTest;
  friend class RendererSchedulerImplForTest;
  friend class RenderWidgetSchedulingState;

  struct Policy {
    Policy();

    TaskQueue::QueuePriority compositor_queue_priority;
    TaskQueue::QueuePriority loading_queue_priority;
    TaskQueue::QueuePriority timer_queue_priority;
    TaskQueue::QueuePriority default_queue_priority;

    bool operator==(const Policy& other) const {
      return compositor_queue_priority == other.compositor_queue_priority &&
             loading_queue_priority == other.loading_queue_priority &&
             timer_queue_priority == other.timer_queue_priority &&
             default_queue_priority == other.default_queue_priority;
    }
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

  static bool ShouldPrioritizeInputEvent(
      const blink::WebInputEvent& web_input_event);

  // The amount of time which idle periods can continue being scheduled when the
  // renderer has been hidden, before going to sleep for good.
  static const int kEndIdleWhenHiddenDelayMillis = 10000;

  // The amount of time for which loading tasks will be prioritized over
  // other tasks during the initial page load.
  static const int kRailsInitialLoadingPrioritizationMillis = 1000;

  // The amount of time in milliseconds we have to respond to user input as
  // defined by RAILS.
  static const int kRailsResponseTimeMillis = 50;

  // For the purposes of deciding whether or not it's safe to turn timers and
  // loading tasks on only in idle periods, we regard the system as being as
  // being "idle period" starved if there hasn't been an idle period in the last
  // 10 seconds. This was chosen to be long enough to cover most anticipated
  // user gestures.
  static const int kIdlePeriodStarvationThresholdMillis = 10000;

  // The amount of time to wait before suspending shared timers after the
  // renderer has been backgrounded. This is used only if background suspension
  // of shared timers is enabled.
  static const int kSuspendTimersWhenBackgroundedDelayMillis = 5 * 60 * 1000;

  // The time we should stay in a priority-escalated mode after a call to
  // DidAnimateForInputOnCompositorThread().
  static const int kFlingEscalationLimitMillis = 100;

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

  // Helper for computing the use case. |expected_usecase_duration| will be
  // filled with the amount of time after which the use case should be updated
  // again. If the duration is zero, a new use case update should not be
  // scheduled. Must be called with |any_thread_lock_| held. Can be called from
  // any thread.
  UseCase ComputeCurrentUseCase(
      base::TimeTicks now,
      base::TimeDelta* expected_use_case_duration) const;

  // Works out if a gesture appears to be in progress based on the current
  // input signals. Can be called from any thread.
  bool InputSignalsSuggestGestureInProgress(base::TimeTicks now) const;

  // An input event of some sort happened, the policy may need updating.
  void UpdateForInputEventOnCompositorThread(blink::WebInputEvent::Type type,
                                             InputEventState input_event_state);

  // Returns true if there has been at least one idle period in the last
  // |kIdlePeriodStarvationThresholdMillis|.
  bool HadAnIdlePeriodRecently(base::TimeTicks now) const;

  // Helpers for safely suspending/resuming the timer queue after a
  // background/foreground signal.
  void SuspendTimerQueueWhenBackgrounded();
  void ResumeTimerQueueWhenForegrounded();

  // The task cost estimators and the UserModel need to be reset upon page
  // nagigation. This function does that. Must be called from the main thread.
  void ResetForNavigationLocked();

  SchedulerHelper helper_;
  IdleHelper idle_helper_;
  ThrottlingHelper throttling_helper_;
  RenderWidgetSignals render_widget_scheduler_signals_;

  const scoped_refptr<TaskQueue> control_task_runner_;
  const scoped_refptr<TaskQueue> compositor_task_runner_;
  std::set<scoped_refptr<TaskQueue>> loading_task_runners_;
  std::set<scoped_refptr<TaskQueue>> timer_task_runners_;
  scoped_refptr<TaskQueue> default_loading_task_runner_;
  scoped_refptr<TaskQueue> default_timer_task_runner_;

  base::Closure update_policy_closure_;
  DeadlineTaskRunner delayed_update_policy_runner_;
  CancelableClosureHolder end_renderer_hidden_idle_period_closure_;
  CancelableClosureHolder suspend_timers_when_backgrounded_closure_;

  // We have decided to improve thread safety at the cost of some boilerplate
  // (the accessors) for the following data members.

  struct MainThreadOnly {
    MainThreadOnly(const scoped_refptr<TaskQueue>& compositor_task_runner,
                   base::TickClock* time_source);
    ~MainThreadOnly();

    TaskCostEstimator loading_task_cost_estimator;
    TaskCostEstimator timer_task_cost_estimator;
    IdleTimeEstimator idle_time_estimator;
    UseCase current_use_case;
    Policy current_policy;
    base::TimeTicks current_policy_expiration_time;
    base::TimeTicks estimated_next_frame_begin;
    base::TimeDelta compositor_frame_interval;
    base::TimeDelta expected_idle_duration;
    int timer_queue_suspend_count;  // TIMER_TASK_QUEUE suspended if non-zero.
    int navigation_task_expected_count;
    bool renderer_hidden;
    bool renderer_backgrounded;
    bool timer_queue_suspension_when_backgrounded_enabled;
    bool timer_queue_suspended_when_backgrounded;
    bool was_shutdown;
    bool loading_tasks_seem_expensive;
    bool timer_tasks_seem_expensive;
    bool touchstart_expected_soon;
    bool have_seen_a_begin_main_frame;
    bool has_visible_render_widget_with_touch_handler;
    bool begin_frame_not_expected_soon;
    bool expensive_task_blocking_allowed;
  };

  struct AnyThread {
    AnyThread();
    ~AnyThread();

    base::TimeTicks last_idle_period_end_time;
    base::TimeTicks rails_loading_priority_deadline;
    base::TimeTicks fling_compositor_escalation_deadline;
    UserModel user_model;
    bool awaiting_touch_start_response;
    bool in_idle_period;
    bool begin_main_frame_on_critical_path;
    bool last_gesture_was_compositor_driven;
    bool have_seen_touchstart;
  };

  struct CompositorThreadOnly {
    CompositorThreadOnly();
    ~CompositorThreadOnly();

    blink::WebInputEvent::Type last_input_type;
    scoped_ptr<base::ThreadChecker> compositor_thread_checker;

    void CheckOnValidThread() {
#if DCHECK_IS_ON()
      // We don't actually care which thread this called from, just so long as
      // its consistent.
      if (!compositor_thread_checker)
        compositor_thread_checker.reset(new base::ThreadChecker());
      DCHECK(compositor_thread_checker->CalledOnValidThread());
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
