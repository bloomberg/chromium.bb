// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/mailbox_manager.h"

#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "gpu/command_buffer/service/gpu_preferences.h"
#include "gpu/command_buffer/service/mailbox_manager_impl.h"
#include "gpu/command_buffer/service/mailbox_manager_sync.h"

namespace gpu {
namespace gles2 {

// static
std::unique_ptr<MailboxManager> MailboxManager::Create(
    const GpuPreferences& gpu_preferences) {
  if (gpu_preferences.enable_threaded_texture_mailboxes)
    return std::make_unique<MailboxManagerSync>();
  return std::make_unique<MailboxManagerImpl>();
}

}  // namespage gles2
}  // namespace gpu
