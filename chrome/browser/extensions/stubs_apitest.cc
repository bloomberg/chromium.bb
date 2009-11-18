// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/ui_test_utils.h"

#if defined(OS_WIN)  // TODO(asargent) get this working on linux
// Tests that we throw errors when you try using extension APIs that aren't
// supported in content scripts.
//
// If you have added a new API to extension_api.json and this test starts
// failing, most likely you need to either mark it as "unprivileged" (if it
// should be available in content scripts) or update the list of privileged APIs
// in renderer_extension_bindings.js.
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, Stubs) {
  HTTPTestServer* server = StartHTTPServer();

  ASSERT_TRUE(RunExtensionTest("stubs")) << message_;

  // Navigate to a simple http:// page, which should get the content script
  // injected and run the rest of the test.
  GURL url = server->TestServerPage("file/extensions/test_file.html");
  ui_test_utils::NavigateToURL(browser(), url);

  ResultCatcher catcher;
  ASSERT_TRUE(catcher.GetNextResult());
}
#endif
