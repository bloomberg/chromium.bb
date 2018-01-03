// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/gpu_in_process_thread_service.h"

#include "base/lazy_instance.h"
#include "base/threading/thread_task_runner_handle.h"

namespace gpu {

GpuInProcessThreadService::GpuInProcessThreadService(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    gpu::SyncPointManager* sync_point_manager,
    gpu::MailboxManager* mailbox_manager,
    scoped_refptr<gl::GLShareGroup> share_group,
    const GpuFeatureInfo& gpu_feature_info)
    : gpu::InProcessCommandBuffer::Service(GpuPreferences(),
                                           mailbox_manager,
                                           share_group,
                                           gpu_feature_info),
      task_runner_(task_runner),
      sync_point_manager_(sync_point_manager) {}

void GpuInProcessThreadService::ScheduleTask(const base::Closure& task) {
  task_runner_->PostTask(FROM_HERE, task);
}

void GpuInProcessThreadService::ScheduleDelayedWork(const base::Closure& task) {
  task_runner_->PostDelayedTask(FROM_HERE, task,
                                base::TimeDelta::FromMilliseconds(2));
}
bool GpuInProcessThreadService::UseVirtualizedGLContexts() {
  return true;
}

gpu::SyncPointManager* GpuInProcessThreadService::sync_point_manager() {
  return sync_point_manager_;
}

void GpuInProcessThreadService::AddRef() const {
  base::RefCountedThreadSafe<GpuInProcessThreadService>::AddRef();
}

void GpuInProcessThreadService::Release() const {
  base::RefCountedThreadSafe<GpuInProcessThreadService>::Release();
}

bool GpuInProcessThreadService::BlockThreadOnWaitSyncToken() const {
  return false;
}

GpuInProcessThreadService::~GpuInProcessThreadService() = default;

}  // namespace gpu
