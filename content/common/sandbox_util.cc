// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/sandbox_util.h"

#include "build/build_config.h"
#include "content/public/common/sandbox_init.h"
#include "ipc/ipc_platform_file.h"

#if defined(OS_POSIX)
#include <unistd.h>
#endif

namespace content {

IPC::PlatformFileForTransit BrokerGetFileHandleForProcess(
    base::PlatformFile handle,
    base::ProcessId target_process_id,
    bool should_close_source) {
  return IPC::GetPlatformFileForTransit(handle, should_close_source);
}

}  // namespace content

