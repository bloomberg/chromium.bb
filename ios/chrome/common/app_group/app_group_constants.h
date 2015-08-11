// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_COMMON_APP_GROUP_APP_GROUP_CONSTANTS_H_
#define IOS_CHROME_COMMON_APP_GROUP_APP_GROUP_CONSTANTS_H_

#import <Foundation/Foundation.h>

// Constants that are shared between apps belonging to the chrome iOS app group.
// They are mainly used for communication between applications in the group.
namespace app_group {

// An enum of the different application member of the Chrome app group.
// To ensure continuity in metrics log, applications can only be added at the
// end.
enum AppGroupApplications {
  APP_GROUP_CHROME,
  APP_GROUP_TODAY_EXTENSION,
};

// The x-callback-url indicating that an application in the group requires a
// command.
extern const char kChromeAppGroupXCallbackCommand[];

// The key of a preference containing a dictionary containing app group command
// parameters.
extern const char kChromeAppGroupCommandPreference[];

// The key in kChromeAppGroupCommandPreference containing the ID of the
// application requesting a x-callback-url command.
extern const char kChromeAppGroupCommandAppPreference[];

// The key in kChromeAppGroupCommandPreference containing the command requested
// by |kChromeAppGroupCommandAppPreference|.
extern const char kChromeAppGroupCommandCommandPreference[];

// The command to open a URL. Parameter must contain the URL.
extern const char kChromeAppGroupOpenURLCommand[];

// The command to trigger a voice search.
extern const char kChromeAppGroupVoiceSearchCommand[];

// The command to open a new tab.
extern const char kChromeAppGroupNewTabCommand[];

// The key in kChromeAppGroupCommandPreference containing a NSDate at which
// |kChromeAppGroupCommandAppPreference| issued the command.
extern const char kChromeAppGroupCommandTimePreference[];

// The key in kChromeAppGroupCommandPreference containing a parameter for the
// command. The nature of the parameter depends on the command.
extern const char kChromeAppGroupCommandParameterPreference[];

// The key of a preference containing Chrome client ID reported in the metrics
// client ID. If the user does not opt in, this value must be cleared from the
// shared user defaults.
extern const char kChromeAppClientID[];

// The key of a preference containing the timestamp when the user enabled
// metrics reporting.
extern const char kUserMetricsEnabledDate[];

// The current epoch time, on the first run of chrome on this machine. It is set
// once and must be attached to metrics reports forever thereafter.
extern const char kInstallDate[];

// The brand code string associated with the install. This brand code will be
// added to metrics logs.
extern const char kBrandCode[];

// Gets the application group
NSString* ApplicationGroup();

// Returns an autoreleased pointer to the shared user defaults if an
// application group is defined. If not (i.e. on simulator, or if entitlements
// do not allow it) returns [NSUserDefaults standardUserDefaults].
NSUserDefaults* GetGroupUserDefaults();

// The application name of |application|.
NSString* ApplicationName(AppGroupApplications application);

}  // namespace app_group

#endif  // IOS_CHROME_COMMON_APP_GROUP_APP_GROUP_CONSTANTS_H_
