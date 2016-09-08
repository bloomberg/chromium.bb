// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/browsertest_util.h"

#include "base/strings/string_number_conversions.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/test_extension_dir.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "testing/gtest/include/gtest/gtest-spi.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {
namespace {

// TODO(jyasskin): It should be possible to test extensions without
// inheriting from the monolithic ExtensionBrowserTest.
class ExtensionBrowsertestUtilTest : public ExtensionBrowserTest {
};

IN_PROC_BROWSER_TEST_F(ExtensionBrowsertestUtilTest,
                       ExecuteScriptInBackground) {
  TestExtensionDir ext_dir;
  ext_dir.WriteManifest(
      "{\n"
      "  \"name\": \"ExecuteScript extension\",\n"
      "  \"version\": \"0.1\",\n"
      "  \"manifest_version\": 2,\n"
      "  \"background\": {\"scripts\": [\"background.js\"]}\n"
      "}\n");
  ext_dir.WriteFile(FILE_PATH_LITERAL("background.js"), "");
  const Extension* extension = LoadExtension(ext_dir.UnpackedPath());
  ASSERT_TRUE(extension);
  EXPECT_EQ(extension->id(),
            browsertest_util::ExecuteScriptInBackgroundPage(
                profile(),
                extension->id(),
                "window.domAutomationController.send(chrome.runtime.id);"));

  // This test checks that executing a script doesn't block the browser process.
  EXPECT_EQ(base::IntToString(browser()->tab_strip_model()->count()),
            browsertest_util::ExecuteScriptInBackgroundPage(
                profile(),
                extension->id(),
                "chrome.tabs.query({}, function(result) {\n"
                "  window.domAutomationController.send('' + result.length);\n"
                "});"));

  // An argument that isn't a string should cause a failure, not a hang.
  EXPECT_NONFATAL_FAILURE(browsertest_util::ExecuteScriptInBackgroundPage(
                              profile(),
                              extension->id(),
                              "window.domAutomationController.send(3);"),
                          "send(3)");
}

}  // namespace
}  // namespace extensions
