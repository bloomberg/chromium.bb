// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/gpu_in_process_thread_service.h"

#include "base/threading/thread_task_runner_handle.h"
#include "gpu/command_buffer/service/scheduler.h"

namespace gpu {

namespace {

// CommandBufferTaskExectuor::Sequence implementation that uses gpu scheduler
// sequences.
class SchedulerSequence : public CommandBufferTaskExecutor::Sequence {
 public:
  explicit SchedulerSequence(Scheduler* scheduler)
      : CommandBufferTaskExecutor::Sequence(),
        scheduler_(scheduler),
        sequence_id_(scheduler->CreateSequence(SchedulingPriority::kHigh)) {}

  // Note: this drops tasks not executed yet.
  ~SchedulerSequence() override { scheduler_->DestroySequence(sequence_id_); }

  // CommandBufferTaskExecutor::Sequence implementation.
  SequenceId GetSequenceId() override { return sequence_id_; }

  bool ShouldYield() override { return scheduler_->ShouldYield(sequence_id_); }

  void SetEnabled(bool enabled) override {
    if (enabled)
      scheduler_->EnableSequence(sequence_id_);
    else
      scheduler_->DisableSequence(sequence_id_);
  }

  void ScheduleTask(base::OnceClosure task,
                    std::vector<SyncToken> sync_token_fences) override {
    scheduler_->ScheduleTask(Scheduler::Task(sequence_id_, std::move(task),
                                             std::move(sync_token_fences)));
  }

  void ContinueTask(base::OnceClosure task) override {
    scheduler_->ContinueTask(sequence_id_, std::move(task));
  }

 private:
  Scheduler* const scheduler_;
  const SequenceId sequence_id_;

  DISALLOW_COPY_AND_ASSIGN(SchedulerSequence);
};

}  // namespace

GpuInProcessThreadService::GpuInProcessThreadService(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    Scheduler* scheduler,
    SyncPointManager* sync_point_manager,
    MailboxManager* mailbox_manager,
    scoped_refptr<gl::GLShareGroup> share_group,
    gl::GLSurfaceFormat share_group_surface_format,
    const GpuFeatureInfo& gpu_feature_info,
    const GpuPreferences& gpu_preferences)
    : CommandBufferTaskExecutor(gpu_preferences,
                                gpu_feature_info,
                                sync_point_manager,
                                mailbox_manager,
                                share_group,
                                share_group_surface_format),
      task_runner_(task_runner),
      scheduler_(scheduler) {}

GpuInProcessThreadService::~GpuInProcessThreadService() = default;

bool GpuInProcessThreadService::ForceVirtualizedGLContexts() const {
  return false;
}

bool GpuInProcessThreadService::ShouldCreateMemoryTracker() const {
  return true;
}

bool GpuInProcessThreadService::BlockThreadOnWaitSyncToken() const {
  return false;
}

std::unique_ptr<CommandBufferTaskExecutor::Sequence>
GpuInProcessThreadService::CreateSequence() {
  return std::make_unique<SchedulerSequence>(scheduler_);
}

void GpuInProcessThreadService::ScheduleOutOfOrderTask(base::OnceClosure task) {
  task_runner_->PostTask(FROM_HERE, std::move(task));
}

void GpuInProcessThreadService::ScheduleDelayedWork(base::OnceClosure task) {
  task_runner_->PostDelayedTask(FROM_HERE, std::move(task),
                                base::TimeDelta::FromMilliseconds(2));
}

}  // namespace gpu
