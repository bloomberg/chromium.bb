// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_GPU_IN_PROCESS_THREAD_SERVICE_H_
#define GPU_IPC_GPU_IN_PROCESS_THREAD_SERVICE_H_

#include "base/compiler_specific.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/gpu_export.h"
#include "gpu/ipc/in_process_command_buffer.h"
#include "ui/gl/gl_share_group.h"

namespace gpu {

// Default Service class when no service is specified. GpuInProcessThreadService
// is used by Mus and unit tests.
class GPU_EXPORT GpuInProcessThreadService
    : public NON_EXPORTED_BASE(gpu::InProcessCommandBuffer::Service),
      public base::RefCountedThreadSafe<GpuInProcessThreadService> {
 public:
  GpuInProcessThreadService(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      gpu::SyncPointManager* sync_point_manager,
      gpu::gles2::MailboxManager* mailbox_manager,
      scoped_refptr<gl::GLShareGroup> share_group);

  // gpu::InProcessCommandBuffer::Service implementation.
  void ScheduleTask(const base::Closure& task) override;
  void ScheduleDelayedWork(const base::Closure& task) override;
  bool UseVirtualizedGLContexts() override;
  scoped_refptr<gpu::gles2::ShaderTranslatorCache> shader_translator_cache()
      override;
  scoped_refptr<gpu::gles2::FramebufferCompletenessCache>
  framebuffer_completeness_cache() override;
  gpu::SyncPointManager* sync_point_manager() override;
  void AddRef() const override;
  void Release() const override;
  bool BlockThreadOnWaitSyncToken() const override;

 private:
  friend class base::RefCountedThreadSafe<GpuInProcessThreadService>;

  ~GpuInProcessThreadService() override;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  gpu::SyncPointManager* sync_point_manager_;  // Non-owning.
  scoped_refptr<gpu::gles2::ShaderTranslatorCache> shader_translator_cache_;
  scoped_refptr<gpu::gles2::FramebufferCompletenessCache>
      framebuffer_completeness_cache_;

  DISALLOW_COPY_AND_ASSIGN(GpuInProcessThreadService);
};

}  // namespace gpu

#endif  // GPU_IPC_GPU_IN_PROCESS_THREAD_SERVICE_H_
