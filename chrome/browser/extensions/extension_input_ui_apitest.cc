// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/common/chrome_switches.h"

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, InputUI) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalExtensionApis);
#if defined(OS_CHROMEOS) && defined(USE_VIRTUAL_KEYBOARD)
  ASSERT_TRUE(RunExtensionTest("input_ui/chromeos_virtual_keyboard")) <<
      message_;
#else
  ASSERT_TRUE(RunExtensionTest("input_ui/other")) << message_;
#endif
}
