// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <atlbase.h>

#include "base/command_line.h"
#include "base/process_util.h"
#include "base/test/test_suite.h"
#include "base/threading/platform_thread.h"
#include "chrome/common/chrome_paths.h"
#include "chrome_frame/test/chrome_frame_test_utils.h"
#include "chrome_frame/test/chrome_frame_ui_test_utils.h"
#include "chrome_frame/test_utils.h"
#include "chrome_frame/utils.h"

// To enable ATL-based code to run in this module
class ChromeFrameUnittestsModule
    : public CAtlExeModuleT<ChromeFrameUnittestsModule> {
 public:
  static HRESULT InitializeCom() {
    return CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
  }
};

ChromeFrameUnittestsModule _AtlModule;

const char kNoRegistrationSwitch[] = "no-registration";

void PureCall() {
  __debugbreak();
}

// This class implements the Run method and registers an exception handler to
// ensure that any ChromeFrame processes like IE, Firefox, etc are terminated
// if there is a crash in the chrome frame test suite.
class ChromeFrameTestSuite : public base::TestSuite {
 public:
  ChromeFrameTestSuite(int argc, char** argv)
      : base::TestSuite(argc, argv) {}

  int Run() {
    // Register a stack based exception handler to catch any exceptions which
    // occur in the course of the test.
    int ret = -1;
    __try {
      ret = base::TestSuite::Run();
    }

    __except(EXCEPTION_EXECUTE_HANDLER) {
      ret = -1;
    }
    return ret;
  }
};

int main(int argc, char **argv) {
  base::EnableTerminationOnHeapCorruption();
  base::PlatformThread::SetName("ChromeFrame tests");

  _set_purecall_handler(PureCall);

  ChromeFrameTestSuite test_suite(argc, argv);

  SetConfigBool(kChromeFrameHeadlessMode, true);
  SetConfigBool(kChromeFrameAccessibleMode, true);

  base::ProcessHandle crash_service = chrome_frame_test::StartCrashService();
  int ret = -1;
  // If mini_installer is used to register CF, we use the switch
  // --no-registration to avoid repetitive registration.
  if (CommandLine::ForCurrentProcess()->HasSwitch(kNoRegistrationSwitch)) {
    ret = test_suite.Run();
  } else {
    // Register paths needed by the ScopedChromeFrameRegistrar.
    chrome::RegisterPathProvider();

    // This will register the chrome frame in the build directory. It currently
    // leaves that chrome frame registered once the tests are done. It must be
    // constructed AFTER the TestSuite is created since TestSuites create THE
    // AtExitManager.
    // TODO(robertshield): Make these tests restore the original registration
    // once done.
    ScopedChromeFrameRegistrar registrar(chrome_frame_test::GetTestBedType());

    // Register IAccessible2 proxy stub DLL, needed for some tests.
    ScopedChromeFrameRegistrar ia2_registrar(
        chrome_frame_test::GetIAccessible2ProxyStubPath().value(),
        ScopedChromeFrameRegistrar::SYSTEM_LEVEL);
    ret = test_suite.Run();
  }

  if (ret == -1) {
    LOG(ERROR) << "ChromeFrame tests crashed";
    chrome_frame_test::KillProcesses(L"iexplore.exe", 0, false);
    chrome_frame_test::KillProcesses(L"firefox.exe", 0, false);
  }

  DeleteConfigValue(kChromeFrameHeadlessMode);
  DeleteConfigValue(kChromeFrameAccessibleMode);

  if (crash_service)
    base::KillProcess(crash_service, 0, false);
}
