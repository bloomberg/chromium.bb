// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"

// Flaky, http://crbug.com/19866. Please consult phajdan.jr before re-enabling.
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, DISABLED_Bookmarks) {
  // TODO(erikkay) no initial state for this test.
  ASSERT_TRUE(RunExtensionTest("bookmarks")) << message_;
}
