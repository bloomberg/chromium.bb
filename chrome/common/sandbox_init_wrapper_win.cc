// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/sandbox_init_wrapper.h"

#include "base/command_line.h"
#include "base/logging.h"

#include "chrome/common/chrome_switches.h"

void SandboxInitWrapper::SetServices(sandbox::SandboxInterfaceInfo* info) {
  if (info) {
    broker_services_ = info->broker_services;
    target_services_ = info->target_services;
  }
  // Both interface pointers cannot be non-zero. A process can either
  // be a target or a broker but not both.
  DCHECK(!(target_services_ && broker_services_));
}

bool SandboxInitWrapper::InitializeSandbox(const CommandLine& command_line,
                                           const std::string& process_type) {
  if (command_line.HasSwitch(switches::kNoSandbox))
    return true;
  if ((process_type == switches::kRendererProcess) ||
      (process_type == switches::kExtensionProcess) ||
      (process_type == switches::kWorkerProcess) ||
      (process_type == switches::kNaClLoaderProcess) ||
      (process_type == switches::kUtilityProcess)) {
    // The above five process types must be sandboxed unless --no-sandbox
    // is present in the command line.
    if (!target_services_)
      return false;
  } else {
    // Other process types might or might not be sandboxed.
    // TODO(cpu): clean this mess.
    if (!target_services_)
      return true;
  }
  return (sandbox::SBOX_ALL_OK == target_services_->Init());
}
