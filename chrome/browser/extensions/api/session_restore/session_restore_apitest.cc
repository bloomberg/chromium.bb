// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"

// Flaky on ChromeOS, times out on OSX Debug http://crbug.com/251199
#if defined(OS_CHROMEOS) || (defined(OS_MACOSX) && !defined(NDEBUG))
#define MAYBE_SessionRestoreApis DISABLED_SessionRestoreApis
#else
#define MAYBE_SessionRestoreApis SessionRestoreApis
#endif
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, MAYBE_SessionRestoreApis) {
  ASSERT_TRUE(RunExtensionSubtest("session_restore",
                                  "session_restore.html")) << message_;
}
