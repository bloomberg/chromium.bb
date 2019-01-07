// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/command_line.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class SerialTest : public ContentBrowserTest {
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ContentBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII("enable-blink-features", "Serial");
  }
};

IN_PROC_BROWSER_TEST_F(SerialTest, GetPorts) {
  NavigateToURL(shell(), GetTestUrl(nullptr, "simple_page.html"));

  int result;
  EXPECT_TRUE(ExecuteScriptAndExtractInt(
      shell(),
      "navigator.serial.getPorts()"
      "    .then(ports => {"
      "        domAutomationController.send(ports.length);"
      "    });",
      &result));
  EXPECT_EQ(0, result);
}

IN_PROC_BROWSER_TEST_F(SerialTest, RequestPort) {
  NavigateToURL(shell(), GetTestUrl(nullptr, "simple_page.html"));

  bool result;
  EXPECT_TRUE(
      ExecuteScriptAndExtractBool(shell(),
                                  "navigator.serial.requestPort({})"
                                  "    .then(port => {"
                                  "        domAutomationController.send(true);"
                                  "    }, error => {"
                                  "        domAutomationController.send(false);"
                                  "    });",
                                  &result));
  EXPECT_FALSE(result);
}

}  // namespace content
