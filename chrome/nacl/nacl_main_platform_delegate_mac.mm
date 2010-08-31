// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/nacl/nacl_main_platform_delegate.h"

#import <Cocoa/Cocoa.h>
#import "base/chrome_application_mac.h"
#include "base/command_line.h"
#include "base/file_path.h"
#include "base/logging.h"
#include "base/native_library.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/sandbox_mac.h"
#include "third_party/WebKit/WebKit/mac/WebCoreSupport/WebSystemInterface.h"

NaClMainPlatformDelegate::NaClMainPlatformDelegate(
    const MainFunctionParams& parameters)
    : parameters_(parameters), sandbox_test_module_(NULL) {
}

NaClMainPlatformDelegate::~NaClMainPlatformDelegate() {
}

// TODO(jvoung): see if this old comment (from renderer_main_platform...)
// is relevant to the nacl loader.
// TODO(mac-port): Any code needed to initialize a process for purposes of
// running a NaClLoader needs to also be reflected in chrome_dll_main.cc for
// --single-process support.
void NaClMainPlatformDelegate::PlatformInitialize() {
}

void NaClMainPlatformDelegate::PlatformUninitialize() {
}

void NaClMainPlatformDelegate::InitSandboxTests(bool no_sandbox) {
  const CommandLine& command_line = parameters_.command_line_;

  DLOG(INFO) << "Started NaClLdr with ";
  const std::vector<std::string>& argstrings = command_line.argv();
  for (std::vector<std::string>::const_iterator ii = argstrings.begin();
       ii != argstrings.end(); ++ii) {
    DLOG(INFO) << *ii;
  }

  // Be sure not to load the sandbox test DLL if the sandbox isn't on.
  // Comment-out guard and recompile if you REALLY want to test w/out the SB.
  // TODO(jvoung): allow testing without sandbox, but change expected ret vals.
  if (!no_sandbox) {
    FilePath test_dll_name =
      command_line.GetSwitchValuePath(switches::kTestNaClSandbox);
    if (!test_dll_name.empty()) {
      sandbox_test_module_ = base::LoadNativeLibrary(test_dll_name);
      CHECK(sandbox_test_module_);
    }
  }
}

void NaClMainPlatformDelegate::EnableSandbox() {
  CommandLine* parsed_command_line = CommandLine::ForCurrentProcess();
  SandboxInitWrapper sandbox_wrapper;
  bool sandbox_initialized_ok =
       sandbox_wrapper.InitializeSandbox(*parsed_command_line,
                                         switches::kNaClLoaderProcess);
  CHECK(sandbox_initialized_ok) << "Error initializing sandbox for "
                                << switches::kNaClLoaderProcess;
}

bool NaClMainPlatformDelegate::RunSandboxTests() {
  // TODO(jvoung): Win and mac should share this identical code.
  bool result = true;
  if (sandbox_test_module_) {
    RunNaClLoaderTests run_security_tests =
      reinterpret_cast<RunNaClLoaderTests>(
        base::GetFunctionPointerFromNativeLibrary(sandbox_test_module_,
                                                  kNaClLoaderTestCall));
    if (run_security_tests) {
      DLOG(INFO) << "Running NaCl Loader security tests";
      result = (*run_security_tests)();
    } else {
      LOG(INFO) << "Failed to get NaCl sandbox test function";
      result = false;
    }
    base::UnloadNativeLibrary(sandbox_test_module_);
    sandbox_test_module_ = NULL;
  }
  return result;
}
