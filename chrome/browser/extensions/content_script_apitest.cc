// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/test/ui_test_utils.h"
#include "net/base/mock_host_resolver.h"

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, ContentScriptAllFrames) {
  ASSERT_TRUE(StartTestServer());
  ASSERT_TRUE(RunExtensionTest("content_scripts/all_frames")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, ContentScriptExtensionIframe) {
  ASSERT_TRUE(StartTestServer());
  ASSERT_TRUE(RunExtensionTest("content_scripts/extension_iframe")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, ContentScriptExtensionProcess) {
  ASSERT_TRUE(StartTestServer());
  ASSERT_TRUE(
      RunExtensionTest("content_scripts/extension_process")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, ContentScriptFragmentNavigation) {
  ASSERT_TRUE(StartTestServer());
  const char* extension_name = "content_scripts/fragment";
  ASSERT_TRUE(RunExtensionTest(extension_name)) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, ContentScriptIsolatedWorlds) {
  // This extension runs various bits of script and tests that they all run in
  // the same isolated world.
  ASSERT_TRUE(StartTestServer());
  ASSERT_TRUE(RunExtensionTest("content_scripts/isolated_world1")) << message_;

  // Now load a different extension, inject into same page, verify worlds aren't
  // shared.
  ASSERT_TRUE(RunExtensionTest("content_scripts/isolated_world2")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, ContentScriptIgnoreHostPermissions) {
  ASSERT_TRUE(StartTestServer());
  host_resolver()->AddRule("a.com", "127.0.0.1");
  host_resolver()->AddRule("b.com", "127.0.0.1");
  ASSERT_TRUE(RunExtensionTest(
      "content_scripts/dont_match_host_permissions")) << message_;
}
