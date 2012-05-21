// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/manifest_tests/extension_manifest_test.h"

#include "chrome/common/extensions/extension_manifest_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace errors = extension_manifest_errors;

TEST_F(ExtensionManifestTest, OptionsPageInApps) {
  scoped_refptr<extensions::Extension> extension;

  // Allow options page with absolute URL in hosted apps.
  extension = LoadAndExpectSuccess("hosted_app_absolute_options.json");
  EXPECT_STREQ("http",
               extension->options_url().scheme().c_str());
  EXPECT_STREQ("example.com",
               extension->options_url().host().c_str());
  EXPECT_STREQ("options.html",
               extension->options_url().ExtractFileName().c_str());

  Testcase testcases[] = {
    // Forbid options page with relative URL in hosted apps.
    Testcase("hosted_app_relative_options.json",
             errors::kInvalidOptionsPageInHostedApp),

    // Forbid options page with non-(http|https) scheme in hosted app.
    Testcase("hosted_app_file_options.json",
             errors::kInvalidOptionsPageInHostedApp),

    // Forbid absolute URL for options page in packaged apps.
    Testcase("packaged_app_absolute_options.json",
             errors::kInvalidOptionsPageExpectUrlInPackage)
  };
  RunTestcases(testcases, arraysize(testcases),
               EXPECT_TYPE_ERROR);
}
