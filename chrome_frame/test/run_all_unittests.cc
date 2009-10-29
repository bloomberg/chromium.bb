// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <atlbase.h>
#include <iostream>

#include "base/at_exit.h"
#include "base/platform_thread.h"
#include "base/process_util.h"
#include "base/test/test_suite.h"
#include "base/command_line.h"
#include "chrome/common/chrome_paths.h"

#include "chrome_frame/test/http_server.h"
#include "chrome_frame/test_utils.h"

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

// Causes the test executable to just run the web server and quit when the
// server is killed. Useful for debugging tests.
const wchar_t kRunAsServer[] = L"server";

int main(int argc, char **argv) {
  base::EnableTerminationOnHeapCorruption();
  PlatformThread::SetName("ChromeFrame tests");

  TestSuite test_suite(argc, argv);

  if (CommandLine::ForCurrentProcess()->HasSwitch(kRunAsServer)) {
    ChromeFrameHTTPServer server;
    server.SetUp();
    GURL server_url(server.server()->TestServerPage(""));
    std::cout << std::endl
              << "Server waiting on " << server_url.spec().c_str()
              << std::endl << std::endl
              << "Test output will be written to "
              << server.server()->GetDataDirectory().value().c_str() << "\\dump"
              << std::endl << std::endl
              << "Hit Ctrl-C or navigate to "
              << server_url.spec().c_str() << "kill to shut down the server."
              << std::endl;
    server.WaitToFinish(INFINITE);
    server.TearDown();
    std::cout << "Server stopped.";
    return 0;
  }

  // If mini_installer is used to register CF, we use the switch
  // --no-registration to avoid repetitive registration.
  if (CommandLine::ForCurrentProcess()->HasSwitch(kNoRegistrationSwitch)) {
    return test_suite.Run();
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

    return test_suite.Run();
  }
}
