// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/common/chrome_switches.h"

// Flaky on windows.  http://crbug.com/46601
#if defined(OS_WIN)
#define MAYBE_Popup FLAKY_Popup
#else
#define MAYBE_Popup Popup
#endif

// Disabled for some rewriting.   http://crbug.com/51821
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, DISABLED_Popup) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalExtensionApis);

  ASSERT_TRUE(RunExtensionTest("popup")) << message_;
}
