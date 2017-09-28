// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"

using ExtensionModuleTest = ExtensionApiTest;

IN_PROC_BROWSER_TEST_F(ExtensionModuleTest, TestModulesAvailable) {
  ASSERT_TRUE(RunExtensionTest("modules")) << message_;
}
