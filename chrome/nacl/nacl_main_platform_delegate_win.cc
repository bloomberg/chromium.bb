// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/nacl/nacl_main_platform_delegate.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/native_library.h"
#include "chrome/common/chrome_switches.h"
#include "sandbox/win/src/sandbox.h"

NaClMainPlatformDelegate::NaClMainPlatformDelegate(
    const content::MainFunctionParams& parameters)
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
  const CommandLine& command_line = parameters_.command_line;

  DVLOG(1) << "Started NaClLdr with " << command_line.GetCommandLineString();

  sandbox::TargetServices* target_services =
      parameters_.sandbox_info->target_services;

  if (target_services && !no_sandbox) {
    base::FilePath test_dll_name =
      command_line.GetSwitchValuePath(switches::kTestNaClSandbox);
    if (!test_dll_name.empty()) {
      // At this point, hack on the suffix according to with bitness
      // of your windows process.
#if defined(_WIN64)
      DVLOG(1) << "Using 64-bit test dll\n";
      test_dll_name = test_dll_name.InsertBeforeExtension(L"64");
      test_dll_name = test_dll_name.ReplaceExtension(L"dll");
#else
      DVLOG(1) << "Using 32-bit test dll\n";
      test_dll_name = test_dll_name.ReplaceExtension(L"dll");
#endif
      DVLOG(1) << "Loading test lib " << test_dll_name.value() << "\n";
      sandbox_test_module_ = base::LoadNativeLibrary(test_dll_name, NULL);
      CHECK(sandbox_test_module_);
      VLOG(1) << "Testing NaCl sandbox\n";
    }
  }
}

void NaClMainPlatformDelegate::EnableSandbox() {
  sandbox::TargetServices* target_services =
      parameters_.sandbox_info->target_services;

  CHECK(target_services) << "NaCl-Win EnableSandbox: No Target Services!";
  // Cause advapi32 to load before the sandbox is turned on.
  unsigned int dummy_rand;
  rand_s(&dummy_rand);
  // Warm up language subsystems before the sandbox is turned on.
  ::GetUserDefaultLangID();
  ::GetUserDefaultLCID();
  // Turn the sandbox on.
  target_services->LowerToken();
}

bool NaClMainPlatformDelegate::RunSandboxTests() {
  // TODO(jvoung): Win and mac should share this code.
  bool result = true;
  if (sandbox_test_module_) {
    RunNaClLoaderTests run_security_tests =
      reinterpret_cast<RunNaClLoaderTests>(
        base::GetFunctionPointerFromNativeLibrary(sandbox_test_module_,
                                                  kNaClLoaderTestCall));
    if (run_security_tests) {
      DVLOG(1) << "Running NaCl Loader security tests";
      result = (*run_security_tests)();
    } else {
      VLOG(1) << "Failed to get NaCl sandbox test function";
      result = false;
    }
    base::UnloadNativeLibrary(sandbox_test_module_);
    sandbox_test_module_ = NULL;
  }
  return result;
}
