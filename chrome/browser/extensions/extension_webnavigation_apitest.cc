// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_webnavigation_api.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/ui_test_utils.h"
#include "content/browser/tab_contents/tab_contents.h"

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, WebNavigation) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalExtensionApis);

  FrameNavigationState::set_allow_extension_scheme(true);

  ASSERT_TRUE(RunExtensionTest("webnavigation/api")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, WebNavigationClientRedirect) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalExtensionApis);

  FrameNavigationState::set_allow_extension_scheme(true);

  ASSERT_TRUE(RunExtensionTest("webnavigation/clientRedirect")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, WebNavigationForwardBack) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalExtensionApis);

  FrameNavigationState::set_allow_extension_scheme(true);

  ASSERT_TRUE(RunExtensionTest("webnavigation/forwardBack")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, WebNavigationIFrame) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalExtensionApis);

  FrameNavigationState::set_allow_extension_scheme(true);

  ASSERT_TRUE(RunExtensionTest("webnavigation/iframe")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, WebNavigationOpenTab) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalExtensionApis);

  FrameNavigationState::set_allow_extension_scheme(true);

  ASSERT_TRUE(RunExtensionTest("webnavigation/openTab")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, WebNavigationReferenceFragment) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalExtensionApis);

  FrameNavigationState::set_allow_extension_scheme(true);

  ASSERT_TRUE(RunExtensionTest("webnavigation/referenceFragment")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, WebNavigationSimpleLoad) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalExtensionApis);

  FrameNavigationState::set_allow_extension_scheme(true);

  ASSERT_TRUE(RunExtensionTest("webnavigation/simpleLoad")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, WebNavigationFailures) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalExtensionApis);

  FrameNavigationState::set_allow_extension_scheme(true);

  ASSERT_TRUE(RunExtensionTest("webnavigation/failures")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, WebNavigationUserAction) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalExtensionApis);

  FrameNavigationState::set_allow_extension_scheme(true);

  // Wait for the extension to set itself up and return control to us.
  ASSERT_TRUE(RunExtensionTest("webnavigation/userAction")) << message_;

  ResultCatcher catcher;

  ExtensionService* service = browser()->profile()->GetExtensionService();
  const Extension* extension =
      service->GetExtensionById(last_loaded_extension_id_, false);
  GURL url = extension->GetResourceURL("a.html");

  ui_test_utils::NavigateToURL(browser(), url);

  url = extension->GetResourceURL("b.html");
  // This corresponds to "Open link in new tab".
  browser()->GetSelectedTabContents()->OpenURL(
      url, GURL(), NEW_BACKGROUND_TAB, PageTransition::LINK);
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
}
