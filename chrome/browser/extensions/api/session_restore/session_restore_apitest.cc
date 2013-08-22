// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/test/base/test_switches.h"

// Flaky on ChromeOS, times out on OSX Debug http://crbug.com/251199
#if defined(OS_CHROMEOS) || (defined(OS_MACOSX) && !defined(NDEBUG))
#define MAYBE_SessionRestoreApis DISABLED_SessionRestoreApis
#else
#define MAYBE_SessionRestoreApis SessionRestoreApis
#endif
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, MAYBE_SessionRestoreApis) {
#if defined(OS_WIN) && defined(USE_ASH)
  // Disable this test in Metro+Ash for now (http://crbug.com/262796).
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kAshBrowserTests))
    return;
#endif

  ASSERT_TRUE(RunExtensionSubtest("session_restore",
                                  "session_restore.html")) << message_;
}
