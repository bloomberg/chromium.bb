// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/scheduler/renderer_scheduler_impl.h"

#include "base/bind.h"
#include "cc/output/begin_frame_args.h"
#include "content/renderer/scheduler/renderer_task_queue_selector.h"
#include "ui/gfx/frame_time.h"

namespace content {

RendererSchedulerImpl::RendererSchedulerImpl(
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner)
    : renderer_task_queue_selector_(new RendererTaskQueueSelector()),
      task_queue_manager_(
          new TaskQueueManager(TASK_QUEUE_COUNT,
                               main_task_runner,
                               renderer_task_queue_selector_.get())),
      control_task_runner_(
          task_queue_manager_->TaskRunnerForQueue(CONTROL_TASK_QUEUE)),
      default_task_runner_(
          task_queue_manager_->TaskRunnerForQueue(DEFAULT_TASK_QUEUE)),
      compositor_task_runner_(
          task_queue_manager_->TaskRunnerForQueue(COMPOSITOR_TASK_QUEUE)),
      current_policy_(NORMAL_PRIORITY_POLICY),
      policy_may_need_update_(&incoming_signals_lock_),
      weak_factory_(this) {
  weak_renderer_scheduler_ptr_ = weak_factory_.GetWeakPtr();
  update_policy_closure_ = base::Bind(&RendererSchedulerImpl::UpdatePolicy,
                                      weak_renderer_scheduler_ptr_);
  end_idle_period_closure_ = base::Bind(&RendererSchedulerImpl::EndIdlePeriod,
                                        weak_renderer_scheduler_ptr_);
  idle_task_runner_ = make_scoped_refptr(new SingleThreadIdleTaskRunner(
      task_queue_manager_->TaskRunnerForQueue(IDLE_TASK_QUEUE),
      base::Bind(&RendererSchedulerImpl::CurrentIdleTaskDeadlineCallback,
                 weak_renderer_scheduler_ptr_)));
  renderer_task_queue_selector_->SetQueuePriority(
      CONTROL_TASK_QUEUE, RendererTaskQueueSelector::CONTROL_PRIORITY);
  renderer_task_queue_selector_->DisableQueue(IDLE_TASK_QUEUE);
  task_queue_manager_->SetAutoPump(IDLE_TASK_QUEUE, false);
}

RendererSchedulerImpl::~RendererSchedulerImpl() {
}

void RendererSchedulerImpl::Shutdown() {
  main_thread_checker_.CalledOnValidThread();
  task_queue_manager_.reset();
}

scoped_refptr<base::SingleThreadTaskRunner>
RendererSchedulerImpl::DefaultTaskRunner() {
  main_thread_checker_.CalledOnValidThread();
  return default_task_runner_;
}

scoped_refptr<base::SingleThreadTaskRunner>
RendererSchedulerImpl::CompositorTaskRunner() {
  main_thread_checker_.CalledOnValidThread();
  return compositor_task_runner_;
}

scoped_refptr<SingleThreadIdleTaskRunner>
RendererSchedulerImpl::IdleTaskRunner() {
  main_thread_checker_.CalledOnValidThread();
  return idle_task_runner_;
}

void RendererSchedulerImpl::WillBeginFrame(const cc::BeginFrameArgs& args) {
  main_thread_checker_.CalledOnValidThread();
  if (!task_queue_manager_)
    return;

  EndIdlePeriod();
  estimated_next_frame_begin_ = args.frame_time + args.interval;
}

void RendererSchedulerImpl::DidCommitFrameToCompositor() {
  main_thread_checker_.CalledOnValidThread();
  if (!task_queue_manager_)
    return;

  base::TimeTicks now(Now());
  if (now < estimated_next_frame_begin_) {
    StartIdlePeriod();
    control_task_runner_->PostDelayedTask(FROM_HERE, end_idle_period_closure_,
                                          estimated_next_frame_begin_ - now);
  }
}

void RendererSchedulerImpl::DidReceiveInputEventOnCompositorThread() {
  // TODO(rmcilroy): Decide whether only a subset of input events should trigger
  // compositor priority policy - http://crbug.com/429814.
  base::AutoLock lock(incoming_signals_lock_);
  if (last_input_time_.is_null()) {
    // Update scheduler policy if should start a new compositor policy mode.
    policy_may_need_update_.SetLocked(true);
    PostUpdatePolicyOnControlRunner(base::TimeDelta());
  }
  last_input_time_ = Now();
}

bool RendererSchedulerImpl::ShouldYieldForHighPriorityWork() {
  main_thread_checker_.CalledOnValidThread();
  if (!task_queue_manager_)
    return false;

  MaybeUpdatePolicy();
  // We only yield if we are in the compositor priority and there is compositor
  // work outstanding. Note: even though the control queue is higher priority
  // we don't yield for it since these tasks are not user-provided work and they
  // are only intended to run before the next task, not interrupt the tasks.
  return SchedulerPolicy() == COMPOSITOR_PRIORITY_POLICY &&
         !task_queue_manager_->IsQueueEmpty(COMPOSITOR_TASK_QUEUE);
}

void RendererSchedulerImpl::CurrentIdleTaskDeadlineCallback(
    base::TimeTicks* deadline_out) const {
  main_thread_checker_.CalledOnValidThread();
  *deadline_out = estimated_next_frame_begin_;
}

RendererSchedulerImpl::Policy RendererSchedulerImpl::SchedulerPolicy() const {
  main_thread_checker_.CalledOnValidThread();
  return current_policy_;
}

void RendererSchedulerImpl::MaybeUpdatePolicy() {
  main_thread_checker_.CalledOnValidThread();
  if (policy_may_need_update_.IsSet()) {
    UpdatePolicy();
  }
}

void RendererSchedulerImpl::PostUpdatePolicyOnControlRunner(
    base::TimeDelta delay) {
  control_task_runner_->PostDelayedTask(FROM_HERE, update_policy_closure_,
                                        delay);
}

void RendererSchedulerImpl::UpdatePolicy() {
  main_thread_checker_.CalledOnValidThread();
  if (!task_queue_manager_)
    return;

  base::AutoLock lock(incoming_signals_lock_);
  policy_may_need_update_.SetLocked(false);

  Policy new_policy = NORMAL_PRIORITY_POLICY;
  if (!last_input_time_.is_null()) {
    base::TimeDelta compositor_priority_duration =
        base::TimeDelta::FromMilliseconds(kCompositorPriorityAfterTouchMillis);
    base::TimeTicks compositor_priority_end(last_input_time_ +
                                            compositor_priority_duration);
    base::TimeTicks now(Now());
    if (compositor_priority_end > now) {
      PostUpdatePolicyOnControlRunner(compositor_priority_end - now);
      new_policy = COMPOSITOR_PRIORITY_POLICY;
    } else {
      // Null out last_input_time_ to ensure
      // DidReceiveInputEventOnCompositorThread will post an
      // UpdatePolicy task when it's next called.
      last_input_time_ = base::TimeTicks();
    }
  }

  if (new_policy == current_policy_)
    return;

  switch (new_policy) {
    case COMPOSITOR_PRIORITY_POLICY:
      renderer_task_queue_selector_->SetQueuePriority(
          COMPOSITOR_TASK_QUEUE, RendererTaskQueueSelector::HIGH_PRIORITY);
      break;
    case NORMAL_PRIORITY_POLICY:
      renderer_task_queue_selector_->SetQueuePriority(
          COMPOSITOR_TASK_QUEUE, RendererTaskQueueSelector::NORMAL_PRIORITY);
      break;
  }
  current_policy_ = new_policy;
}

void RendererSchedulerImpl::StartIdlePeriod() {
  main_thread_checker_.CalledOnValidThread();
  renderer_task_queue_selector_->EnableQueue(
      IDLE_TASK_QUEUE, RendererTaskQueueSelector::BEST_EFFORT_PRIORITY);
  task_queue_manager_->PumpQueue(IDLE_TASK_QUEUE);
}

void RendererSchedulerImpl::EndIdlePeriod() {
  main_thread_checker_.CalledOnValidThread();
  renderer_task_queue_selector_->DisableQueue(IDLE_TASK_QUEUE);
}

base::TimeTicks RendererSchedulerImpl::Now() const {
  return gfx::FrameTime::Now();
}

RendererSchedulerImpl::PollableNeedsUpdateFlag::PollableNeedsUpdateFlag(
    base::Lock* write_lock_)
    : flag_(false), write_lock_(write_lock_) {
}

RendererSchedulerImpl::PollableNeedsUpdateFlag::~PollableNeedsUpdateFlag() {
}

void RendererSchedulerImpl::PollableNeedsUpdateFlag::SetLocked(bool value) {
  write_lock_->AssertAcquired();
  base::subtle::Release_Store(&flag_, value);
}

bool RendererSchedulerImpl::PollableNeedsUpdateFlag::IsSet() const {
  thread_checker_.CalledOnValidThread();
  return base::subtle::Acquire_Load(&flag_) != 0;
}
}  // namespace content
