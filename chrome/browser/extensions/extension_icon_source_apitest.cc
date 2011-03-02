// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/ui_test_utils.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "googleurl/src/gurl.h"
#include "net/base/mock_host_resolver.h"

class ExtensionIconSourceTest : public ExtensionApiTest {
};

IN_PROC_BROWSER_TEST_F(ExtensionIconSourceTest, IconsLoaded) {
  FilePath basedir = test_data_dir_.AppendASCII("icons");
  ASSERT_TRUE(LoadExtension(basedir.AppendASCII("extension_with_permission")));
  ASSERT_TRUE(LoadExtension(basedir.AppendASCII("extension_no_permission")));
  std::string result;

  // Test that the icons are loaded and that the chrome://extension-icon
  // parameters work correctly.
  ui_test_utils::NavigateToURL(
      browser(),
      GURL("chrome-extension://gbmgkahjioeacddebbnengilkgbkhodg/index.html"));
  ASSERT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractString(
      browser()->GetSelectedTabContents()->render_view_host(), L"",
      L"window.domAutomationController.send(document.title)",
      &result));
  EXPECT_EQ(result, "Loaded");

  // Verify that the an extension can't load chrome://extension-icon icons
  // without the management permission.
  ui_test_utils::NavigateToURL(
      browser(),
      GURL("chrome-extension://apocjbpjpkghdepdngjlknfpmabcmlao/index.html"));
  ASSERT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractString(
      browser()->GetSelectedTabContents()->render_view_host(), L"",
      L"window.domAutomationController.send(document.title)",
      &result));
  EXPECT_EQ(result, "Not Loaded");
}
