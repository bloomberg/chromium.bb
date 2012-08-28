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

  // First, try to enable seccomp-legacy.
  seccomp_legacy_started = linux_sandbox->StartSeccompLegacy(process_type);

  // Then, try to enable seccomp-bpf.
  // If seccomp-legacy is enabled, seccomp-bpf initialization will crash
  // instead of failing gracefully.
  // TODO(markus): fix this (crbug.com/139872).
  if (!seccomp_legacy_started) {
    seccomp_bpf_started = linux_sandbox->StartSeccompBpf(process_type);
  }

  return seccomp_legacy_started || seccomp_bpf_started;
}

}  // namespace content
