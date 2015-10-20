// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_GLES2_GPU_STATE_H_
#define COMPONENTS_MUS_GLES2_GPU_STATE_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "gpu/command_buffer/service/mailbox_manager_impl.h"
#include "gpu/command_buffer/service/sync_point_manager.h"
#include "gpu/config/gpu_info.h"
#include "ui/gl/gl_share_group.h"

namespace mus {

// We need to share these across all CommandBuffer instances so that contexts
// they create can share resources with each other via mailboxes.
class GpuState : public base::RefCounted<GpuState> {
 public:
  explicit GpuState(bool hardware_rendering_available);

  // We run the CommandBufferImpl on the control_task_runner, which forwards
  // most method class to the CommandBufferDriver, which runs on the "driver",
  // thread (i.e., the thread on which GpuImpl instances are created).
  scoped_refptr<base::SingleThreadTaskRunner> control_task_runner() {
    return control_thread_.task_runner();
  }

  void StopControlThread();

  // These objects are intended to be used on the "driver" thread (i.e., the
  // thread on which GpuImpl instances are created).
  gfx::GLShareGroup* share_group() const { return share_group_.get(); }
  gpu::gles2::MailboxManager* mailbox_manager() const {
    return mailbox_manager_.get();
  }
  gpu::SyncPointManager* sync_point_manager() const {
    return sync_point_manager_.get();
  }

  const gpu::GPUInfo& gpu_info() const {
    return gpu_info_;
  }

  bool HardwareRenderingAvailable() const {
    return hardware_rendering_available_;
  }

 private:
  friend class base::RefCounted<GpuState>;
  ~GpuState();

  base::Thread control_thread_;
  scoped_ptr<gpu::SyncPointManager> sync_point_manager_;
  scoped_refptr<gfx::GLShareGroup> share_group_;
  scoped_refptr<gpu::gles2::MailboxManager> mailbox_manager_;
  gpu::GPUInfo gpu_info_;
  bool hardware_rendering_available_;
};

}  // namespace mus

#endif  // COMPONENTS_MUS_GLES2_GPU_STATE_H_
