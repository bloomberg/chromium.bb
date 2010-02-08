// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"

// Disabled, http://crbug.com/30151 (Linux and ChromeOS),
// http://crbug.com/35034 (others).
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, DISABLED_Toolstrip) {
  ASSERT_TRUE(RunExtensionTest("toolstrip")) << message_;
}
