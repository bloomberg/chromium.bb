// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base_switches.h"
#include "base/command_line.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "net/base/mock_host_resolver.h"

class ExtensionHistoryApiTest : public ExtensionApiTest {
 public:
  virtual void SetUpInProcessBrowserTestFixture() {
    ExtensionApiTest::SetUpInProcessBrowserTestFixture();

    host_resolver()->AddRule("www.a.com", "127.0.0.1");
    host_resolver()->AddRule("www.b.com", "127.0.0.1");

    ASSERT_TRUE(StartTestServer());
  }

  virtual void SetUpCommandLine(CommandLine* command_line) {
    ExtensionApiTest::SetUpCommandLine(command_line);

    // Tests are flaky, but only on the waterfall (crbug.com/26296).
    // Failures involve a call to chrome.history.search() not finding
    // a result whose addition caused event chrome.history.onVisited
    // to fire.  Turn on verbose logging.
    // TODO(skerner): Remove this once the flakiness is fixed.
    command_line->AppendSwitchASCII(switches::kVModule, "*/history/*=1");
  }
};

// Flaky, http://crbug.com/26296.
IN_PROC_BROWSER_TEST_F(ExtensionHistoryApiTest, MiscSearch) {
  ASSERT_TRUE(RunExtensionSubtest("history", "misc_search.html")) << message_;
}

// Flaky, http://crbug.com/26296.
IN_PROC_BROWSER_TEST_F(ExtensionHistoryApiTest, TimedSearch) {
  ASSERT_TRUE(RunExtensionSubtest("history", "timed_search.html")) << message_;
}

// Flaky, http://crbug.com/26296.
IN_PROC_BROWSER_TEST_F(ExtensionHistoryApiTest, Delete) {
  ASSERT_TRUE(RunExtensionSubtest("history", "delete.html")) << message_;
}

// Flaky, http://crbug.com/26296.
IN_PROC_BROWSER_TEST_F(ExtensionHistoryApiTest, GetVisits) {
  ASSERT_TRUE(RunExtensionSubtest("history", "get_visits.html")) << message_;
}
