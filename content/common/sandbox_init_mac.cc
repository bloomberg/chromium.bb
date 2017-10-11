// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/sandbox_init_mac.h"

#include "base/callback.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/sandbox_init.h"
#include "media/gpu/vt_video_decode_accelerator_mac.h"
#include "sandbox/mac/seatbelt.h"
#include "services/service_manager/sandbox/mac/sandbox_mac.h"
#include "services/service_manager/sandbox/sandbox_type.h"
#include "ui/gl/init/gl_factory.h"

namespace content {

namespace {

// NOTE: This function is the exact code for the entry point of mac sandbox
// once it moves to service_manager/sandbox.
bool InitializeSandboxInternal(service_manager::SandboxType sandbox_type,
                               const base::FilePath& allowed_dir,
                               base::OnceClosure hook) {
  // Warm up APIs before turning on the sandbox.
  service_manager::Sandbox::SandboxWarmup(sandbox_type);

  // Execute the post warmup callback.
  if (!hook.is_null())
    std::move(hook).Run();

  // Actually sandbox the process.
  return service_manager::Sandbox::EnableSandbox(sandbox_type, allowed_dir);
}

// Helper method to make a closure from a closure.
base::OnceClosure MaybeWrapWithGPUSandboxHook(
    service_manager::SandboxType sandbox_type,
    base::OnceClosure original) {
  if (sandbox_type != service_manager::SANDBOX_TYPE_GPU)
    return original;

  return base::Bind(
      [](base::OnceClosure arg) {
        // Preload either the desktop GL or the osmesa so, depending on the
        // --use-gl flag.
        gl::init::InitializeGLOneOff();

        // Preload VideoToolbox.
        media::InitializeVideoToolbox();

        // Invoke original hook.
        if (!arg.is_null())
          std::move(arg).Run();
      },
      base::Passed(std::move(original)));
}

// Fill in |sandbox_type| and |allowed_dir| based on the command line,  returns
// false if the current process type doesn't need to be sandboxed or if the
// sandbox was disabled from the command line.
bool GetSandboxInfoFromCommandLine(service_manager::SandboxType* sandbox_type,
                                   base::FilePath* allowed_dir) {
  DCHECK(sandbox_type);
  DCHECK(allowed_dir);

  *allowed_dir = base::FilePath();  // Empty by default.
  auto* command_line = base::CommandLine::ForCurrentProcess();
  std::string process_type =
      command_line->GetSwitchValueASCII(switches::kProcessType);
  if (process_type == switches::kUtilityProcess) {
    *allowed_dir =
        command_line->GetSwitchValuePath(switches::kUtilityProcessAllowedDir);
  }

  *sandbox_type = service_manager::SandboxTypeFromCommandLine(*command_line);
  if (service_manager::IsUnsandboxedSandboxType(*sandbox_type))
    return false;

  if (command_line->HasSwitch(switches::kV2SandboxedEnabled)) {
    CHECK(sandbox::Seatbelt::IsSandboxed());
    // Do not enable the sandbox if V2 is already enabled.
    return false;
  }

  return *sandbox_type != service_manager::SANDBOX_TYPE_INVALID;
}

}  // namespace

bool InitializeSandbox(service_manager::SandboxType sandbox_type,
                       const base::FilePath& allowed_dir) {
  return InitializeSandboxInternal(
      sandbox_type, allowed_dir,
      MaybeWrapWithGPUSandboxHook(sandbox_type, base::OnceClosure()));
}

bool InitializeSandboxWithPostWarmupHook(base::OnceClosure hook) {
  service_manager::SandboxType sandbox_type =
      service_manager::SANDBOX_TYPE_INVALID;
  base::FilePath allowed_dir;
  return !GetSandboxInfoFromCommandLine(&sandbox_type, &allowed_dir) ||
         InitializeSandboxInternal(
             sandbox_type, allowed_dir,
             MaybeWrapWithGPUSandboxHook(sandbox_type, std::move(hook)));
}

bool InitializeSandbox() {
  return InitializeSandboxWithPostWarmupHook(base::OnceClosure());
}

}  // namespace content
