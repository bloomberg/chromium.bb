// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/prefs/pref_value_map.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/sessions/restore_on_startup_policy_handler.h"
#include "chrome/common/pref_names.h"
#include "components/policy/core/browser/configuration_policy_handler.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "grit/components_strings.h"
#include "policy/policy_constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace policy {

class RestoreOnStartupPolicyHandlerTest : public testing::Test {
 protected:
  void SetPolicyValue(const std::string& policy, base::Value* value) {
    policies_.Set(
        policy, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER, value, NULL);
  }
  bool CheckPolicySettings() {
    return handler_.CheckPolicySettings(policies_, &errors_);
  }
  void ApplyPolicySettings() {
    handler_.ApplyPolicySettings(policies_, &prefs_);
  }
  PolicyErrorMap& errors() { return errors_; }
  PrefValueMap& prefs() { return prefs_; }

 private:
  PolicyMap policies_;
  PolicyErrorMap errors_;
  PrefValueMap prefs_;
  RestoreOnStartupPolicyHandler handler_;
};

TEST_F(RestoreOnStartupPolicyHandlerTest, CheckPolicySettings_FailsTypeCheck) {
  // Handler expects an int; pass it a bool.
  SetPolicyValue(key::kRestoreOnStartup, new base::FundamentalValue(false));
  // Checking should fail and add an error to the error map.
  EXPECT_FALSE(CheckPolicySettings());
  EXPECT_EQ(1U, errors().size());
  EXPECT_EQ(l10n_util::GetStringFUTF16(
                IDS_POLICY_TYPE_ERROR,
                base::ASCIIToUTF16(
                    ConfigurationPolicyHandler::ValueTypeToString(
                        base::Value::TYPE_INTEGER))),
            errors().begin()->second);
}

TEST_F(RestoreOnStartupPolicyHandlerTest, CheckPolicySettings_Unspecified) {
  // Don't specify a value for the policy.
  // Checking should succeed with no errors.
  EXPECT_TRUE(CheckPolicySettings());
  EXPECT_EQ(0U, errors().size());
}

TEST_F(RestoreOnStartupPolicyHandlerTest, CheckPolicySettings_UnknownValue) {
  // Specify an unknown value for the policy.
  int impossible_value = SessionStartupPref::kPrefValueHomePage +
                         SessionStartupPref::kPrefValueLast +
                         SessionStartupPref::kPrefValueURLs +
                         SessionStartupPref::kPrefValueNewTab;
  SetPolicyValue(key::kRestoreOnStartup,
                 new base::FundamentalValue(impossible_value));
  // Checking should succeed but add an error to the error map.
  EXPECT_TRUE(CheckPolicySettings());
  EXPECT_EQ(1U, errors().size());
  EXPECT_EQ(l10n_util::GetStringFUTF16(
                IDS_POLICY_OUT_OF_RANGE_ERROR,
                base::ASCIIToUTF16(base::IntToString(impossible_value))),
            errors().begin()->second);
}

TEST_F(RestoreOnStartupPolicyHandlerTest, CheckPolicySettings_HomePage) {
  // Specify the HomePage value.
  SetPolicyValue(
      key::kRestoreOnStartup,
      new base::FundamentalValue(SessionStartupPref::kPrefValueHomePage));
  // Checking should succeed but add an error to the error map.
  EXPECT_TRUE(CheckPolicySettings());
  EXPECT_EQ(1U, errors().size());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_POLICY_VALUE_DEPRECATED),
            errors().begin()->second);
}

TEST_F(RestoreOnStartupPolicyHandlerTest,
       CheckPolicySettings_RestoreLastSession_SessionCookies) {
  // Specify the Last value and the Session-Only Cookies value.
  SetPolicyValue(
      key::kRestoreOnStartup,
      new base::FundamentalValue(SessionStartupPref::kPrefValueLast));
  scoped_ptr<base::ListValue> urls(new base::ListValue);
  urls->AppendString("http://foo.com");
  SetPolicyValue(key::kCookiesSessionOnlyForUrls, urls.release());
  // Checking should succeed but add an error to the error map.
  EXPECT_TRUE(CheckPolicySettings());
  EXPECT_EQ(1U, errors().size());
  EXPECT_TRUE(key::kCookiesSessionOnlyForUrls == errors().begin()->first);
  EXPECT_EQ(l10n_util::GetStringFUTF16(
                IDS_POLICY_OVERRIDDEN,
                base::ASCIIToUTF16(key::kRestoreOnStartup)),
            errors().begin()->second);
}

TEST_F(RestoreOnStartupPolicyHandlerTest,
       CheckPolicySettings_RestoreLastSession_ClearDataOnExit) {
  // Specify the Last value and the Clear-Data-On-Exit value.
  SetPolicyValue(
      key::kRestoreOnStartup,
      new base::FundamentalValue(SessionStartupPref::kPrefValueLast));
  SetPolicyValue(key::kClearSiteDataOnExit, new base::FundamentalValue(true));
  // Checking should succeed but add an error to the error map.
  EXPECT_TRUE(CheckPolicySettings());
  EXPECT_EQ(1U, errors().size());
  EXPECT_TRUE(key::kClearSiteDataOnExit == errors().begin()->first);
  EXPECT_EQ(l10n_util::GetStringFUTF16(
                IDS_POLICY_OVERRIDDEN,
                base::ASCIIToUTF16(key::kRestoreOnStartup)),
            errors().begin()->second);
}

TEST_F(RestoreOnStartupPolicyHandlerTest,
       CheckPolicySettings_RestoreLastSession) {
  // Specify the Last value without the conflicts.
  SetPolicyValue(
      key::kRestoreOnStartup,
      new base::FundamentalValue(SessionStartupPref::kPrefValueLast));
  // Checking should succeed with no errors.
  EXPECT_TRUE(CheckPolicySettings());
  EXPECT_EQ(0U, errors().size());
}

TEST_F(RestoreOnStartupPolicyHandlerTest, CheckPolicySettings_URLs) {
  // Specify the URLs value.
  SetPolicyValue(
      key::kRestoreOnStartup,
      new base::FundamentalValue(SessionStartupPref::kPrefValueURLs));
  // Checking should succeed with no errors.
  EXPECT_TRUE(CheckPolicySettings());
  EXPECT_EQ(0U, errors().size());
}

TEST_F(RestoreOnStartupPolicyHandlerTest, CheckPolicySettings_NewTab) {
  // Specify the NewTab value.
  SetPolicyValue(
      key::kRestoreOnStartup,
      new base::FundamentalValue(SessionStartupPref::kPrefValueNewTab));
  // Checking should succeed with no errors.
  EXPECT_TRUE(CheckPolicySettings());
  EXPECT_EQ(0U, errors().size());
}

TEST_F(RestoreOnStartupPolicyHandlerTest, ApplyPolicySettings_NoValue) {
  // Don't specify a value for the policy.
  ApplyPolicySettings();
  // The resulting prefs should be empty.
  EXPECT_TRUE(prefs().begin() == prefs().end());
}

TEST_F(RestoreOnStartupPolicyHandlerTest, ApplyPolicySettings_WrongType) {
  // Handler expects an int; pass it a bool.
  SetPolicyValue(key::kRestoreOnStartup, new base::FundamentalValue(false));
  // The resulting prefs should be empty.
  EXPECT_TRUE(prefs().begin() == prefs().end());
}

TEST_F(RestoreOnStartupPolicyHandlerTest, ApplyPolicySettings_NotHomePage) {
  // Specify anything except the HomePage value.
  int not_home_page = SessionStartupPref::kPrefValueHomePage + 1;
  SetPolicyValue(key::kRestoreOnStartup,
                 new base::FundamentalValue(not_home_page));
  ApplyPolicySettings();
  // The resulting prefs should have the value we specified.
  int result;
  EXPECT_TRUE(prefs().GetInteger(prefs::kRestoreOnStartup, &result));
  EXPECT_EQ(not_home_page, result);
}

TEST_F(RestoreOnStartupPolicyHandlerTest,
       ApplyPolicySettings_HomePage_NoHomePageValue) {
  // Specify the HomePage value but no HomePageIsNewTabPage value.
  SetPolicyValue(
      key::kRestoreOnStartup,
      new base::FundamentalValue(SessionStartupPref::kPrefValueHomePage));
  ApplyPolicySettings();
  // The resulting prefs should be empty.
  EXPECT_TRUE(prefs().begin() == prefs().end());
}

TEST_F(RestoreOnStartupPolicyHandlerTest,
       ApplyPolicySettings_HomePage_HomePageValueIsWrongType) {
  // Specify the HomePage value and an integer for the home page value.
  SetPolicyValue(
      key::kRestoreOnStartup,
      new base::FundamentalValue(SessionStartupPref::kPrefValueHomePage));
  SetPolicyValue(
      key::kHomepageIsNewTabPage,
      new base::FundamentalValue(314159));
  ApplyPolicySettings();
  // The resulting prefs should be empty.
  EXPECT_TRUE(prefs().begin() == prefs().end());
}

TEST_F(RestoreOnStartupPolicyHandlerTest,
       ApplyPolicySettings_HomePage_HomePageIsNewTabPage) {
  // Specify the HomePage value and the home page as the new tab page.
  SetPolicyValue(
      key::kRestoreOnStartup,
      new base::FundamentalValue(SessionStartupPref::kPrefValueHomePage));
  SetPolicyValue(
      key::kHomepageIsNewTabPage,
      new base::FundamentalValue(true));
  ApplyPolicySettings();
  // The resulting prefs should have the restore value as NTP.
  int result;
  EXPECT_TRUE(prefs().GetInteger(prefs::kRestoreOnStartup, &result));
  int expected = SessionStartupPref::kPrefValueNewTab;
  EXPECT_EQ(expected, result);
}

TEST_F(RestoreOnStartupPolicyHandlerTest,
       ApplyPolicySettings_HomePage_HomePageIsNotNewTabPage_NotDefined) {
  // Specify the HomePage value but don't specify the home page to use.
  SetPolicyValue(
      key::kRestoreOnStartup,
      new base::FundamentalValue(SessionStartupPref::kPrefValueHomePage));
  SetPolicyValue(
      key::kHomepageIsNewTabPage,
      new base::FundamentalValue(false));
  ApplyPolicySettings();
  // The resulting prefs should be empty.
  EXPECT_TRUE(prefs().begin() == prefs().end());
}

TEST_F(RestoreOnStartupPolicyHandlerTest,
       ApplyPolicySettings_HomePage_HomePageIsNotNewTabPage_WrongType) {
  // Specify the HomePage value but specify a boolean as the home page.
  SetPolicyValue(
      key::kRestoreOnStartup,
      new base::FundamentalValue(SessionStartupPref::kPrefValueHomePage));
  SetPolicyValue(
      key::kHomepageIsNewTabPage,
      new base::FundamentalValue(false));
  SetPolicyValue(
      key::kHomepageLocation,
      new base::FundamentalValue(false));
  ApplyPolicySettings();
  // The resulting prefs should be empty.
  EXPECT_TRUE(prefs().begin() == prefs().end());
}

TEST_F(RestoreOnStartupPolicyHandlerTest,
       ApplyPolicySettings_HomePage_HomePageIsNotNewTabPage) {
  SetPolicyValue(
      key::kRestoreOnStartup,
      new base::FundamentalValue(SessionStartupPref::kPrefValueHomePage));
  SetPolicyValue(key::kHomepageIsNewTabPage, new base::FundamentalValue(false));
  SetPolicyValue(key::kHomepageLocation,
                 new base::StringValue("http://foo.com"));
  ApplyPolicySettings();

  // The resulting prefs should have have URLs specified for startup.
  int result;
  EXPECT_TRUE(prefs().GetInteger(prefs::kRestoreOnStartup, &result));
  int expected = SessionStartupPref::kPrefValueURLs;
  EXPECT_EQ(expected, result);

  // The resulting prefs should have the URL we specified as the home page.
  base::Value* url_result;
  EXPECT_TRUE(prefs().GetValue(prefs::kURLsToRestoreOnStartup, &url_result));
  base::ListValue* url_list_result;
  EXPECT_TRUE(url_result->GetAsList(&url_list_result));
  EXPECT_EQ(1U, url_list_result->GetSize());
  std::string expected_url;
  EXPECT_TRUE(url_list_result->GetString(0, &expected_url));
  EXPECT_EQ(std::string("http://foo.com"), expected_url);
}

}  // namespace policy
