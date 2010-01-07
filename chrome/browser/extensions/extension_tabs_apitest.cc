// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, TabsBasics) {
  StartHTTPServer();
  ASSERT_TRUE(RunExtensionTest("tabs/basics")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, TabsConnect) {
  StartHTTPServer();
  ASSERT_TRUE(RunExtensionTest("tabs/connect")) << message_;
}
