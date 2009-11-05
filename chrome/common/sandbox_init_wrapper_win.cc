// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/sandbox_init_wrapper.h"

#include "base/command_line.h"
#include "chrome/common/chrome_switches.h"

void SandboxInitWrapper::SetServices(sandbox::SandboxInterfaceInfo* info) {
  if (info) {
    broker_services_ = info->broker_services;
    target_services_ = info->target_services;
  }
}

bool SandboxInitWrapper::InitializeSandbox(const CommandLine& command_line,
                                           const std::string& process_type) {
  if (command_line.HasSwitch(switches::kNoSandbox))
    return true;
  if ((process_type == switches::kRendererProcess) ||
      (process_type == switches::kWorkerProcess) ||
      (process_type == switches::kNaClProcess) ||
      (process_type == switches::kUtilityProcess) ||
      (process_type == switches::kPluginProcess &&
       command_line.HasSwitch(switches::kSafePlugins))) {
    if (!target_services_)
      return false;
    target_services_->Init();
  }
  return true;
}
