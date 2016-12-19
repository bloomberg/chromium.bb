// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_ACTIVITY_SERVICES_ACTIVITY_TYPE_UTIL_H_
#define IOS_CHROME_BROWSER_UI_ACTIVITY_SERVICES_ACTIVITY_TYPE_UTIL_H_

#import <UIKit/UIKit.h>

namespace activity_type_util {

enum ActivityType {
  NATIVE_FACEBOOK,
  NATIVE_MAIL,
  NATIVE_MESSAGE,
  NATIVE_TWITTER,
  NATIVE_WEIBO,
  NATIVE_CLIPBOARD,
  PRINT,
  GOOGLE_DRIVE,
  GOOGLE_GMAIL,
  GOOGLE_GOOGLEPLUS,
  GOOGLE_HANGOUTS,
  GOOGLE_INBOX,
  GOOGLE_UNKNOWN,
  THIRD_PARTY_MAILBOX,
  THIRD_PARTY_FACEBOOK_MESSENGER,
  THIRD_PARTY_WHATS_APP,
  THIRD_PARTY_LINE,
  THIRD_PARTY_VIBER,
  THIRD_PARTY_SKYPE,
  THIRD_PARTY_TANGO,
  THIRD_PARTY_WECHAT,
  THIRD_PARTY_EVERNOTE,
  THIRD_PARTY_PINTEREST,
  THIRD_PARTY_POCKET,
  THIRD_PARTY_READABILITY,
  THIRD_PARTY_INSTAPAPER,
  APPEX_PASSWORD_MANAGEMENT_1PASSWORD,
  APPEX_PASSWORD_MANAGEMENT_LASTPASS,
  APPEX_PASSWORD_MANAGEMENT_DASHLANE,
  APPEX_PASSWORD_MANAGEMENT_OTHERS,
  // UNKNOWN must be the last type.
  UNKNOWN,
};

// Returns the ActivityType associated with |activityString|.
ActivityType TypeFromString(NSString* activityString);

// Returns the version number to use for Password Management App Extensions
// for the activity indicated by |activityString|. This string is the
// identification for the App Extension. Returned value is an autoreleased
// object or nil if |activityString| does not belong to a Password
// Management App Extension.
NSNumber* PasswordAppExActivityVersion(NSString* activityString);

// Whether activity indicated in |activityString| is an iOS Password Management
// App Extension.
bool IsPasswordAppExActivity(NSString* activityString);

// Returns the message to present when the activity |type| successfully
// occurred. Returns nil if no message should be presented.
NSString* SuccessMessageForActivity(ActivityType type);

// Records the UMA for activity |type|.
void RecordMetricForActivity(ActivityType type);

}  // namespace activity_type_util

#endif  // IOS_CHROME_BROWSER_UI_ACTIVITY_SERVICES_ACTIVITY_TYPE_UTIL_H_
