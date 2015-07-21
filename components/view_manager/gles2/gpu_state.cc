// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "components/view_manager/gles2/gpu_state.h"

namespace gles2 {

GpuState::GpuState()
    : control_thread_("gpu_command_buffer_control"),
      sync_point_manager_(new gpu::SyncPointManager(true)),
      share_group_(new gfx::GLShareGroup),
      mailbox_manager_(new gpu::gles2::MailboxManagerImpl) {
  control_thread_.Start();
}

GpuState::~GpuState() {
}

}  // namespace gles2
