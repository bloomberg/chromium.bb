// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/sandbox_policy_fuchsia.h"

#include <launchpad/launchpad.h>
#include <zircon/processargs.h>

#include "base/command_line.h"
#include "base/process/launch.h"
#include "content/public/common/content_switches.h"

namespace content {

void UpdateLaunchOptionsForSandbox(service_manager::SandboxType type,
                                   base::LaunchOptions* options) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kNoSandbox)) {
    type = service_manager::SANDBOX_TYPE_NO_SANDBOX;
  }

  if (type != service_manager::SANDBOX_TYPE_NO_SANDBOX) {
    // TODO(750938): Implement sandboxed/isolated subprocess launching
    //               once we implement a solution for accessing file resources
    //               from sandboxed processes.
    NOTIMPLEMENTED();

    options->clone_flags = LP_CLONE_FDIO_STDIO;
  } else {
    options->clone_flags = LP_CLONE_FDIO_NAMESPACE | LP_CLONE_DEFAULT_JOB |
                           LP_CLONE_FDIO_CWD | LP_CLONE_FDIO_STDIO;
  }
}

}  // namespace content
