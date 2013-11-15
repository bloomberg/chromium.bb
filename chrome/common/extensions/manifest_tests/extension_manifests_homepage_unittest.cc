// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/manifest_tests/extension_manifest_test.h"
#include "chrome/common/extensions/manifest_url_handler.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace errors = extensions::manifest_errors;

class HomepageURLManifestTest : public ExtensionManifestTest {
};

TEST_F(HomepageURLManifestTest, ParseHomepageURLs) {
  scoped_refptr<extensions::Extension> extension(
      LoadAndExpectSuccess("homepage_valid.json"));

  Testcase testcases[] = {
    Testcase("homepage_empty.json",
             errors::kInvalidHomepageURL),
    Testcase("homepage_invalid.json",
             errors::kInvalidHomepageURL),
    Testcase("homepage_bad_schema.json",
             errors::kInvalidHomepageURL)
  };
  RunTestcases(testcases, arraysize(testcases),
               EXPECT_TYPE_ERROR);
}

TEST_F(HomepageURLManifestTest, GetHomepageURL) {
  scoped_refptr<extensions::Extension> extension(
      LoadAndExpectSuccess("homepage_valid.json"));
  EXPECT_EQ(GURL("http://foo.com#bar"),
            extensions::ManifestURL::GetHomepageURL(extension.get()));

  // The Google Gallery URL ends with the id, which depends on the path, which
  // can be different in testing, so we just check the part before id.
  extension = LoadAndExpectSuccess("homepage_google_hosted.json");
  EXPECT_TRUE(StartsWithASCII(
      extensions::ManifestURL::GetHomepageURL(extension.get()).spec(),
      "https://chrome.google.com/webstore/detail/",
      false));

  extension = LoadAndExpectSuccess("homepage_externally_hosted.json");
  EXPECT_EQ(GURL(), extensions::ManifestURL::GetHomepageURL(extension.get()));
}
