// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/common/chrome_switches.h"


IN_PROC_BROWSER_TEST_F(ExtensionApiTest, WebNavigation) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalExtensionApis);

  ASSERT_TRUE(RunExtensionTest("webnavigation/api")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, WebNavigationClientRedirect) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalExtensionApis);

  ASSERT_TRUE(RunExtensionTest("webnavigation/clientRedirect")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, WebNavigationForwardBack) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalExtensionApis);

  ASSERT_TRUE(RunExtensionTest("webnavigation/forwardBack")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, WebNavigationIFrame) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalExtensionApis);

  ASSERT_TRUE(RunExtensionTest("webnavigation/iframe")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, WebNavigationOpenTab) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalExtensionApis);

  ASSERT_TRUE(RunExtensionTest("webnavigation/openTab")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, WebNavigationReferenceFragment) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalExtensionApis);

  ASSERT_TRUE(RunExtensionTest("webnavigation/referenceFragment")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, WebNavigationSimpleLoad) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalExtensionApis);

  ASSERT_TRUE(RunExtensionTest("webnavigation/simpleLoad")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, WebNavigationFailures) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalExtensionApis);

  ASSERT_TRUE(RunExtensionTest("webnavigation/failures")) << message_;
}
