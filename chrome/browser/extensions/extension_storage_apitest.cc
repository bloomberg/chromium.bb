// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"

// Started failing with r29947. WebKit merge 49961:49992.
IN_PROC_BROWSER_TEST_F(DISABLED_ExtensionApiTest, Storage) {
  ASSERT_TRUE(RunExtensionTest("storage")) << message_;
}
