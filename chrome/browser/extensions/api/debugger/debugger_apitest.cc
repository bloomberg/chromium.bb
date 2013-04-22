// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/common/chrome_switches.h"

// Debugger is flaky on browser_tests on Windows: crbug.com/234166.
#if defined(OS_WIN)
#define MAYBE(x) DISABLED_##x
#else
#define MAYBE(x) x
#endif

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, MAYBE(Debugger)) {
  ASSERT_TRUE(RunExtensionTest("debugger")) << message_;
}
