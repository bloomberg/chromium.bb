// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_GFX_DEFERRED_GPU_COMMAND_SERVICE_H_
#define ANDROID_WEBVIEW_BROWSER_GFX_DEFERRED_GPU_COMMAND_SERVICE_H_

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/containers/queue.h"
#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "base/threading/thread_local.h"
#include "base/time/time.h"
#include "gpu/config/gpu_info.h"
#include "gpu/ipc/command_buffer_task_executor.h"

namespace gpu {
struct GpuFeatureInfo;
struct GpuPreferences;
class SyncPointManager;
class SharedImageManager;
}

namespace android_webview {

class ScopedAllowGL {
 public:
  ScopedAllowGL();
  ~ScopedAllowGL();

  static bool IsAllowed();

 private:
  static base::LazyInstance<base::ThreadLocalBoolean>::DestructorAtExit
      allow_gl;

  DISALLOW_COPY_AND_ASSIGN(ScopedAllowGL);
};

class TaskForwardingSequence;

class DeferredGpuCommandService : public gpu::CommandBufferTaskExecutor {
 public:
  static DeferredGpuCommandService* GetInstance();

  // gpu::CommandBufferTaskExecutor implementation.
  bool ForceVirtualizedGLContexts() const override;
  bool ShouldCreateMemoryTracker() const override;
  std::unique_ptr<gpu::CommandBufferTaskExecutor::Sequence> CreateSequence()
      override;
  void ScheduleOutOfOrderTask(base::OnceClosure task) override;
  void ScheduleDelayedWork(base::OnceClosure task) override;
  void PostNonNestableToClient(base::OnceClosure callback) override;

  const gpu::GPUInfo& gpu_info() const { return gpu_info_; }

  bool CanSupportThreadedTextureMailbox() const;

 protected:
  ~DeferredGpuCommandService() override;

 private:
  friend class ScopedAllowGL;
  friend class TaskForwardingSequence;

  DeferredGpuCommandService(
      std::unique_ptr<gpu::SyncPointManager> sync_point_manager,
      std::unique_ptr<gpu::MailboxManager> mailbox_manager,
      std::unique_ptr<gpu::SharedImageManager> shared_image_manager,
      const gpu::GpuPreferences& gpu_preferences,
      const gpu::GPUInfo& gpu_info,
      const gpu::GpuFeatureInfo& gpu_feature_info);

  static DeferredGpuCommandService* CreateDeferredGpuCommandService();

  // Flush the idle queue until it is empty.
  void PerformAllIdleWork();

  // Called by ScopedAllowGL and ScheduleTask().
  void RunTasks();

  void RunAllTasks();

  // Called by TaskForwardingSequence. |out_of_order| indicates if task should
  // be run ahead of already enqueued tasks.
  void ScheduleTask(base::OnceClosure task, bool out_of_order);

  // All access to task queue should happen on a single thread.
  THREAD_CHECKER(task_queue_thread_checker_);
  base::circular_deque<base::OnceClosure> tasks_;
  base::queue<std::pair<base::Time, base::OnceClosure>> idle_tasks_;
  base::queue<base::OnceClosure> client_tasks_;

  bool inside_run_tasks_ = false;
  bool inside_run_idle_tasks_ = false;

  std::unique_ptr<gpu::SyncPointManager> sync_point_manager_;
  std::unique_ptr<gpu::MailboxManager> mailbox_manager_;
  gpu::GPUInfo gpu_info_;

  // TODO(https://crbug.com/922834): This should eventually be shared with the
  // GpuChannelManager's SharedImageManager.
  std::unique_ptr<gpu::SharedImageManager> shared_image_manager_;

  DISALLOW_COPY_AND_ASSIGN(DeferredGpuCommandService);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_GFX_DEFERRED_GPU_COMMAND_SERVICE_H_
