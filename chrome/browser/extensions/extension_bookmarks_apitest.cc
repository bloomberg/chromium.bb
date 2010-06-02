// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"

#if defined(OS_WIN)
// On Windows, this test occasionally crash. See http://crbug.com/45595
#define MAYBE_Bookmarks DISABLED_Bookmarks
#else
#define MAYBE_Bookmarks Bookmarks
#endif

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, MAYBE_Bookmarks) {
  ASSERT_TRUE(RunExtensionTest("bookmarks")) << message_;
}
