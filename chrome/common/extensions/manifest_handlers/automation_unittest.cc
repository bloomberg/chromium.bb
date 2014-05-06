// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "chrome/common/extensions/manifest_handlers/automation.h"
#include "chrome/common/extensions/manifest_tests/extension_manifest_test.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/manifest_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

class AutomationManifestTest : public ExtensionManifestTest {
 public:
  AutomationManifestTest() : channel_(chrome::VersionInfo::CHANNEL_UNKNOWN) {}

 protected:
  AutomationInfo* GetAutomationInfo(scoped_refptr<Extension> extension) {
    return static_cast<AutomationInfo*>(
        extension->GetManifestData(manifest_keys::kAutomation));
  }

 private:
  ScopedCurrentChannel channel_;
};

TEST_F(AutomationManifestTest, AsBooleanFalse) {
  scoped_refptr<Extension> extension =
      LoadAndExpectSuccess("automation_boolean_false.json");
  ASSERT_TRUE(extension.get());

  const AutomationInfo* info = AutomationInfo::Get(extension.get());
  ASSERT_FALSE(info);
}

TEST_F(AutomationManifestTest, AsBooleanTrue) {
  scoped_refptr<Extension> extension =
      LoadAndExpectSuccess("automation_boolean_true.json");
  ASSERT_TRUE(extension.get());

  const AutomationInfo* info = AutomationInfo::Get(extension.get());
  ASSERT_TRUE(info);

  EXPECT_FALSE(info->desktop);
  EXPECT_FALSE(info->interact);
  EXPECT_FALSE(info->specified_matches);
  EXPECT_TRUE(info->matches.is_empty());
}

TEST_F(AutomationManifestTest, InteractTrue) {
  scoped_refptr<Extension> extension =
      LoadAndExpectSuccess("automation_interact_true.json");
  ASSERT_TRUE(extension.get());

  const AutomationInfo* info = AutomationInfo::Get(extension.get());
  ASSERT_TRUE(info);

  EXPECT_FALSE(info->desktop);
  EXPECT_TRUE(info->interact);
  EXPECT_FALSE(info->specified_matches);
  EXPECT_TRUE(info->matches.is_empty());
}

TEST_F(AutomationManifestTest, Matches) {
  scoped_refptr<Extension> extension = LoadAndExpectWarning(
      "automation_matches.json",
      ErrorUtils::FormatErrorMessage(
          automation_errors::kErrorInvalidMatch,
          "www.badpattern.com",
          URLPattern::GetParseResultString(
              URLPattern::PARSE_ERROR_MISSING_SCHEME_SEPARATOR)));
  ASSERT_TRUE(extension.get());

  const AutomationInfo* info = AutomationInfo::Get(extension.get());
  ASSERT_TRUE(info);

  EXPECT_FALSE(info->desktop);
  EXPECT_FALSE(info->interact);
  EXPECT_TRUE(info->specified_matches);
  EXPECT_FALSE(info->matches.is_empty());

  EXPECT_TRUE(info->matches.MatchesURL(GURL("http://www.google.com/")));
  EXPECT_TRUE(info->matches.MatchesURL(GURL("http://www.google.com")));
  EXPECT_TRUE(info->matches.MatchesURL(GURL("http://www.twitter.com/")));
  EXPECT_TRUE(info->matches.MatchesURL(GURL("http://www.twitter.com")));

  EXPECT_FALSE(info->matches.MatchesURL(GURL("http://www.bing.com/")));
  EXPECT_FALSE(info->matches.MatchesURL(GURL("http://www.bing.com")));
}

TEST_F(AutomationManifestTest, EmptyMatches) {
  scoped_refptr<Extension> extension =
      LoadAndExpectWarning("automation_empty_matches.json",
                           automation_errors::kErrorNoMatchesProvided);
  ASSERT_TRUE(extension.get());

  const AutomationInfo* info = AutomationInfo::Get(extension.get());
  ASSERT_TRUE(info);

  EXPECT_FALSE(info->desktop);
  EXPECT_FALSE(info->interact);
  EXPECT_TRUE(info->specified_matches);
  EXPECT_TRUE(info->matches.is_empty());
}

TEST_F(AutomationManifestTest, NoValidMatches) {
  std::string error;
  scoped_refptr<Extension> extension =
      LoadExtension(Manifest("automation_no_valid_matches.json"), &error);
  ASSERT_TRUE(extension.get());
  EXPECT_EQ("", error);
  EXPECT_EQ(2u, extension->install_warnings().size());
  EXPECT_EQ(ErrorUtils::FormatErrorMessage(
                automation_errors::kErrorInvalidMatch,
                "www.badpattern.com",
                URLPattern::GetParseResultString(
                    URLPattern::PARSE_ERROR_MISSING_SCHEME_SEPARATOR)),
            extension->install_warnings()[0].message);
  EXPECT_EQ(automation_errors::kErrorNoMatchesProvided,
            extension->install_warnings()[1].message);

  const AutomationInfo* info = AutomationInfo::Get(extension.get());
  ASSERT_TRUE(info);

  EXPECT_FALSE(info->desktop);
  EXPECT_FALSE(info->interact);
  EXPECT_TRUE(info->specified_matches);
  EXPECT_TRUE(info->matches.is_empty());
}

TEST_F(AutomationManifestTest, DesktopFalse) {
  scoped_refptr<Extension> extension =
      LoadAndExpectSuccess("automation_desktop_false.json");
  ASSERT_TRUE(extension.get());

  const AutomationInfo* info = AutomationInfo::Get(extension.get());
  ASSERT_TRUE(info);

  EXPECT_FALSE(info->desktop);
  EXPECT_FALSE(info->interact);
  EXPECT_FALSE(info->specified_matches);
  EXPECT_TRUE(info->matches.is_empty());
}

TEST_F(AutomationManifestTest, DesktopTrue) {
  scoped_refptr<Extension> extension =
      LoadAndExpectSuccess("automation_desktop_true.json");
  ASSERT_TRUE(extension.get());

  const AutomationInfo* info = AutomationInfo::Get(extension.get());
  ASSERT_TRUE(info);

  EXPECT_TRUE(info->desktop);
  EXPECT_TRUE(info->interact);
  EXPECT_FALSE(info->specified_matches);
  EXPECT_TRUE(info->matches.is_empty());
}

TEST_F(AutomationManifestTest, Desktop_InteractTrue) {
  scoped_refptr<Extension> extension =
      LoadAndExpectSuccess("automation_desktop_interact_true.json");
  ASSERT_TRUE(extension.get());

  const AutomationInfo* info = AutomationInfo::Get(extension.get());
  ASSERT_TRUE(info);

  EXPECT_TRUE(info->desktop);
  EXPECT_TRUE(info->interact);
  EXPECT_FALSE(info->specified_matches);
  EXPECT_TRUE(info->matches.is_empty());
}

TEST_F(AutomationManifestTest, Desktop_InteractFalse) {
  scoped_refptr<Extension> extension =
      LoadAndExpectWarning("automation_desktop_interact_false.json",
                           automation_errors::kErrorDesktopTrueInteractFalse);
  ASSERT_TRUE(extension.get());

  const AutomationInfo* info = AutomationInfo::Get(extension.get());
  ASSERT_TRUE(info);

  EXPECT_TRUE(info->desktop);
  EXPECT_TRUE(info->interact);
  EXPECT_FALSE(info->specified_matches);
  EXPECT_TRUE(info->matches.is_empty());
}

TEST_F(AutomationManifestTest, Desktop_MatchesSpecified) {
  scoped_refptr<Extension> extension = LoadAndExpectWarning(
      "automation_desktop_matches_specified.json",
      automation_errors::kErrorDesktopTrueMatchesSpecified);
  ASSERT_TRUE(extension.get());

  const AutomationInfo* info = AutomationInfo::Get(extension.get());
  ASSERT_TRUE(info);

  EXPECT_TRUE(info->desktop);
  EXPECT_TRUE(info->interact);
  EXPECT_FALSE(info->specified_matches);
  EXPECT_TRUE(info->matches.is_empty());
}

}  // namespace extensions
