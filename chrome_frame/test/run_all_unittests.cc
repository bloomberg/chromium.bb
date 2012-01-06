// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <atlbase.h>

#include "base/command_line.h"
#include "base/process_util.h"
#include "base/test/test_suite.h"
#include "base/threading/platform_thread.h"
#include "chrome/common/chrome_paths.h"
#include "chrome_frame/crash_server_init.h"
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

const char kNoCrashService[] = "no-crash-service";
const char kNoRegistrationSwitch[] = "no-registration";

void PureCall() {
  __debugbreak();
}

int main(int argc, char **argv) {
  base::EnableTerminationOnHeapCorruption();
  base::PlatformThread::SetName("ChromeFrame tests");

  _set_purecall_handler(PureCall);

  base::TestSuite test_suite(argc, argv);

  SetConfigBool(kChromeFrameHeadlessMode, true);
  SetConfigBool(kChromeFrameAccessibleMode, true);

  base::ProcessHandle crash_service = NULL;
  google_breakpad::scoped_ptr<google_breakpad::ExceptionHandler> breakpad;

  if (!CommandLine::ForCurrentProcess()->HasSwitch(kNoCrashService)) {
    crash_service = chrome_frame_test::StartCrashService();

    breakpad.reset(InitializeCrashReporting(HEADLESS));
  }

  int ret = -1;
  // If mini_installer is used to register CF, we use the switch
  // --no-registration to avoid repetitive registration.
  if (CommandLine::ForCurrentProcess()->HasSwitch(kNoRegistrationSwitch)) {
    ret = test_suite.Run();
  } else {
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

  DeleteConfigValue(kChromeFrameHeadlessMode);
  DeleteConfigValue(kChromeFrameAccessibleMode);

  if (crash_service)
    base::KillProcess(crash_service, 0, false);
  return ret;
}
