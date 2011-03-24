// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/extensions/extension_apitest.h"

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, CustomizationPrivateTest) {
  ASSERT_TRUE(RunComponentExtensionTest("chromeos_info_private")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, CustomizationPrivateFailTest) {
  // Only component extensions can use it.
  ASSERT_FALSE(RunExtensionTest("chromeos_info_private")) << message_;
}
