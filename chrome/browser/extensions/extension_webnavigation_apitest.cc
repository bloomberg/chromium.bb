// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/common/chrome_switches.h"


IN_PROC_BROWSER_TEST_F(ExtensionApiTest, WebNavigation) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalExtensionApis);

  ASSERT_TRUE(RunExtensionTest("webnavigation/api")) << message_;
}

// Disabled, flakily exceeds timeout, http://crbug.com/72165.
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, DISABLED_WebNavigationEvents1) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalExtensionApis);

  ASSERT_TRUE(RunExtensionTest("webnavigation/navigation1")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, WebNavigationEvents2) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalExtensionApis);

  ASSERT_TRUE(RunExtensionTest("webnavigation/navigation2")) << message_;
}
