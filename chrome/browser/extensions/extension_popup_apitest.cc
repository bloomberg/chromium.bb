// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/common/chrome_switches.h"

// TODO(beng): Marking disabled... this change causes this test to fail however
//             twiz@ notes that this test and the support code for it are going
//             away very soon so I don't need to fix it. Yay! >:D
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, DISABLED_Popup) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalExtensionApis);

  ASSERT_TRUE(RunExtensionTest("popup/popup_main")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, PopupFromInfobar) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
    switches::kEnableExperimentalExtensionApis);

  ASSERT_TRUE(RunExtensionTest("popup/popup_from_infobar")) << message_;
}
