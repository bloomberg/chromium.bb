// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/common/switches.h"
#include "net/dns/mock_host_resolver.h"
#include "url/gurl.h"

using ExtensionIconSourceTest = extensions::ExtensionApiTest;

// Times out on Mac and Win. http://crbug.com/238705
#if defined(OS_WIN) || defined(OS_MACOSX)
#define MAYBE_IconsLoaded DISABLED_IconsLoaded
#else
#define MAYBE_IconsLoaded IconsLoaded
#endif

IN_PROC_BROWSER_TEST_F(ExtensionIconSourceTest, MAYBE_IconsLoaded) {
  base::FilePath basedir = test_data_dir_.AppendASCII("icons");
  ASSERT_TRUE(LoadExtension(basedir.AppendASCII("extension_with_permission")));
  ASSERT_TRUE(LoadExtension(basedir.AppendASCII("extension_no_permission")));
  std::string result;

  // Test that the icons are loaded and that the chrome://extension-icon
  // parameters work correctly.
  ui_test_utils::NavigateToURL(
      browser(),
      GURL("chrome-extension://gbmgkahjioeacddebbnengilkgbkhodg/index.html"));
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      browser()->tab_strip_model()->GetActiveWebContents(),
      "window.domAutomationController.send(document.title)",
      &result));
  EXPECT_EQ(result, "Loaded");

  // Verify that the an extension can't load chrome://extension-icon icons
  // without the management permission.
  ui_test_utils::NavigateToURL(
      browser(),
      GURL("chrome-extension://apocjbpjpkghdepdngjlknfpmabcmlao/index.html"));
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      browser()->tab_strip_model()->GetActiveWebContents(),
      "window.domAutomationController.send(document.title)",
      &result));
  EXPECT_EQ(result, "Not Loaded");
}

IN_PROC_BROWSER_TEST_F(ExtensionIconSourceTest, InvalidURL) {
  std::string result;

  // Test that navigation to an invalid url works.
  ui_test_utils::NavigateToURL(
      browser(),
      GURL("chrome://extension-icon/invalid"));

  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      browser()->tab_strip_model()->GetActiveWebContents(),
      "window.domAutomationController.send(document.title)",
      &result));
  EXPECT_EQ(result, "invalid (96\xC3\x97""96)");
}

// Times out on Mac and Win. http://crbug.com/238705
#if defined(OS_WIN) || defined(OS_MACOSX)
#define MAYBE_IconsLoadedIncognito DISABLED_IconsLoadedIncognito
#else
#define MAYBE_IconsLoadedIncognito IconsLoadedIncognito
#endif

IN_PROC_BROWSER_TEST_F(ExtensionIconSourceTest, MAYBE_IconsLoadedIncognito) {
  base::FilePath basedir = test_data_dir_.AppendASCII("icons");
  ASSERT_TRUE(LoadExtensionIncognito(
      basedir.AppendASCII("extension_with_permission")));
  ASSERT_TRUE(LoadExtensionIncognito(
      basedir.AppendASCII("extension_no_permission")));
  std::string result;

  // Test that the icons are loaded and that the chrome://extension-icon
  // parameters work correctly.
  Browser* otr_browser = OpenURLOffTheRecord(
      browser()->profile(),
      GURL("chrome-extension://gbmgkahjioeacddebbnengilkgbkhodg/index.html"));
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      otr_browser->tab_strip_model()->GetActiveWebContents(),
      "window.domAutomationController.send(document.title)",
      &result));
  EXPECT_EQ(result, "Loaded");

  // Verify that the an extension can't load chrome://extension-icon icons
  // without the management permission.
  OpenURLOffTheRecord(
      browser()->profile(),
      GURL("chrome-extension://apocjbpjpkghdepdngjlknfpmabcmlao/index.html"));
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      otr_browser->tab_strip_model()->GetActiveWebContents(),
      "window.domAutomationController.send(document.title)",
      &result));
  EXPECT_EQ(result, "Not Loaded");
}
