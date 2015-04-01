// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/scheduler/renderer_scheduler_impl.h"

#include "base/bind.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_event_argument.h"
#include "cc/output/begin_frame_args.h"
#include "content/child/scheduler/nestable_single_thread_task_runner.h"
#include "content/child/scheduler/prioritizing_task_queue_selector.h"
#include "ui/gfx/frame_time.h"

namespace content {

RendererSchedulerImpl::RendererSchedulerImpl(
    scoped_refptr<NestableSingleThreadTaskRunner> main_task_runner)
    : helper_(main_task_runner,
              this,
              "renderer.scheduler",
              TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
              TASK_QUEUE_COUNT),
      control_task_runner_(helper_.ControlTaskRunner()),
      compositor_task_runner_(
          helper_.SchedulerTaskQueueManager()->TaskRunnerForQueue(
              COMPOSITOR_TASK_QUEUE)),
      loading_task_runner_(
          helper_.SchedulerTaskQueueManager()->TaskRunnerForQueue(
              LOADING_TASK_QUEUE)),
      delayed_update_policy_runner_(
          base::Bind(&RendererSchedulerImpl::UpdatePolicy,
                     base::Unretained(this)),
          helper_.ControlTaskRunner()),
      current_policy_(Policy::NORMAL),
      last_input_type_(blink::WebInputEvent::Undefined),
      input_stream_state_(InputStreamState::INACTIVE),
      policy_may_need_update_(&incoming_signals_lock_),
      weak_factory_(this) {
  update_policy_closure_ = base::Bind(&RendererSchedulerImpl::UpdatePolicy,
                                      weak_factory_.GetWeakPtr());
  for (size_t i = SchedulerHelper::TASK_QUEUE_COUNT;
       i < TASK_QUEUE_COUNT;
       i++) {
    helper_.SchedulerTaskQueueManager()->SetQueueName(
        i, TaskQueueIdToString(static_cast<QueueId>(i)));
  }
  TRACE_EVENT_OBJECT_CREATED_WITH_ID(
      TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"), "RendererScheduler",
      this);
}

RendererSchedulerImpl::~RendererSchedulerImpl() {
  TRACE_EVENT_OBJECT_DELETED_WITH_ID(
      TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"), "RendererScheduler",
      this);
}

void RendererSchedulerImpl::Shutdown() {
  helper_.Shutdown();
}

scoped_refptr<base::SingleThreadTaskRunner>
RendererSchedulerImpl::DefaultTaskRunner() {
  return helper_.DefaultTaskRunner();
}

scoped_refptr<base::SingleThreadTaskRunner>
RendererSchedulerImpl::CompositorTaskRunner() {
  helper_.CheckOnValidThread();
  return compositor_task_runner_;
}

scoped_refptr<SingleThreadIdleTaskRunner>
RendererSchedulerImpl::IdleTaskRunner() {
  return helper_.IdleTaskRunner();
}

scoped_refptr<base::SingleThreadTaskRunner>
RendererSchedulerImpl::LoadingTaskRunner() {
  helper_.CheckOnValidThread();
  return loading_task_runner_;
}

bool RendererSchedulerImpl::CanExceedIdleDeadlineIfRequired() const {
  return helper_.CanExceedIdleDeadlineIfRequired();
}

void RendererSchedulerImpl::AddTaskObserver(
    base::MessageLoop::TaskObserver* task_observer) {
  helper_.AddTaskObserver(task_observer);
}

void RendererSchedulerImpl::RemoveTaskObserver(
    base::MessageLoop::TaskObserver* task_observer) {
  helper_.RemoveTaskObserver(task_observer);
}

void RendererSchedulerImpl::WillBeginFrame(const cc::BeginFrameArgs& args) {
  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
               "RendererSchedulerImpl::WillBeginFrame", "args", args.AsValue());
  helper_.CheckOnValidThread();
  if (helper_.IsShutdown())
    return;

  helper_.EndIdlePeriod();
  estimated_next_frame_begin_ = args.frame_time + args.interval;
  // TODO(skyostil): Wire up real notification of input events processing
  // instead of this approximation.
  DidProcessInputEvent(args.frame_time);
}

void RendererSchedulerImpl::DidCommitFrameToCompositor() {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
               "RendererSchedulerImpl::DidCommitFrameToCompositor");
  helper_.CheckOnValidThread();
  if (helper_.IsShutdown())
    return;

  base::TimeTicks now(helper_.Now());
  if (now < estimated_next_frame_begin_) {
    // TODO(rmcilroy): Consider reducing the idle period based on the runtime of
    // the next pending delayed tasks (as currently done in for long idle times)
    helper_.StartIdlePeriod(
        SchedulerHelper::IdlePeriodState::IN_SHORT_IDLE_PERIOD,
        now,
        estimated_next_frame_begin_,
        true);
  }
}

void RendererSchedulerImpl::BeginFrameNotExpectedSoon() {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
               "RendererSchedulerImpl::BeginFrameNotExpectedSoon");
  helper_.CheckOnValidThread();
  if (helper_.IsShutdown())
    return;

  // TODO(skyostil): Wire up real notification of input events processing
  // instead of this approximation.
  DidProcessInputEvent(base::TimeTicks());

  helper_.InitiateLongIdlePeriod();
}

void RendererSchedulerImpl::DidReceiveInputEventOnCompositorThread(
    const blink::WebInputEvent& web_input_event) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
               "RendererSchedulerImpl::DidReceiveInputEventOnCompositorThread");
  // We regard MouseMove events with the left mouse button down as a signal
  // that the user is doing something requiring a smooth frame rate.
  if (web_input_event.type == blink::WebInputEvent::MouseMove &&
      (web_input_event.modifiers & blink::WebInputEvent::LeftButtonDown)) {
    UpdateForInputEvent(web_input_event.type);
    return;
  }
  // Ignore all other mouse events because they probably don't signal user
  // interaction needing a smooth framerate. NOTE isMouseEventType returns false
  // for mouse wheel events, hence we regard them as user input.
  // Ignore keyboard events because it doesn't really make sense to enter
  // compositor priority for them.
  if (blink::WebInputEvent::isMouseEventType(web_input_event.type) ||
      blink::WebInputEvent::isKeyboardEventType(web_input_event.type)) {
    return;
  }
  UpdateForInputEvent(web_input_event.type);
}

void RendererSchedulerImpl::DidAnimateForInputOnCompositorThread() {
  UpdateForInputEvent(blink::WebInputEvent::Undefined);
}

void RendererSchedulerImpl::UpdateForInputEvent(
    blink::WebInputEvent::Type type) {
  base::AutoLock lock(incoming_signals_lock_);

  InputStreamState new_input_stream_state =
      ComputeNewInputStreamState(input_stream_state_, type, last_input_type_);

  if (input_stream_state_ != new_input_stream_state) {
    // Update scheduler policy if we should start a new policy mode.
    input_stream_state_ = new_input_stream_state;
    EnsureUrgentPolicyUpdatePostedOnMainThread(FROM_HERE);
  }
  last_input_receipt_time_on_compositor_ = helper_.Now();
  // Clear the last known input processing time so that we know an input event
  // is still queued up. This timestamp will be updated the next time the
  // compositor commits or becomes quiescent. Note that this function is always
  // called before the input event is processed either on the compositor or
  // main threads.
  last_input_process_time_on_main_ = base::TimeTicks();
  last_input_type_ = type;
}

void RendererSchedulerImpl::DidProcessInputEvent(
    base::TimeTicks begin_frame_time) {
  helper_.CheckOnValidThread();
  base::AutoLock lock(incoming_signals_lock_);
  if (input_stream_state_ == InputStreamState::INACTIVE)
    return;
  // Avoid marking input that arrived after the BeginFrame as processed.
  if (!begin_frame_time.is_null() &&
      begin_frame_time < last_input_receipt_time_on_compositor_)
    return;
  last_input_process_time_on_main_ = helper_.Now();
  UpdatePolicyLocked();
}

bool RendererSchedulerImpl::IsHighPriorityWorkAnticipated() {
  helper_.CheckOnValidThread();
  if (helper_.IsShutdown())
    return false;

  MaybeUpdatePolicy();
  // The touchstart and compositor policies indicate a strong likelihood of
  // high-priority work in the near future.
  return SchedulerPolicy() == Policy::COMPOSITOR_PRIORITY ||
         SchedulerPolicy() == Policy::TOUCHSTART_PRIORITY;
}

bool RendererSchedulerImpl::ShouldYieldForHighPriorityWork() {
  helper_.CheckOnValidThread();
  if (helper_.IsShutdown())
    return false;

  MaybeUpdatePolicy();
  // We only yield if we are in the compositor priority and there is compositor
  // work outstanding, or if we are in the touchstart response priority.
  // Note: even though the control queue is higher priority we don't yield for
  // it since these tasks are not user-provided work and they are only intended
  // to run before the next task, not interrupt the tasks.
  switch (SchedulerPolicy()) {
    case Policy::NORMAL:
      return false;

    case Policy::COMPOSITOR_PRIORITY:
      return !helper_.SchedulerTaskQueueManager()->IsQueueEmpty(
          COMPOSITOR_TASK_QUEUE);

    case Policy::TOUCHSTART_PRIORITY:
      return true;

    default:
      NOTREACHED();
      return false;
  }
}

base::TimeTicks
RendererSchedulerImpl::CurrentIdleTaskDeadlineForTesting() const {
  base::TimeTicks deadline;
  helper_.CurrentIdleTaskDeadlineCallback(&deadline);
  return deadline;
}

RendererSchedulerImpl::Policy RendererSchedulerImpl::SchedulerPolicy() const {
  helper_.CheckOnValidThread();
  return current_policy_;
}

void RendererSchedulerImpl::MaybeUpdatePolicy() {
  helper_.CheckOnValidThread();
  if (policy_may_need_update_.IsSet()) {
    UpdatePolicy();
  }
}

void RendererSchedulerImpl::EnsureUrgentPolicyUpdatePostedOnMainThread(
    const tracked_objects::Location& from_here) {
  // TODO(scheduler-dev): Check that this method isn't called from the main
  // thread.
  incoming_signals_lock_.AssertAcquired();
  if (!policy_may_need_update_.IsSet()) {
    policy_may_need_update_.SetWhileLocked(true);
    control_task_runner_->PostTask(from_here, update_policy_closure_);
  }
}

void RendererSchedulerImpl::UpdatePolicy() {
  base::AutoLock lock(incoming_signals_lock_);
  UpdatePolicyLocked();
}

void RendererSchedulerImpl::UpdatePolicyLocked() {
  helper_.CheckOnValidThread();
  incoming_signals_lock_.AssertAcquired();
  if (helper_.IsShutdown())
    return;

  base::TimeTicks now = helper_.Now();
  policy_may_need_update_.SetWhileLocked(false);

  base::TimeDelta new_policy_duration;
  Policy new_policy = ComputeNewPolicy(now, &new_policy_duration);
  if (new_policy_duration > base::TimeDelta()) {
    current_policy_expiration_time_ = now + new_policy_duration;
    delayed_update_policy_runner_.SetDeadline(FROM_HERE, new_policy_duration,
                                              now);
  } else {
    current_policy_expiration_time_ = base::TimeTicks();
  }

  if (new_policy == current_policy_)
    return;

  PrioritizingTaskQueueSelector* task_queue_selector =
      helper_.SchedulerTaskQueueSelector();

  switch (new_policy) {
    case Policy::COMPOSITOR_PRIORITY:
      task_queue_selector->SetQueuePriority(
          COMPOSITOR_TASK_QUEUE, PrioritizingTaskQueueSelector::HIGH_PRIORITY);
      // TODO(scheduler-dev): Add a task priority between HIGH and BEST_EFFORT
      // that still has some guarantee of running.
      task_queue_selector->SetQueuePriority(
          LOADING_TASK_QUEUE,
          PrioritizingTaskQueueSelector::BEST_EFFORT_PRIORITY);
      break;
    case Policy::TOUCHSTART_PRIORITY:
      task_queue_selector->SetQueuePriority(
          COMPOSITOR_TASK_QUEUE, PrioritizingTaskQueueSelector::HIGH_PRIORITY);
      task_queue_selector->DisableQueue(LOADING_TASK_QUEUE);
      break;
    case Policy::NORMAL:
      task_queue_selector->SetQueuePriority(
          COMPOSITOR_TASK_QUEUE,
          PrioritizingTaskQueueSelector::NORMAL_PRIORITY);
      task_queue_selector->SetQueuePriority(
          LOADING_TASK_QUEUE, PrioritizingTaskQueueSelector::NORMAL_PRIORITY);
      break;
  }
  DCHECK(task_queue_selector->IsQueueEnabled(COMPOSITOR_TASK_QUEUE));
  if (new_policy != Policy::TOUCHSTART_PRIORITY)
    DCHECK(task_queue_selector->IsQueueEnabled(LOADING_TASK_QUEUE));

  current_policy_ = new_policy;

  TRACE_EVENT_OBJECT_SNAPSHOT_WITH_ID(
      TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"), "RendererScheduler",
      this, AsValueLocked(now));
  TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
                 "RendererScheduler.policy", current_policy_);
}

RendererSchedulerImpl::Policy RendererSchedulerImpl::ComputeNewPolicy(
    base::TimeTicks now,
    base::TimeDelta* new_policy_duration) {
  helper_.CheckOnValidThread();
  incoming_signals_lock_.AssertAcquired();

  Policy new_policy = Policy::NORMAL;
  *new_policy_duration = base::TimeDelta();

  if (input_stream_state_ == InputStreamState::INACTIVE)
    return new_policy;

  Policy input_priority_policy =
      input_stream_state_ ==
              InputStreamState::ACTIVE_AND_AWAITING_TOUCHSTART_RESPONSE
          ? Policy::TOUCHSTART_PRIORITY
          : Policy::COMPOSITOR_PRIORITY;
  base::TimeDelta time_left_in_policy = TimeLeftInInputEscalatedPolicy(now);
  if (time_left_in_policy > base::TimeDelta()) {
    new_policy = input_priority_policy;
    *new_policy_duration = time_left_in_policy;
  } else {
    // Reset |input_stream_state_| to ensure
    // DidReceiveInputEventOnCompositorThread will post an UpdatePolicy task
    // when it's next called.
    input_stream_state_ = InputStreamState::INACTIVE;
  }
  return new_policy;
}

base::TimeDelta RendererSchedulerImpl::TimeLeftInInputEscalatedPolicy(
    base::TimeTicks now) const {
  helper_.CheckOnValidThread();
  // TODO(rmcilroy): Change this to DCHECK_EQ when crbug.com/463869 is fixed.
  DCHECK(input_stream_state_ != InputStreamState::INACTIVE);
  incoming_signals_lock_.AssertAcquired();

  base::TimeDelta escalated_priority_duration =
      base::TimeDelta::FromMilliseconds(kPriorityEscalationAfterInputMillis);
  base::TimeDelta time_left_in_policy;
  if (last_input_process_time_on_main_.is_null() &&
      !helper_.SchedulerTaskQueueManager()->IsQueueEmpty(
          COMPOSITOR_TASK_QUEUE)) {
    // If the input event is still pending, go into input prioritized policy
    // and check again later.
    time_left_in_policy = escalated_priority_duration;
  } else {
    // Otherwise make sure the input prioritization policy ends on time.
    base::TimeTicks new_priority_end(
        std::max(last_input_receipt_time_on_compositor_,
                 last_input_process_time_on_main_) +
        escalated_priority_duration);
    time_left_in_policy = new_priority_end - now;
  }
  return time_left_in_policy;
}

bool RendererSchedulerImpl::CanEnterLongIdlePeriod(
    base::TimeTicks now,
    base::TimeDelta* next_long_idle_period_delay_out) {
  helper_.CheckOnValidThread();

  MaybeUpdatePolicy();
  if (SchedulerPolicy() == Policy::TOUCHSTART_PRIORITY) {
    // Don't start a long idle task in touch start priority, try again when
    // the policy is scheduled to end.
    *next_long_idle_period_delay_out = current_policy_expiration_time_ - now;
    return false;
  }
  return true;
}

void RendererSchedulerImpl::SetTimeSourceForTesting(
    scoped_refptr<cc::TestNowSource> time_source) {
  helper_.SetTimeSourceForTesting(time_source);
}

void RendererSchedulerImpl::SetWorkBatchSizeForTesting(size_t work_batch_size) {
  helper_.SetWorkBatchSizeForTesting(work_batch_size);
}

RendererSchedulerImpl::PollableNeedsUpdateFlag::PollableNeedsUpdateFlag(
    base::Lock* write_lock_)
    : flag_(false), write_lock_(write_lock_) {
}

RendererSchedulerImpl::PollableNeedsUpdateFlag::~PollableNeedsUpdateFlag() {
}

void RendererSchedulerImpl::PollableNeedsUpdateFlag::SetWhileLocked(
    bool value) {
  write_lock_->AssertAcquired();
  base::subtle::Release_Store(&flag_, value);
}

bool RendererSchedulerImpl::PollableNeedsUpdateFlag::IsSet() const {
  return base::subtle::Acquire_Load(&flag_) != 0;
}

// static
const char* RendererSchedulerImpl::TaskQueueIdToString(QueueId queue_id) {
  switch (queue_id) {
    case COMPOSITOR_TASK_QUEUE:
      return "compositor_tq";
    case LOADING_TASK_QUEUE:
      return "loading_tq";
    default:
      return SchedulerHelper::TaskQueueIdToString(
          static_cast<SchedulerHelper::QueueId>(queue_id));
  }
}

// static
const char* RendererSchedulerImpl::PolicyToString(Policy policy) {
  switch (policy) {
    case Policy::NORMAL:
      return "normal";
    case Policy::COMPOSITOR_PRIORITY:
      return "compositor";
    case Policy::TOUCHSTART_PRIORITY:
      return "touchstart";
    default:
      NOTREACHED();
      return nullptr;
  }
}

const char* RendererSchedulerImpl::InputStreamStateToString(
    InputStreamState state) {
  switch (state) {
    case InputStreamState::INACTIVE:
      return "inactive";
    case InputStreamState::ACTIVE:
      return "active";
    case InputStreamState::ACTIVE_AND_AWAITING_TOUCHSTART_RESPONSE:
      return "active_and_awaiting_touchstart_response";
    default:
      NOTREACHED();
      return nullptr;
  }
}

scoped_refptr<base::trace_event::ConvertableToTraceFormat>
RendererSchedulerImpl::AsValueLocked(base::TimeTicks optional_now) const {
  helper_.CheckOnValidThread();
  incoming_signals_lock_.AssertAcquired();

  if (optional_now.is_null())
    optional_now = helper_.Now();
  scoped_refptr<base::trace_event::TracedValue> state =
      new base::trace_event::TracedValue();

  state->SetString("current_policy", PolicyToString(current_policy_));
  state->SetString("idle_period_state",
                   SchedulerHelper::IdlePeriodStateToString(
                       helper_.SchedulerIdlePeriodState()));
  state->SetString("input_stream_state",
                   InputStreamStateToString(input_stream_state_));
  state->SetDouble("now", (optional_now - base::TimeTicks()).InMillisecondsF());
  state->SetDouble("last_input_receipt_time_on_compositor_",
                   (last_input_receipt_time_on_compositor_ - base::TimeTicks())
                       .InMillisecondsF());
  state->SetDouble(
      "last_input_process_time_on_main_",
      (last_input_process_time_on_main_ - base::TimeTicks()).InMillisecondsF());
  state->SetDouble(
      "estimated_next_frame_begin",
      (estimated_next_frame_begin_ - base::TimeTicks()).InMillisecondsF());

  return state;
}

RendererSchedulerImpl::InputStreamState
RendererSchedulerImpl::ComputeNewInputStreamState(
    InputStreamState current_state,
    blink::WebInputEvent::Type new_input_type,
    blink::WebInputEvent::Type last_input_type) {
  switch (new_input_type) {
    case blink::WebInputEvent::TouchStart:
      return InputStreamState::ACTIVE_AND_AWAITING_TOUCHSTART_RESPONSE;

    case blink::WebInputEvent::TouchMove:
      // Observation of consecutive touchmoves is a strong signal that the page
      // is consuming the touch sequence, in which case touchstart response
      // prioritization is no longer necessary. Otherwise, the initial touchmove
      // should preserve the touchstart response pending state.
      if (current_state ==
          InputStreamState::ACTIVE_AND_AWAITING_TOUCHSTART_RESPONSE) {
        return last_input_type == blink::WebInputEvent::TouchMove
                   ? InputStreamState::ACTIVE
                   : InputStreamState::ACTIVE_AND_AWAITING_TOUCHSTART_RESPONSE;
      }
      break;

    case blink::WebInputEvent::GestureTapDown:
    case blink::WebInputEvent::GestureShowPress:
    case blink::WebInputEvent::GestureFlingCancel:
    case blink::WebInputEvent::GestureScrollEnd:
      // With no observable effect, these meta events do not indicate a
      // meaningful touchstart response and should not impact task priority.
      return current_state;

    default:
      break;
  }
  return InputStreamState::ACTIVE;
}

}  // namespace content
