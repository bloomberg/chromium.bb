// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/activity_services/activity_type_util.h"

#include "base/logging.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#import "ios/chrome/browser/ui/activity_services/appex_constants.h"
#import "ios/chrome/browser/ui/activity_services/print_activity.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// A substring to identify activity strings that are from Password Management
// App Extensions. This string is intentionally without the leading and
// trailing "." so it can be used as a prefix, suffix, or substring of the
// App Extension's bundle ID.
NSString* const kFindLoginActionBundleSubstring = @"find-login-action";
}

namespace activity_type_util {

struct PrefixTypeAssociation {
  activity_type_util::ActivityType type_;
  NSString* const prefix_;
  bool requiresExactMatch_;
};

const PrefixTypeAssociation prefixTypeAssociations[] = {
    {NATIVE_FACEBOOK, @"com.apple.UIKit.activity.PostToFacebook", true},
    {NATIVE_MAIL, @"com.apple.UIKit.activity.Mail", true},
    {NATIVE_MESSAGE, @"com.apple.UIKit.activity.Message", true},
    {NATIVE_TWITTER, @"com.apple.UIKit.activity.PostToTwitter", true},
    {NATIVE_WEIBO, @"com.apple.UIKit.activity.PostToWeibo", true},
    {NATIVE_CLIPBOARD, @"com.apple.UIKit.activity.CopyToPasteboard", true},
    {PRINT, @"com.google.chrome.printActivity", true},
    // The trailing '.' prevents false positives.
    // For instance, "com.viberific" won't be matched by the "com.viber.".
    {GOOGLE_DRIVE, @"com.google.Drive.", false},
    {GOOGLE_GMAIL, @"com.google.Gmail.", false},
    {GOOGLE_GOOGLEPLUS, @"com.google.GooglePlus.", false},
    {GOOGLE_HANGOUTS, @"com.google.hangouts.", false},
    {GOOGLE_INBOX, @"com.google.inbox.", false},
    {GOOGLE_UNKNOWN, @"com.google.", false},
    {THIRD_PARTY_MAILBOX, @"com.orchestra.v2.", false},
    {THIRD_PARTY_FACEBOOK_MESSENGER, @"com.facebook.Messenger.", false},
    {THIRD_PARTY_WHATS_APP, @"net.whatsapp.WhatsApp.", false},
    {THIRD_PARTY_LINE, @"jp.naver.line.", false},
    {THIRD_PARTY_VIBER, @"com.viber.", false},
    {THIRD_PARTY_SKYPE, @"com.skype.", false},
    {THIRD_PARTY_TANGO, @"com.sgiggle.Tango.", false},
    {THIRD_PARTY_WECHAT, @"com.tencent.xin.", false},
    {THIRD_PARTY_EVERNOTE, @"com.evernote.", false},
    {THIRD_PARTY_PINTEREST, @"pinterest.", false},
    {THIRD_PARTY_POCKET, @"com.ideashower.ReadItLaterPro.", false},
    {THIRD_PARTY_READABILITY, @"com.readability.ReadabilityMobile.", false},
    {THIRD_PARTY_INSTAPAPER, @"com.marcoarment.instapaperpro.", false},
    {APPEX_PASSWORD_MANAGEMENT_1PASSWORD,
     activity_services::kAppExtensionOnePassword, true},
    {APPEX_PASSWORD_MANAGEMENT_LASTPASS,
     activity_services::kAppExtensionLastPass, true},
    {APPEX_PASSWORD_MANAGEMENT_DASHLANE,
     activity_services::kAppExtensionDashlanePrefix, false}};

ActivityType TypeFromString(NSString* activityString) {
  DCHECK(activityString);
  // Checks for the special case first so the more general patterns in
  // prefixTypeAssociations would not prematurely trapped them.
  NSRange found =
      [activityString rangeOfString:kFindLoginActionBundleSubstring];
  if (found.length)
    return APPEX_PASSWORD_MANAGEMENT_OTHERS;
  for (auto const& assocation : prefixTypeAssociations) {
    if (assocation.requiresExactMatch_) {
      if ([activityString isEqualToString:assocation.prefix_])
        return assocation.type_;
    } else {
      if ([activityString hasPrefix:assocation.prefix_])
        return assocation.type_;
    }
  }
  return UNKNOWN;
}

NSNumber* PasswordAppExActivityVersion(NSString* activityString) {
  switch (TypeFromString(activityString)) {
    case APPEX_PASSWORD_MANAGEMENT_1PASSWORD:
    case APPEX_PASSWORD_MANAGEMENT_LASTPASS:
    case APPEX_PASSWORD_MANAGEMENT_DASHLANE:
    case APPEX_PASSWORD_MANAGEMENT_OTHERS:
      return activity_services::kPasswordAppExVersionNumber;
    default:
      return nil;
  }
}

bool IsPasswordAppExActivity(NSString* activityString) {
  return PasswordAppExActivityVersion(activityString) != nil;
}

NSString* SuccessMessageForActivity(ActivityType type) {
  switch (type) {
    case NATIVE_CLIPBOARD:
      return l10n_util::GetNSString(IDS_IOS_SHARE_TO_CLIPBOARD_SUCCESS);
    case NATIVE_FACEBOOK:
      return l10n_util::GetNSString(IDS_IOS_SHARE_FACEBOOK_COMPLETE);
    case NATIVE_MAIL:
      return l10n_util::GetNSString(IDS_IOS_SHARE_EMAIL_COMPLETE);
    case NATIVE_MESSAGE:
      return l10n_util::GetNSString(IDS_IOS_SHARE_MESSAGES_COMPLETE);
    case NATIVE_TWITTER:
      return l10n_util::GetNSString(IDS_IOS_SHARE_TWITTER_COMPLETE);
    case GOOGLE_GMAIL:
      return l10n_util::GetNSString(IDS_IOS_SHARE_EMAIL_COMPLETE);
    case GOOGLE_GOOGLEPLUS:
      return l10n_util::GetNSString(IDS_IOS_SHARE_GPLUS_COMPLETE);
    case APPEX_PASSWORD_MANAGEMENT_1PASSWORD:
    case APPEX_PASSWORD_MANAGEMENT_LASTPASS:
    case APPEX_PASSWORD_MANAGEMENT_DASHLANE:
    case APPEX_PASSWORD_MANAGEMENT_OTHERS:
      return l10n_util::GetNSString(IDS_IOS_APPEX_PASSWORD_FORM_FILLED_SUCCESS);
    default:
      return nil;
  }
}

void RecordMetricForActivity(ActivityType type) {
  switch (type) {
    case UNKNOWN:
      base::RecordAction(base::UserMetricsAction("MobileShareMenuUnknown"));
      break;
    case NATIVE_CLIPBOARD:
      base::RecordAction(base::UserMetricsAction("MobileShareMenuClipboard"));
      break;
    case PRINT:
      base::RecordAction(base::UserMetricsAction("MobileShareMenuPrint"));
      break;
    case GOOGLE_GMAIL:
      base::RecordAction(base::UserMetricsAction("MobileShareMenuGmail"));
      break;
    case GOOGLE_GOOGLEPLUS:
      base::RecordAction(base::UserMetricsAction("MobileShareMenuGooglePlus"));
      break;
    case GOOGLE_DRIVE:
      base::RecordAction(base::UserMetricsAction("MobileShareMenuGoogleDrive"));
      break;
    case GOOGLE_HANGOUTS:
      base::RecordAction(base::UserMetricsAction("MobileShareMenuHangouts"));
      break;
    case GOOGLE_INBOX:
      base::RecordAction(base::UserMetricsAction("MobileShareMenuInbox"));
      break;
    case GOOGLE_UNKNOWN:
      base::RecordAction(
          base::UserMetricsAction("MobileShareMenuGoogleUnknown"));
      break;
    case NATIVE_MAIL:
    case THIRD_PARTY_MAILBOX:
      base::RecordAction(base::UserMetricsAction("MobileShareMenuToMail"));
      break;
    case NATIVE_FACEBOOK:
    case NATIVE_TWITTER:
      base::RecordAction(
          base::UserMetricsAction("MobileShareMenuToSocialNetwork"));
      break;
    case NATIVE_MESSAGE:
      base::RecordAction(base::UserMetricsAction("MobileShareMenuToSMSApp"));
      break;
    case NATIVE_WEIBO:
    case THIRD_PARTY_FACEBOOK_MESSENGER:
    case THIRD_PARTY_WHATS_APP:
    case THIRD_PARTY_LINE:
    case THIRD_PARTY_VIBER:
    case THIRD_PARTY_SKYPE:
    case THIRD_PARTY_TANGO:
    case THIRD_PARTY_WECHAT:
      base::RecordAction(
          base::UserMetricsAction("MobileShareMenuToInstantMessagingApp"));
      break;
    case THIRD_PARTY_EVERNOTE:
    case THIRD_PARTY_PINTEREST:
    case THIRD_PARTY_POCKET:
    case THIRD_PARTY_READABILITY:
    case THIRD_PARTY_INSTAPAPER:
      base::RecordAction(
          base::UserMetricsAction("MobileShareMenuToContentApp"));
      break;
    case APPEX_PASSWORD_MANAGEMENT_1PASSWORD:
    case APPEX_PASSWORD_MANAGEMENT_LASTPASS:
    case APPEX_PASSWORD_MANAGEMENT_DASHLANE:
    case APPEX_PASSWORD_MANAGEMENT_OTHERS:
      base::RecordAction(
          base::UserMetricsAction("MobileAppExFormFilledByPasswordManager"));
      break;
  }
}

}  // activity_type_util
