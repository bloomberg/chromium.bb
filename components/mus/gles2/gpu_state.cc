// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "components/mus/gles2/gpu_state.h"

#include "base/command_line.h"
#include "gpu/config/gpu_info_collector.h"

namespace mus {

namespace {
const char kOverrideUseGLWithOSMesaForTests[] =
    "override-use-gl-with-osmesa-for-tests";
}  // namespace

GpuState::GpuState()
    : control_thread_("gpu_command_buffer_control"),
      sync_point_manager_(new gpu::SyncPointManager(true)),
      share_group_(new gfx::GLShareGroup),
      mailbox_manager_(new gpu::gles2::MailboxManagerImpl) {
  base::CommandLine* cmd = base::CommandLine::ForCurrentProcess();
  if (!cmd->HasSwitch(kOverrideUseGLWithOSMesaForTests)) {
    gpu::CollectInfoResult result = gpu::CollectBasicGraphicsInfo(&gpu_info_);
    CHECK(result == gpu::kCollectInfoSuccess);
    result = gpu::CollectContextGraphicsInfo(&gpu_info_);
    CHECK(result == gpu::kCollectInfoSuccess);
  }
  control_thread_.Start();
}

GpuState::~GpuState() {}

void GpuState::StopControlThread() {
  control_thread_.Stop();
}

}  // namespace mus
