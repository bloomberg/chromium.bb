// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/stl_util.h"
#include "chrome/common/extensions/manifest_tests/chrome_manifest_test.h"
#include "extensions/common/extension.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

class ExcludeMatchesManifestTest : public ChromeManifestTest {
};

TEST_F(ExcludeMatchesManifestTest, ExcludeMatchPatterns) {
  Testcase testcases[] = {
    Testcase("exclude_matches.json"),
    Testcase("exclude_matches_empty.json")
  };
  RunTestcases(testcases, base::size(testcases), EXPECT_TYPE_SUCCESS);

  Testcase testcases2[] = {
    Testcase("exclude_matches_not_list.json",
             "Invalid value for 'content_scripts[0].exclude_matches'."),
    Testcase("exclude_matches_invalid_host.json",
             "Invalid value for 'content_scripts[0].exclude_matches[0]': "
                 "Invalid host wildcard.")
  };
  RunTestcases(testcases2, base::size(testcases2), EXPECT_TYPE_ERROR);
}

}  // namespace extensions
