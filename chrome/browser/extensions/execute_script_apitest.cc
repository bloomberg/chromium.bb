// Copyright (c) 20109 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"
#include "net/base/mock_host_resolver.h"

// EXTREMELY flaky, crashy, and bad. See http://crbug.com/28630 and don't dare
// to re-enable without a real fix or at least adding more debugging info.
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, DISABLED_ExecuteScript) {
  // We need a.com to be a little bit slow to trigger a race condition.
  host_resolver()->AddRuleWithLatency("a.com", "127.0.0.1", 500);
  host_resolver()->AddRule("b.com", "127.0.0.1");
  host_resolver()->AddRule("c.com", "127.0.0.1");
  ASSERT_TRUE(StartHTTPServer());

  ASSERT_TRUE(RunExtensionTest("executescript/basic")) << message_;
  ASSERT_TRUE(RunExtensionTest("executescript/in_frame")) << message_;
  ASSERT_TRUE(RunExtensionTest("executescript/permissions")) << message_;
}

// TODO(rafaelw) - This case is split out per Pawel's request. When the above
// (ExecuteScript) tests are de-flakified, reunite this case with it's brethern.
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, ExecuteScriptFileAfterClose) {
  host_resolver()->AddRule("b.com", "127.0.0.1");
  ASSERT_TRUE(StartHTTPServer());

  ASSERT_TRUE(RunExtensionTest("executescript/file_after_close")) << message_;
}
