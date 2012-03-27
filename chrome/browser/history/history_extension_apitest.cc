// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base_switches.h"
#include "base/command_line.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "net/base/mock_host_resolver.h"

class HistoryExtensionApiTest : public ExtensionApiTest {
 public:
  virtual void SetUpInProcessBrowserTestFixture() {
    ExtensionApiTest::SetUpInProcessBrowserTestFixture();

    host_resolver()->AddRule("www.a.com", "127.0.0.1");
    host_resolver()->AddRule("www.b.com", "127.0.0.1");

    ASSERT_TRUE(StartTestServer());
  }
};

// Full text search indexing sometimes exceeds a timeout. (http://crbug/119505)
IN_PROC_BROWSER_TEST_F(HistoryExtensionApiTest, DISABLED_MiscSearch) {
  ASSERT_TRUE(RunExtensionSubtest("history", "misc_search.html")) << message_;
}

// Same could happen here without the FTS (http://crbug/119505)
IN_PROC_BROWSER_TEST_F(HistoryExtensionApiTest, DISABLED_TimedSearch) {
  ASSERT_TRUE(RunExtensionSubtest("history", "timed_search.html")) << message_;
}

#if defined(OS_WIN)
// Flaky on Windows: crbug.com/88318
#define MAYBE_Delete DISABLED_Delete
#else
#define MAYBE_Delete Delete
#endif
IN_PROC_BROWSER_TEST_F(HistoryExtensionApiTest, MAYBE_Delete) {
  ASSERT_TRUE(RunExtensionSubtest("history", "delete.html")) << message_;
}

// See crbug.com/79074
IN_PROC_BROWSER_TEST_F(HistoryExtensionApiTest, DISABLED_GetVisits) {
  ASSERT_TRUE(RunExtensionSubtest("history", "get_visits.html")) << message_;
}

#if defined(OS_WIN)
// Searching for a URL right after adding it fails on win XP.
// Fix this as part of crbug/76170.
#define MAYBE_SearchAfterAdd DISABLED_SearchAfterAdd
#else
#define MAYBE_SearchAfterAdd SearchAfterAdd
#endif

IN_PROC_BROWSER_TEST_F(HistoryExtensionApiTest, MAYBE_SearchAfterAdd) {
  ASSERT_TRUE(RunExtensionSubtest("history", "search_after_add.html"))
      << message_;
}
