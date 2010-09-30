// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/service/service_child_process_host.h"

#include "base/logging.h"
#include "base/process_util.h"
#include "chrome/common/result_codes.h"

#if defined(OS_WIN)
#include "base/file_path.h"
#include "chrome/common/sandbox_policy.h"
#endif  // defined(OS_WIN)

ServiceChildProcessHost::ServiceChildProcessHost(ProcessType type)
    : ChildProcessInfo(type, -1) {
}

ServiceChildProcessHost::~ServiceChildProcessHost() {
  // We need to kill the child process when the host dies.
  base::KillProcess(handle(), ResultCodes::NORMAL_EXIT, false);
}

bool ServiceChildProcessHost::Launch(CommandLine* cmd_line,
                                     const FilePath& exposed_dir) {
#if !defined(OS_WIN)
  // TODO(sanjeevr): Implement for non-Windows OSes.
  NOTIMPLEMENTED();
  return false;
#else  // !defined(OS_WIN)
  set_handle(sandbox::StartProcessWithAccess(cmd_line, exposed_dir));
  return (handle() != base::kNullProcessHandle);
#endif  // !defined(OS_WIN)
}
