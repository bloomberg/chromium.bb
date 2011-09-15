// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"
#include "net/base/mock_host_resolver.h"

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, CrossOriginXHRBackgroundPage) {
  host_resolver()->AddRule("*.com", "127.0.0.1");
  ASSERT_TRUE(StartTestServer());
  ASSERT_TRUE(RunExtensionTest("cross_origin_xhr/background_page")) << message_;
}

// Flaky on the trybots. See http://crbug.com/96725.
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, FLAKY_CrossOriginXHRContentScript) {
  host_resolver()->AddRule("*.com", "127.0.0.1");
  ASSERT_TRUE(StartTestServer());
  ASSERT_TRUE(RunExtensionTest("cross_origin_xhr/content_script")) << message_;
}
