// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/browser/extensions/extension_apitest.h"

namespace extensions {

// Times out on win asan, http://crbug.com/166026
#if defined(OS_WIN) && defined(ADDRESS_SANITIZER)
#define MAYBE_Cookies DISABLED_Cookies
#else
#define MAYBE_Cookies Cookies
#endif
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, MAYBE_Cookies) {
  ASSERT_TRUE(RunExtensionTest("cookies/api")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, CookiesEvents) {
  ASSERT_TRUE(RunExtensionTest("cookies/events")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, CookiesNoPermission) {
  ASSERT_TRUE(RunExtensionTest("cookies/no_permission")) << message_;
}

}  // namespace extensions
