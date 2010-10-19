// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"


IN_PROC_BROWSER_TEST_F(ExtensionApiTest, ContentScriptFragmentNavigation) {
  ASSERT_TRUE(StartTestServer());
  const char* extension_name = "content_scripts/fragment";
  ASSERT_TRUE(RunExtensionTest(extension_name)) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, ExecuteScriptFragmentNavigation) {
  ASSERT_TRUE(StartTestServer());
  const char* extension_name = "executescript/fragment";
  ASSERT_TRUE(RunExtensionTest(extension_name)) << message_;
}
