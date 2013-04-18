// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <atlbase.h>

#include "base/command_line.h"
#include "base/process_util.h"
#include "base/test/test_suite.h"
#include "base/threading/platform_thread.h"
#include "base/win/scoped_com_initializer.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/metrics/entropy_provider.h"
#include "chrome/test/logging/win/test_log_collector.h"
#include "chrome_frame/crash_server_init.h"
#include "chrome_frame/test/chrome_frame_test_utils.h"
#include "chrome_frame/test/chrome_frame_ui_test_utils.h"
#include "chrome_frame/test/ie_configurator.h"
#include "chrome_frame/test/test_scrubber.h"
#include "chrome_frame/test_utils.h"
#include "chrome_frame/utils.h"

// To enable ATL-based code to run in this module
class ChromeFrameUnittestsModule
    : public CAtlExeModuleT<ChromeFrameUnittestsModule> {
 public:
  // Called at static init time, for versions of ATL included in VS2008 and
  // earlier only.  The default implementation initializes COM in MTA mode,
  // which we don't want.  We could init STA mode here, but since we have to
  // init in main() for VS2010 and above anyway, we simply do nothing, since
  // nothing needs COM before main() runs.
  static HRESULT InitializeCom() { return S_OK; }
  static void UninitializeCom() {}
};

ChromeFrameUnittestsModule _AtlModule;

const char kNoCrashService[] = "no-crash-service";
const char kNoLogCollector[] = "no-log-collector";
const char kNoRegistrationSwitch[] = "no-registration";

void PureCall() {
  __debugbreak();
}

int main(int argc, char **argv) {
  // For ATL in VS2010 and up, ChromeFrameUnittestsModule::InitializeCom() is
  // not called, so we init COM here.
  base::win::ScopedCOMInitializer com_initializer_;

  ScopedChromeFrameRegistrar::RegisterAndExitProcessIfDirected();
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

  // Install the log collector before anything else that adds a Google Test
  // event listener so that it's the last one to run after each test (the
  // listeners are invoked in reverse order at the end of a test).  This allows
  // the collector to emit logs if other listeners ADD_FAILURE or EXPECT_*.
  if (!CommandLine::ForCurrentProcess()->HasSwitch(kNoLogCollector))
    logging_win::InstallTestLogCollector(testing::UnitTest::GetInstance());

  chrome_frame_test::InstallIEConfigurator();

  chrome_frame_test::InstallTestScrubber(testing::UnitTest::GetInstance());

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
