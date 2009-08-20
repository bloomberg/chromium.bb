// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"

// TODO(brettw) bug 19866: this test is disabled because it is flaky.
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, DISABLED_Bookmarks) {
  // TODO(erikkay) no initial state for this test.
  ASSERT_TRUE(RunExtensionTest("bookmarks")) << message_;
}
