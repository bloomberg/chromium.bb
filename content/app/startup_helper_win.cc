// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/app/startup_helper_win.h"

#include <crtdbg.h>
#include <new.h>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/win/windows_version.h"
#include "sandbox/src/dep.h"
#include "sandbox/src/sandbox_factory.h"

namespace {

#pragma optimize("", off)
// Handlers for invalid parameter and pure call. They generate a breakpoint to
// tell breakpad that it needs to dump the process.
void InvalidParameter(const wchar_t* expression, const wchar_t* function,
                      const wchar_t* file, unsigned int line,
                      uintptr_t reserved) {
  __debugbreak();
  _exit(1);
}

void PureCall() {
  __debugbreak();
  _exit(1);
}
#pragma optimize("", on)

}  // namespace

namespace content {

void InitializeSandboxInfo(sandbox::SandboxInterfaceInfo* info) {
  info->broker_services = sandbox::SandboxFactory::GetBrokerServices();
  if (!info->broker_services)
    info->target_services = sandbox::SandboxFactory::GetTargetServices();

  if (base::win::GetVersion() < base::win::VERSION_VISTA) {
    // Enforces strong DEP support. Vista uses the NXCOMPAT flag in the exe.
    sandbox::SetCurrentProcessDEP(sandbox::DEP_ENABLED);
  }
}

// Register the invalid param handler and pure call handler to be able to
// notify breakpad when it happens.
void RegisterInvalidParamHandler() {
  _set_invalid_parameter_handler(InvalidParameter);
  _set_purecall_handler(PureCall);
  // Also enable the new handler for malloc() based failures.
  _set_new_mode(1);
}

void SetupCRT(const CommandLine& command_line) {
#if defined(_CRTDBG_MAP_ALLOC)
  _CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDERR);
  _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
#else
  if (!command_line.HasSwitch(switches::kDisableBreakpad)) {
    _CrtSetReportMode(_CRT_ASSERT, 0);
  }
#endif
}

}  // namespace content
