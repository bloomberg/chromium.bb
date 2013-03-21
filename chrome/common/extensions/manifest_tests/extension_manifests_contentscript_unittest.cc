// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_manifest_constants.h"
#include "chrome/common/extensions/manifest_tests/extension_manifest_test.h"
#include "extensions/common/error_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace errors = extension_manifest_errors;

TEST_F(ExtensionManifestTest, ContentScriptMatchPattern) {
  Testcase testcases[] = {
    // chrome:// urls are not allowed.
    Testcase("content_script_chrome_url_invalid.json",
             extensions::ErrorUtils::FormatErrorMessage(
                 errors::kInvalidMatch,
                 base::IntToString(0),
                 base::IntToString(0),
                 URLPattern::GetParseResultString(
                     URLPattern::PARSE_ERROR_INVALID_SCHEME))),

    // Match paterns must be strings.
    Testcase("content_script_match_pattern_not_string.json",
             extensions::ErrorUtils::FormatErrorMessage(errors::kInvalidMatch,
                                                        base::IntToString(0),
                                                        base::IntToString(0),
                                                        errors::kExpectString))
  };
  RunTestcases(testcases, arraysize(testcases),
               EXPECT_TYPE_ERROR);

  LoadAndExpectSuccess("ports_in_content_scripts.json");
}

TEST_F(ExtensionManifestTest, ContentScriptsOnChromeUrlsWithFlag) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kExtensionsOnChromeURLs);
  std::string error;
  scoped_refptr<extensions::Extension> extension =
    LoadAndExpectSuccess("content_script_chrome_url_invalid.json");
  EXPECT_EQ("", error);
  const GURL newtab_url("chrome://newtab/");
  EXPECT_TRUE(extension->HasContentScriptAtURL(newtab_url));
}
