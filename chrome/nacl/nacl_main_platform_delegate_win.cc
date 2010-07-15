// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/nacl/nacl_main_platform_delegate.h"
#include "base/command_line.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "sandbox/src/sandbox.h"

NaClMainPlatformDelegate::NaClMainPlatformDelegate(
    const MainFunctionParams& parameters)
        : parameters_(parameters), sandbox_test_module_(NULL) {
}

NaClMainPlatformDelegate::~NaClMainPlatformDelegate() {
}

void NaClMainPlatformDelegate::PlatformInitialize() {
  // Be mindful of what resources you acquire here. They can be used by
  // malicious code if the renderer gets compromised.
}

void NaClMainPlatformDelegate::PlatformUninitialize() {
}

void NaClMainPlatformDelegate::InitSandboxTests(bool no_sandbox) {
  const CommandLine& command_line = parameters_.command_line_;

  DLOG(INFO) << "Started NaClLdr with " << command_line.command_line_string();

  sandbox::TargetServices* target_services =
      parameters_.sandbox_info_.TargetServices();

  if (target_services && !no_sandbox) {
    std::wstring test_dll_name =
      command_line.GetSwitchValue(switches::kTestNaClSandbox);
    if (!test_dll_name.empty()) {
      // At this point, hack on the suffix according to with bitness
      // of your windows process.
#if defined(_WIN64)
      DLOG(INFO) << "Using 64-bit test dll\n";
      test_dll_name.append(L"64.dll");
#else
      DLOG(INFO) << "Using 32-bit test dll\n";
      test_dll_name.append(L".dll");
#endif
      DLOG(INFO) << "Loading test lib " << test_dll_name << "\n";
      sandbox_test_module_ = LoadLibrary(test_dll_name.c_str());
      CHECK(sandbox_test_module_);
      LOG(INFO) << "Testing NaCl sandbox\n";
    }
  }
  return;
}

bool NaClMainPlatformDelegate::EnableSandbox() {
  sandbox::TargetServices* target_services =
      parameters_.sandbox_info_.TargetServices();

  if (target_services) {
    // Cause advapi32 to load before the sandbox is turned on.
    unsigned int dummy_rand;
    rand_s(&dummy_rand);
    // Turn the sandbox on.
    target_services->LowerToken();
    return true;
  }
  return false;
}

void NaClMainPlatformDelegate::RunSandboxTests() {
  if (sandbox_test_module_) {
    RunNaClLoaderTests run_security_tests = reinterpret_cast<RunNaClLoaderTests>
      (GetProcAddress(sandbox_test_module_, kNaClLoaderTestCall));

    CHECK(run_security_tests);
    if (run_security_tests) {
      DLOG(INFO) << "Running NaCl Loader security tests";
      CHECK((*run_security_tests)());
    }
    FreeLibrary(sandbox_test_module_);
  }
}

