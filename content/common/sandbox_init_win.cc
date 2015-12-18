// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/sandbox_init.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "content/common/sandbox_win.h"
#include "content/public/common/content_switches.h"
#include "sandbox/win/src/sandbox.h"
#include "sandbox/win/src/sandbox_types.h"

namespace content {

bool InitializeSandbox(sandbox::SandboxInterfaceInfo* sandbox_info) {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  sandbox::BrokerServices* broker_services = sandbox_info->broker_services;
  if (broker_services) {
    if (!InitBrokerServices(broker_services))
      return false;

    // IMPORTANT: This piece of code needs to run as early as possible in the
    // process because it will initialize the sandbox broker, which requires the
    // process to swap its window station. During this time all the UI will be
    // broken. This has to run before threads and windows are created.
    if (!command_line.HasSwitch(switches::kNoSandbox)) {
      // Precreate the desktop and window station used by the renderers.
      sandbox::TargetPolicy* policy = broker_services->CreatePolicy();
      sandbox::ResultCode result = policy->CreateAlternateDesktop(true);
      CHECK(sandbox::SBOX_ERROR_FAILED_TO_SWITCH_BACK_WINSTATION != result);
      policy->Release();
    }
    return true;
  }

  if (command_line.HasSwitch(switches::kNoSandbox))
    return true;

  sandbox::TargetServices* target_services = sandbox_info->target_services;
  return InitTargetServices(target_services);
}

bool BrokerDuplicateSharedMemoryHandle(
    const base::SharedMemoryHandle& source_handle,
    base::ProcessId target_process_id,
    base::SharedMemoryHandle* target_handle) {
  HANDLE duped_handle;
  if (!BrokerDuplicateHandle(
          source_handle.GetHandle(), target_process_id, &duped_handle,
          FILE_MAP_READ | FILE_MAP_WRITE | SECTION_QUERY, 0)) {
    return false;
  }

  *target_handle = base::SharedMemoryHandle(duped_handle, target_process_id);
  return true;
}

}  // namespace content
