// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_SCHEDULER_RENDERER_SCHEDULER_IMPL_H_
#define CONTENT_RENDERER_SCHEDULER_RENDERER_SCHEDULER_IMPL_H_

#include "base/atomicops.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_checker.h"
#include "cc/test/test_now_source.h"
#include "content/renderer/scheduler/cancelable_closure_holder.h"
#include "content/renderer/scheduler/renderer_scheduler.h"
#include "content/renderer/scheduler/single_thread_idle_task_runner.h"
#include "content/renderer/scheduler/task_queue_manager.h"

namespace base {
namespace trace_event {
class ConvertableToTraceFormat;
}
}

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
  scoped_refptr<base::SingleThreadTaskRunner> LoadingTaskRunner() override;
  void WillBeginFrame(const cc::BeginFrameArgs& args) override;
  void BeginFrameNotExpectedSoon() override;
  void DidCommitFrameToCompositor() override;
  void DidReceiveInputEventOnCompositorThread(
      const blink::WebInputEvent& web_input_event) override;
  void DidAnimateForInputOnCompositorThread() override;
  bool IsHighPriorityWorkAnticipated() override;
  bool ShouldYieldForHighPriorityWork() override;
  void Shutdown() override;

  void SetTimeSourceForTesting(scoped_refptr<cc::TestNowSource> time_source);

 private:
  friend class RendererSchedulerImplTest;

  // Keep RendererSchedulerImpl::TaskQueueIdToString in sync with this enum.
  enum QueueId {
    DEFAULT_TASK_QUEUE,
    COMPOSITOR_TASK_QUEUE,
    IDLE_TASK_QUEUE,
    CONTROL_TASK_QUEUE,
    LOADING_TASK_QUEUE,
    // Must be the last entry.
    TASK_QUEUE_COUNT,
  };

  enum Policy {
    NORMAL_PRIORITY_POLICY,
    COMPOSITOR_PRIORITY_POLICY,
    TOUCHSTART_PRIORITY_POLICY,
  };

  enum InputStreamState {
    INPUT_INACTIVE,
    INPUT_ACTIVE,
    INPUT_ACTIVE_AND_AWAITING_TOUCHSTART_RESPONSE
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

    DISALLOW_COPY_AND_ASSIGN(PollableNeedsUpdateFlag);
  };

  // Returns the serialized scheduler state for tracing.
  scoped_refptr<base::trace_event::ConvertableToTraceFormat> AsValueLocked(
      base::TimeTicks optional_now) const;
  static const char* TaskQueueIdToString(QueueId queue_id);
  static const char* PolicyToString(Policy policy);
  static const char* InputStreamStateToString(InputStreamState state);

  static InputStreamState ComputeNewInputStreamState(
      InputStreamState current_state,
      blink::WebInputEvent::Type new_input_event,
      blink::WebInputEvent::Type last_input_event);

  // The time we should stay in a priority-escalated mode after an input event.
  static const int kPriorityEscalationAfterInputMillis = 100;

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

  // An input event of some sort happened, the policy may need updating.
  void UpdateForInputEvent(blink::WebInputEvent::Type type);

  // Start and end an idle period.
  void StartIdlePeriod();
  void EndIdlePeriod();

  base::TimeTicks Now() const;

  base::ThreadChecker main_thread_checker_;
  scoped_ptr<RendererTaskQueueSelector> renderer_task_queue_selector_;
  scoped_ptr<TaskQueueManager> task_queue_manager_;
  scoped_refptr<base::SingleThreadTaskRunner> control_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> default_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> loading_task_runner_;
  scoped_refptr<SingleThreadIdleTaskRunner> idle_task_runner_;

  base::Closure update_policy_closure_;
  CancelableClosureHolder end_idle_period_closure_;

  // Don't access current_policy_ directly, instead use SchedulerPolicy().
  Policy current_policy_;

  base::TimeTicks estimated_next_frame_begin_;

  // The incoming_signals_lock_ mutex protects access to all variables in the
  // (contiguous) block below.
  base::Lock incoming_signals_lock_;
  base::TimeTicks last_input_time_;
  blink::WebInputEvent::Type last_input_type_;
  InputStreamState input_stream_state_;
  PollableNeedsUpdateFlag policy_may_need_update_;

  scoped_refptr<cc::TestNowSource> time_source_;

  base::WeakPtr<RendererSchedulerImpl> weak_renderer_scheduler_ptr_;
  base::WeakPtrFactory<RendererSchedulerImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(RendererSchedulerImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_SCHEDULER_RENDERER_SCHEDULER_IMPL_H_
