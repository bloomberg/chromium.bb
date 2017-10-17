// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/command_line.h"
#include "base/process/launch.h"
#include "base/test/launcher/test_launcher.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "content/public/common/service_manager_connection.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/test_launcher.h"
#include "content/shell/browser/shell_content_browser_client.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/test/echo/public/interfaces/echo.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::HasSubstr;
using testing::Not;

namespace content {
namespace {

bool ShouldTerminateOnServiceQuit(const service_manager::Identity& id) {
  return id.name() == echo::mojom::kServiceName;
}

}  // namespace

using ServiceManagerContextTest = ContentBrowserTest;

// "MANUAL" tests only run when kRunManualTestsFlag is set.
IN_PROC_BROWSER_TEST_F(ServiceManagerContextTest,
                       MANUAL_TerminateOnServiceQuit) {
  ShellContentBrowserClient::Get()
      ->set_should_terminate_on_service_quit_callback(
          base::Bind(&ShouldTerminateOnServiceQuit));

  // Launch a test service.
  echo::mojom::EchoPtr echo_ptr;
  content::ServiceManagerConnection::GetForProcess()
      ->GetConnector()
      ->BindInterface(echo::mojom::kServiceName, &echo_ptr);

  base::RunLoop loop;
  // Terminate the service. Browser should exit in response with an error.
  echo_ptr->Quit();
  loop.Run();
}

// Verifies that if an important service quits then the browser exits.
IN_PROC_BROWSER_TEST_F(ServiceManagerContextTest, TerminateOnServiceQuit) {
  // Run the above test and collect the test output.
  base::CommandLine new_test =
      base::CommandLine(base::CommandLine::ForCurrentProcess()->GetProgram());
  new_test.AppendSwitchASCII(
      base::kGTestFilterFlag,
      "ServiceManagerContextTest.MANUAL_TerminateOnServiceQuit");
  new_test.AppendSwitch(kRunManualTestsFlag);
  new_test.AppendSwitch(kSingleProcessTestsFlag);

  base::ScopedAllowBlockingForTesting allow;
  std::string output;
  base::GetAppOutputAndError(new_test, &output);

#if !defined(OS_ANDROID)
  // The test output contains the failure message.
  // TODO(jamescook): The |output| is always empty on Android. I suspect the
  // test runner does logs collection after the program has exited.
  EXPECT_THAT(output, HasSubstr("Terminating because service 'echo' quit"));
#endif
}

}  // namespace content
