// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/autotest_private/autotest_private_api.h"
#include "chrome/browser/extensions/api/autotest_private/autotest_private_api_factory.h"
#include "chrome/browser/extensions/extension_apitest.h"

// Times out on win asan, http://crbug.com/166026
#if defined(OS_WIN) && defined(ADDRESS_SANITIZER)
#define MAYBE_AutotestPrivate DISABLED_AutotestPrivate
#else
#define MAYBE_AutotestPrivate AutotestPrivate
#endif
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, MAYBE_AutotestPrivate) {
  // Turn on testing mode so we don't kill the browser.
  extensions::AutotestPrivateAPIFactory::GetForProfile(
      browser()->profile())->set_test_mode(true);
  ASSERT_TRUE(RunComponentExtensionTest("autotest_private")) << message_;
}
