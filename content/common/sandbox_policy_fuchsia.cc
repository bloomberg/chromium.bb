// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/sandbox_policy_fuchsia.h"

#include <launchpad/launchpad.h>
#include <zircon/processargs.h>

#include "base/base_paths_fuchsia.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "content/public/common/content_switches.h"

namespace content {

void UpdateLaunchOptionsForSandbox(service_manager::SandboxType type,
                                   base::LaunchOptions* options) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kNoSandbox)) {
    type = service_manager::SANDBOX_TYPE_NO_SANDBOX;
  }

  if (type != service_manager::SANDBOX_TYPE_NO_SANDBOX) {
    auto package_root = base::GetPackageRoot();
    if (!package_root.empty()) {
      // TODO(kmarshall): Build path mappings for each sandbox type.

      // Map /pkg (read-only files deployed from the package) and /tmp into the
      // child's namespace.
      options->paths_to_map.push_back(package_root.AsUTF8Unsafe());
      base::FilePath temp_dir;
      base::GetTempDir(&temp_dir);
      options->paths_to_map.push_back(temp_dir.AsUTF8Unsafe());

      // Clear environmental variables to better isolate the child from
      // this process.
      options->clear_environ = true;

      // Propagate stdout/stderr/stdin to the child.
      options->clone_flags = LP_CLONE_FDIO_STDIO;
      return;
    }

    // TODO(crbug.com/750938): Remove this once package deployments become
    //                         mandatory.
    LOG(ERROR) << "Sandboxing was requested but is not available because"
               << "the parent process is not hosted within a package.";
    type = service_manager::SANDBOX_TYPE_NO_SANDBOX;
  }

  DCHECK_EQ(type, service_manager::SANDBOX_TYPE_NO_SANDBOX);
  options->clone_flags =
      LP_CLONE_FDIO_NAMESPACE | LP_CLONE_DEFAULT_JOB | LP_CLONE_FDIO_STDIO;
  options->clear_environ = false;
}

}  // namespace content
