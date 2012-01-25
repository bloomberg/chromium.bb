// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"

class ExtensionTerminalPrivateApiTest : public ExtensionApiTest {
};

IN_PROC_BROWSER_TEST_F(ExtensionTerminalPrivateApiTest, TerminalTest) {
  EXPECT_TRUE(RunComponentExtensionTest("terminal/component_extension"))
      << message_;
};

IN_PROC_BROWSER_TEST_F(ExtensionTerminalPrivateApiTest, NoPermission) {
  EXPECT_TRUE(RunExtensionTest("terminal/no_permission")) << message_;
};
