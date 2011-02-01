// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"
#include "net/base/mock_host_resolver.h"

class ExecuteScriptApiTest : public ExtensionApiTest {
 protected:
  void SetupDelayedHostResolver() {
    // We need a.com to be a little bit slow to trigger a race condition.
    host_resolver()->AddRuleWithLatency("a.com", "127.0.0.1", 500);
    host_resolver()->AddRule("b.com", "127.0.0.1");
    host_resolver()->AddRule("c.com", "127.0.0.1");
  }
};

IN_PROC_BROWSER_TEST_F(ExecuteScriptApiTest, ExecuteScriptBasic) {
  SetupDelayedHostResolver();
  ASSERT_TRUE(StartTestServer());
  ASSERT_TRUE(RunExtensionTest("executescript/basic")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExecuteScriptApiTest, ExecuteScriptInFrame) {
  SetupDelayedHostResolver();
  ASSERT_TRUE(StartTestServer());
  ASSERT_TRUE(RunExtensionTest("executescript/in_frame")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExecuteScriptApiTest, ExecuteScriptPermissions) {
  SetupDelayedHostResolver();
  ASSERT_TRUE(StartTestServer());
  ASSERT_TRUE(RunExtensionTest("executescript/permissions")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExecuteScriptApiTest, ExecuteScriptFileAfterClose) {
  host_resolver()->AddRule("b.com", "127.0.0.1");
  ASSERT_TRUE(StartTestServer());
  ASSERT_TRUE(RunExtensionTest("executescript/file_after_close")) << message_;
}

// Crashy, http://crbug.com/67774.
IN_PROC_BROWSER_TEST_F(ExecuteScriptApiTest,
                       DISABLED_ExecuteScriptFragmentNavigation) {
  ASSERT_TRUE(StartTestServer());
  const char* extension_name = "executescript/fragment";
  ASSERT_TRUE(RunExtensionTest(extension_name)) << message_;
}
