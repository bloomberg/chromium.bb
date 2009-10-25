// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"

// This test is disabled. See bug 25746
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, DISABLED_Storage) {
  ASSERT_TRUE(RunExtensionTest("storage")) << message_;
}
