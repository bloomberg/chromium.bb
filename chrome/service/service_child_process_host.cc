// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/service/service_child_process_host.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/process_util.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/common/result_codes.h"

#if defined(OS_WIN)
#include "base/file_path.h"
#include "content/common/sandbox_policy.h"
#endif  // defined(OS_WIN)

ServiceChildProcessHost::ServiceChildProcessHost()
    : handle_(base::kNullProcessHandle) {
}

ServiceChildProcessHost::~ServiceChildProcessHost() {
  // We need to kill the child process when the host dies.
  base::KillProcess(handle_, content::RESULT_CODE_NORMAL_EXIT, false);
}

bool ServiceChildProcessHost::Launch(CommandLine* cmd_line,
                                     bool no_sandbox,
                                     const FilePath& exposed_dir) {
#if !defined(OS_WIN)
  // TODO(sanjeevr): Implement for non-Windows OSes.
  NOTIMPLEMENTED();
  return false;
#else  // !defined(OS_WIN)

  if (no_sandbox) {
    base::ProcessHandle process = base::kNullProcessHandle;
    cmd_line->AppendSwitch(switches::kNoSandbox);
    base::LaunchProcess(*cmd_line, base::LaunchOptions(), &handle_);
  } else {
    handle_ = sandbox::StartProcessWithAccess(cmd_line, exposed_dir);
  }
  return (handle_ != base::kNullProcessHandle);
#endif  // !defined(OS_WIN)
}
