// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <atlbase.h>

#include "base/process_util.h"
#include "base/test/test_suite.h"
#include "base/command_line.h"
#include "chrome/common/chrome_paths.h"
#include "chrome_frame/test/chrome_frame_test_utils.h"
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

const wchar_t kNoRegistrationSwitch[] = L"no-registration";

void PureCall() {
  __debugbreak();
}

int main(int argc, char **argv) {
  base::EnableTerminationOnHeapCorruption();
  PlatformThread::SetName("ChromeFrame tests");

  _set_purecall_handler(PureCall);

  TestSuite test_suite(argc, argv);

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
    ScopedChromeFrameRegistrar registrar;
    ret = test_suite.Run();
  }

  DeleteConfigValue(kChromeFrameHeadlessMode);
  DeleteConfigValue(kChromeFrameAccessibleMode);

  if (crash_service)
    base::KillProcess(crash_service, 0, false);
}
