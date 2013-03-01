// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/extension_manifest_constants.h"
#include "chrome/common/extensions/manifest_tests/extension_manifest_test.h"
#include "chrome/common/extensions/manifest_url_handler.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace errors = extension_manifest_errors;

class URLOverridesManifestTest : public ExtensionManifestTest {
  virtual void SetUp() OVERRIDE {
    ExtensionManifestTest::SetUp();
    (new extensions::URLOverridesHandler)->Register();
  }
};

TEST_F(URLOverridesManifestTest, Override) {
  Testcase testcases[] = {
    Testcase("override_newtab_and_history.json", errors::kMultipleOverrides),
    Testcase("override_invalid_page.json", errors::kInvalidChromeURLOverrides)
  };
  RunTestcases(testcases, arraysize(testcases),
               EXPECT_TYPE_ERROR);

  scoped_refptr<extensions::Extension> extension;

  extension = LoadAndExpectSuccess("override_new_tab.json");
  EXPECT_EQ(extension->url().spec() + "newtab.html",
            extensions::URLOverrides::GetChromeURLOverrides(extension).
                find("newtab")->second.spec());

  extension = LoadAndExpectSuccess("override_history.json");
  EXPECT_EQ(extension->url().spec() + "history.html",
            extensions::URLOverrides::GetChromeURLOverrides(extension).
                find("history")->second.spec());
}
