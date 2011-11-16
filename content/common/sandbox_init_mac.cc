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

bool InitializeSandbox() {
  using sandbox::Sandbox;

  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kNoSandbox))
    return true;

  Sandbox::SandboxProcessType sandbox_process_type;
  FilePath allowed_dir;  // Empty by default.

  std::string process_type =
      command_line.GetSwitchValueASCII(switches::kProcessType);
  if (process_type.empty()) {
    // Browser process isn't sandboxed.
    return true;
  } else if (process_type == switches::kRendererProcess) {
    if (!command_line.HasSwitch(switches::kDisable3DAPIs) &&
        !command_line.HasSwitch(switches::kDisableExperimentalWebGL) &&
        command_line.HasSwitch(switches::kInProcessWebGL)) {
      // TODO(kbr): this check seems to be necessary only on this
      // platform because the sandbox is initialized later. Remove
      // this once this flag is removed.
      return true;
    } else {
      sandbox_process_type = Sandbox::SANDBOX_TYPE_RENDERER;
    }
  } else if (process_type == switches::kUtilityProcess) {
    // Utility process sandbox.
    sandbox_process_type = Sandbox::SANDBOX_TYPE_UTILITY;
    allowed_dir =
        command_line.GetSwitchValuePath(switches::kUtilityProcessAllowedDir);
  } else if (process_type == switches::kWorkerProcess) {
    // Worker process sandbox.
    sandbox_process_type = Sandbox::SANDBOX_TYPE_WORKER;
  } else if (process_type == switches::kNaClLoaderProcess) {
    // Native Client sel_ldr (user untrusted code) sandbox.
    sandbox_process_type = Sandbox::SANDBOX_TYPE_NACL_LOADER;
  } else if (process_type == switches::kGpuProcess) {
    sandbox_process_type = Sandbox::SANDBOX_TYPE_GPU;
  } else if ((process_type == switches::kPluginProcess) ||
             (process_type == switches::kServiceProcess) ||
             (process_type == switches::kPpapiBrokerProcess)) {
    return true;
  } else if (process_type == switches::kPpapiPluginProcess) {
    sandbox_process_type = Sandbox::SANDBOX_TYPE_PPAPI;
  } else {
    // Failsafe: If you hit an unreached here, is your new process type in need
    // of sandboxing?
    NOTREACHED() << "Unknown process type " << process_type;
    return true;
  }

  // Warm up APIs before turning on the sandbox.
  Sandbox::SandboxWarmup(sandbox_process_type);

  // Actually sandbox the process.
  return Sandbox::EnableSandbox(sandbox_process_type, allowed_dir);
}

}  // namespace content
