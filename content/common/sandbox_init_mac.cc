// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/sandbox_init.h"

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/logging.h"
#include "content/common/sandbox_mac.h"
#include "content/public/common/content_switches.h"

namespace content {

bool InitializeSandbox(int sandbox_type, const FilePath& allowed_dir) {
  // Warm up APIs before turning on the sandbox.
  sandbox::Sandbox::SandboxWarmup(sandbox_type);

  // Actually sandbox the process.
  return sandbox::Sandbox::EnableSandbox(sandbox_type, allowed_dir);
}

// Fill in |sandbox_type| and |allowed_dir| based on the command line,  returns
// false if the current process type doesn't need to be sandboxed or if the
// sandbox was disabled from the command line.
bool GetSandboxTypeFromCommandLine(int* sandbox_type,
                                   FilePath* allowed_dir) {
  DCHECK(sandbox_type);
  DCHECK(allowed_dir);

  *sandbox_type = -1;
  *allowed_dir = FilePath();  // Empty by default.

  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kNoSandbox))
    return false;

  std::string process_type =
      command_line.GetSwitchValueASCII(switches::kProcessType);
  if (process_type.empty()) {
    // Browser process isn't sandboxed.
    return false;
  } else if (process_type == switches::kRendererProcess) {
    if (!command_line.HasSwitch(switches::kDisable3DAPIs) &&
        !command_line.HasSwitch(switches::kDisableExperimentalWebGL) &&
        command_line.HasSwitch(switches::kInProcessWebGL)) {
      // TODO(kbr): this check seems to be necessary only on this
      // platform because the sandbox is initialized later. Remove
      // this once this flag is removed.
      return false;
    } else {
      *sandbox_type = SANDBOX_TYPE_RENDERER;
    }
  } else if (process_type == switches::kUtilityProcess) {
    // Utility process sandbox.
    *sandbox_type = SANDBOX_TYPE_UTILITY;
    *allowed_dir =
        command_line.GetSwitchValuePath(switches::kUtilityProcessAllowedDir);
  } else if (process_type == switches::kWorkerProcess) {
    // Worker process sandbox.
    *sandbox_type = SANDBOX_TYPE_WORKER;
  } else if (process_type == switches::kGpuProcess) {
    *sandbox_type = SANDBOX_TYPE_GPU;
  } else if ((process_type == switches::kPluginProcess) ||
             (process_type == switches::kServiceProcess) ||
             (process_type == switches::kPpapiBrokerProcess)) {
    return false;
  } else if (process_type == switches::kPpapiPluginProcess) {
    *sandbox_type = SANDBOX_TYPE_PPAPI;
  } else {
    // Failsafe: If you hit an unreached here, is your new process type in need
    // of sandboxing?
    NOTREACHED() << "Unknown process type " << process_type;
    return false;
  }
  return true;
}

bool InitializeSandbox() {
  int sandbox_type = 0;
  FilePath allowed_dir;
  if (!GetSandboxTypeFromCommandLine(&sandbox_type, &allowed_dir))
    return true;
  return InitializeSandbox(sandbox_type, allowed_dir);
}

}  // namespace content
