// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/test/base/ui_test_utils.h"

IN_PROC_BROWSER_TEST_F(ExtensionApiTest,
                       ExtensionFullscreenAccessFail) {
  // Test that fullscreen can be accessed from an extension without permission.
  ASSERT_TRUE(RunPlatformAppTest("fullscreen/no_permission")) << message_;
}

// Disabled, a user gesture is required for fullscreen. http://crbug.com/174178
IN_PROC_BROWSER_TEST_F(ExtensionApiTest,
                       DISABLED_ExtensionFullscreenAccessPass) {
  // Test that fullscreen can be accessed from an extension with permission.
  ASSERT_TRUE(RunPlatformAppTest("fullscreen/has_permission")) << message_;
}
