// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/activity_services/activity_type_util.h"

#include "ios/chrome/browser/ui/activity_services/appex_constants.h"
#include "ios/chrome/browser/ui/activity_services/print_activity.h"
#include "ios/chrome/grit/ios_strings.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

void StringToTypeTestHelper(NSString* activityString,
                            activity_type_util::ActivityType expectedType) {
  EXPECT_EQ(activity_type_util::TypeFromString(activityString), expectedType);
}

TEST(ActivityTypeUtilTest, StringToTypeTest) {
  StringToTypeTestHelper(@"", activity_type_util::UNKNOWN);
  StringToTypeTestHelper(@"foo", activity_type_util::UNKNOWN);
  StringToTypeTestHelper(@"com.google", activity_type_util::UNKNOWN);
  StringToTypeTestHelper(@"com.google.", activity_type_util::GOOGLE_UNKNOWN);
  StringToTypeTestHelper(@"com.google.Gmail",
                         activity_type_util::GOOGLE_UNKNOWN);
  StringToTypeTestHelper(@"com.google.Gmail.Bar",
                         activity_type_util::GOOGLE_GMAIL);
  StringToTypeTestHelper(@"com.apple.UIKit.activity.Mail",
                         activity_type_util::NATIVE_MAIL);
  StringToTypeTestHelper(@"com.apple.UIKit.activity.Mail.Qux",
                         activity_type_util::UNKNOWN);
  StringToTypeTestHelper([PrintActivity activityIdentifier],
                         activity_type_util::PRINT);
}

void TypeToMessageTestHelper(activity_type_util::ActivityType type,
                             NSString* expectedMessage) {
  EXPECT_NSEQ(activity_type_util::SuccessMessageForActivity(type),
              expectedMessage);
}

TEST(ActivityTypeUtilTest, TypeToMessageTest) {
  TypeToMessageTestHelper(activity_type_util::UNKNOWN, nil);
  TypeToMessageTestHelper(activity_type_util::PRINT, nil);
  TypeToMessageTestHelper(
      activity_type_util::NATIVE_TWITTER,
      l10n_util::GetNSString(IDS_IOS_SHARE_TWITTER_COMPLETE));
  TypeToMessageTestHelper(
      activity_type_util::APPEX_PASSWORD_MANAGEMENT_OTHERS,
      l10n_util::GetNSString(IDS_IOS_APPEX_PASSWORD_FORM_FILLED_SUCCESS));
}

TEST(ActivityTypeUtilTest, IsPasswordAppExtensionTest) {
  // Verifies that known Bundle ID for 1Password requires exact match.
  EXPECT_TRUE(activity_type_util::IsPasswordAppExActivity(
      @"com.agilebits.onepassword-ios.extension"));
  EXPECT_FALSE(activity_type_util::IsPasswordAppExActivity(
      @"com.agilebits.onepassword-ios.extension.otherstuff"));
  // Verifies that known Bundle ID for LastPass requires exact match.
  EXPECT_TRUE(activity_type_util::IsPasswordAppExActivity(
      @"com.lastpass.ilastpass.LastPassExt"));
  EXPECT_FALSE(activity_type_util::IsPasswordAppExActivity(
      @"com.lastpass.ilastpass.LastPassExt.otherstuff"));
  // Verifies that both variants of Dashlane Bundle IDs are recognized.
  EXPECT_TRUE(activity_type_util::IsPasswordAppExActivity(
      @"com.dashlane.dashlanephonefinal.SafariExtension"));
  EXPECT_TRUE(activity_type_util::IsPasswordAppExActivity(
      @"com.dashlane.dashlanephonefinal.appextension"));
  // Verifies that any Bundle ID with @"find-login-action" is recognized.
  EXPECT_TRUE(activity_type_util::IsPasswordAppExActivity(
      @"com.some-company.find-login-action.an-extension"));
  EXPECT_TRUE(activity_type_util::IsPasswordAppExActivity(
      @"com.some-company.compatible-find-login-action-an-extension"));
  EXPECT_TRUE(activity_type_util::IsPasswordAppExActivity(
      @"com.some-company.find-login-action-as-prefix"));
  EXPECT_TRUE(activity_type_util::IsPasswordAppExActivity(
      @"com.some-company.with-suffix-of-find-login-action"));
  EXPECT_TRUE(activity_type_util::IsPasswordAppExActivity(
      @"com.google.find-login-action.extension"));
  // Verifies non-matching Bundle IDs.
  EXPECT_FALSE(
      activity_type_util::IsPasswordAppExActivity(@"com.google.chrome.ios"));
  EXPECT_FALSE(activity_type_util::IsPasswordAppExActivity(
      @"com.apple.UIKit.activity.PostToFacebook"));
}

TEST(ActivityTypeUtilTest, PasswordAppExtensionVersionTest) {
  EXPECT_EQ(activity_services::kPasswordAppExVersionNumber,
            activity_type_util::PasswordAppExActivityVersion(
                @"com.agilebits.onepassword-ios.extension"));
  EXPECT_EQ(activity_services::kPasswordAppExVersionNumber,
            activity_type_util::PasswordAppExActivityVersion(
                @"com.lastpass.ilastpass.LastPassExt"));
  EXPECT_EQ(activity_services::kPasswordAppExVersionNumber,
            activity_type_util::PasswordAppExActivityVersion(
                @"com.dashlane.dashlanephonefinal.SafariExtension"));
  EXPECT_EQ(activity_services::kPasswordAppExVersionNumber,
            activity_type_util::PasswordAppExActivityVersion(
                @"com.some-company.find-login-action.an-extension"));
  EXPECT_NE(activity_services::kPasswordAppExVersionNumber,
            activity_type_util::PasswordAppExActivityVersion(
                @"com.apple.UIKit.activity.PostToFacebook"));
}

}  // namespace
