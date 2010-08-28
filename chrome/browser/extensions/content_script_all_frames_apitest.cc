// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, ContentScriptAllFrames) {
  ASSERT_TRUE(test_server()->Start());
  ASSERT_TRUE(RunExtensionTest("content_scripts/all_frames")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, ContentScriptExtensionIframe) {
  ASSERT_TRUE(test_server()->Start());
  ASSERT_TRUE(RunExtensionTest("content_scripts/extension_iframe")) << message_;
}
