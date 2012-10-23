// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/sandbox_init.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "content/common/sandbox_policy.h"
#include "content/public/common/content_switches.h"
#include "sandbox/win/src/sandbox.h"
#include "sandbox/win/src/sandbox_types.h"

namespace content {

bool InitializeSandbox(sandbox::SandboxInterfaceInfo* sandbox_info) {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  std::string process_type =
      command_line.GetSwitchValueASCII(switches::kProcessType);
  sandbox::BrokerServices* broker_services = sandbox_info->broker_services;
  if (broker_services && (process_type.empty() ||
                          process_type == switches::kNaClBrokerProcess ||
                          process_type == switches::kServiceProcess)) {
    if (!InitBrokerServices(broker_services))
      return false;
  }

  if (process_type.empty() || process_type == switches::kNaClBrokerProcess) {
    // IMPORTANT: This piece of code needs to run as early as possible in the
    // process because it will initialize the sandbox broker, which requires the
    // process to swap its window station. During this time all the UI will be
    // broken. This has to run before threads and windows are created.
    if (broker_services) {
      if (!command_line.HasSwitch(switches::kNoSandbox)) {
        bool use_winsta = !command_line.HasSwitch(
            switches::kDisableAltWinstation);
        // Precreate the desktop and window station used by the renderers.
        sandbox::TargetPolicy* policy = broker_services->CreatePolicy();
        sandbox::ResultCode result = policy->CreateAlternateDesktop(use_winsta);
        CHECK(sandbox::SBOX_ERROR_FAILED_TO_SWITCH_BACK_WINSTATION != result);
        policy->Release();
      }
    }
    return true;
  }

  if (command_line.HasSwitch(switches::kNoSandbox))
    return true;

  sandbox::TargetServices* target_services = sandbox_info->target_services;
  if ((process_type == switches::kRendererProcess) ||
      (process_type == switches::kWorkerProcess) ||
      (process_type == switches::kNaClLoaderProcess) ||
      (process_type == switches::kUtilityProcess)) {
    // The above five process types must be sandboxed unless --no-sandbox
    // is present in the command line.
    if (!target_services)
      return false;
  } else {
    // Other process types might or might not be sandboxed.
    // TODO(cpu): clean this mess.
    if (!target_services)
      return true;
  }
  return InitTargetServices(target_services);
}

}  // namespace content
