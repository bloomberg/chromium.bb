// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/command_line.h"
#include "base/logging.h"
#include "content/common/sandbox_linux.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/sandbox_init.h"

namespace content {

// TODO(jln): have call sites provide a process / policy type to
// InitializeSandbox().
bool InitializeSandbox() {
  bool seccomp_legacy_started = false;
  bool seccomp_bpf_started = false;
  LinuxSandbox* linux_sandbox = LinuxSandbox::GetInstance();
  const std::string process_type =
      CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kProcessType);

  // No matter what, it's always an error to call InitializeSandbox() after
  // threads have been created.
  if (!linux_sandbox->IsSingleThreaded()) {
    std::string error_message = "InitializeSandbox() called with multiple "
                                "threads in process " + process_type;
    // TODO(jln): change this into a CHECK() once we are more comfortable it
    // does not trigger.
    LOG(ERROR) << error_message;
    return false;
  }

  // Attempt to limit the future size of the address space of the process.
  linux_sandbox->LimitAddressSpace(process_type);

  // First, try to enable seccomp-bpf.
  seccomp_bpf_started = linux_sandbox->StartSeccompBpf(process_type);

  // If that fails, try to enable seccomp-legacy.
  if (!seccomp_bpf_started) {
    seccomp_legacy_started = linux_sandbox->StartSeccompLegacy(process_type);
  }

  return seccomp_legacy_started || seccomp_bpf_started;
}

}  // namespace content
