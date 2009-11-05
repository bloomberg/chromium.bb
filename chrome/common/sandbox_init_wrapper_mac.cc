// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/sandbox_init_wrapper.h"

#include "base/command_line.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/sandbox_mac.h"

bool SandboxInitWrapper::InitializeSandbox(const CommandLine& command_line,
                                           const std::string& process_type) {
  if (command_line.HasSwitch(switches::kNoSandbox))
    return true;

  sandbox::SandboxProcessType sandbox_process_type;
  FilePath allowed_dir;  // Empty by default.

  if (process_type.empty()) {
    // Browser process isn't sandboxed.
    return true;
  } else if (process_type == switches::kRendererProcess) {
    // Renderer process sandbox.
    sandbox_process_type = sandbox::SANDBOX_TYPE_RENDERER;
  } else if (process_type == switches::kUtilityProcess) {
    // Utility process sandbox.
    sandbox_process_type = sandbox::SANDBOX_TYPE_UTILITY;
    allowed_dir = FilePath::FromWStringHack(
          command_line.GetSwitchValue(switches::kUtilityProcessAllowedDir));
  } else if (process_type == switches::kWorkerProcess) {
    // Worker process sandbox.
    sandbox_process_type = sandbox::SANDBOX_TYPE_WORKER;
  } else if ((process_type == switches::kNaClProcess) ||
             (process_type == switches::kPluginProcess) ||
             (process_type == switches::kProfileImportProcess)) {
    return true;
  } else {
    // Failsafe: If you hit an unreached here, is your new process type in need
    // of sandboxing?
    NOTREACHED();
    return true;
  }

  // Warm up APIs before turning on the sandbox.
  sandbox::SandboxWarmup();

  // Actually sandbox the process.
  return sandbox::EnableSandbox(sandbox_process_type, allowed_dir);
}
