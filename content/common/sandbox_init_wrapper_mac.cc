// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/sandbox_init_wrapper.h"

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/logging.h"
#include "content/common/content_switches.h"
#include "content/common/sandbox_mac.h"

bool SandboxInitWrapper::InitializeSandbox(const CommandLine& command_line,
                                           const std::string& process_type) {
  using sandbox::Sandbox;

  if (command_line.HasSwitch(switches::kNoSandbox))
    return true;

  Sandbox::SandboxProcessType sandbox_process_type;
  FilePath allowed_dir;  // Empty by default.

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
  } else if (process_type == switches::kExtensionProcess) {
    // Extension processes are just renderers [they use RenderMain()] with a
    // different set of command line flags.
    // If we ever get here it means something has changed in regards
    // to the extension process mechanics and we should probably reexamine
    // how we sandbox extension processes since they are no longer identical
    // to renderers.
    NOTREACHED();
    return true;
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
             (process_type == switches::kProfileImportProcess) ||
             (process_type == switches::kServiceProcess)) {
    return true;
  } else {
    // Failsafe: If you hit an unreached here, is your new process type in need
    // of sandboxing?
    NOTREACHED();
    return true;
  }

  // Warm up APIs before turning on the sandbox.
  Sandbox::SandboxWarmup(sandbox_process_type);

  // Actually sandbox the process.
  return Sandbox::EnableSandbox(sandbox_process_type, allowed_dir);
}
