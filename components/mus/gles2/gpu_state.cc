// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "components/mus/gles2/gpu_state.h"

#include "base/command_line.h"
#include "gpu/config/gpu_info_collector.h"
#include "ui/gl/gl_implementation.h"

namespace mus {

GpuState::GpuState(bool hardware_rendering_available)
    : control_thread_("gpu_command_buffer_control"),
      sync_point_manager_(new gpu::SyncPointManager(true)),
      share_group_(new gfx::GLShareGroup),
      mailbox_manager_(new gpu::gles2::MailboxManagerImpl),
      hardware_rendering_available_(hardware_rendering_available) {
  // TODO(penghuang): investigate why gpu::CollectBasicGraphicsInfo() failed on
  // windows remote desktop.
  const gfx::GLImplementation impl = gfx::GetGLImplementation();
  if (impl != gfx::kGLImplementationNone &&
      impl != gfx::kGLImplementationOSMesaGL &&
      impl != gfx::kGLImplementationMockGL) {
    gpu::CollectInfoResult result = gpu::CollectBasicGraphicsInfo(&gpu_info_);
    LOG_IF(ERROR, result != gpu::kCollectInfoSuccess)
        << "Collect basic graphics info failed!";
  }
  if (impl != gfx::kGLImplementationNone) {
    gpu::CollectInfoResult result = gpu::CollectContextGraphicsInfo(&gpu_info_);
    LOG_IF(ERROR, result != gpu::kCollectInfoSuccess)
        << "Collect context graphics info failed!";
  }
  control_thread_.Start();
}

GpuState::~GpuState() {}

void GpuState::StopControlThread() {
  control_thread_.Stop();
}

}  // namespace mus
