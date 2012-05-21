// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/manifest_tests/extension_manifest_test.h"

#include "chrome/common/extensions/extension_manifest_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace errors = extension_manifest_errors;

TEST_F(ExtensionManifestTest, Override) {
  Testcase testcases[] = {
    Testcase("override_newtab_and_history.json", errors::kMultipleOverrides),
    Testcase("override_invalid_page.json", errors::kInvalidChromeURLOverrides)
  };
  RunTestcases(testcases, arraysize(testcases),
               EXPECT_TYPE_ERROR);

  scoped_refptr<extensions::Extension> extension;

  extension = LoadAndExpectSuccess("override_new_tab.json");
  EXPECT_EQ(extension->url().spec() + "newtab.html",
            extension->GetChromeURLOverrides().find("newtab")->second.spec());

  extension = LoadAndExpectSuccess("override_history.json");
  EXPECT_EQ(extension->url().spec() + "history.html",
            extension->GetChromeURLOverrides().find("history")->second.spec());
}
