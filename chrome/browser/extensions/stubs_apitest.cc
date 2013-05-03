// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/ui_test_utils.h"
#include "googleurl/src/gurl.h"
#include "net/test/spawned_test_server.h"

// Tests that we throw errors when you try using extension APIs that aren't
// supported in content scripts.
// Timey-outy on mac. http://crbug.com/89116
#if defined(OS_MACOSX)
#define MAYBE_Stubs DISABLED_Stubs
#else
#define MAYBE_Stubs Stubs
#endif
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, MAYBE_Stubs) {
  ASSERT_TRUE(test_server()->Start());

  ASSERT_TRUE(RunExtensionTest("stubs")) << message_;

  // Navigate to a simple http:// page, which should get the content script
  // injected and run the rest of the test.
  GURL url(test_server()->GetURL("file/extensions/test_file.html"));
  ui_test_utils::NavigateToURL(browser(), url);

  ResultCatcher catcher;
  ASSERT_TRUE(catcher.GetNextResult());
}
