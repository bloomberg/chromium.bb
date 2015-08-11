// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/common/app_group/app_group_constants.h"

#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"
#include "components/version_info/version_info.h"

namespace {
NSString* const kChromeAppGroupIdentifier = @"group.com.google.chrome";
}

namespace app_group {

const char kChromeAppGroupXCallbackCommand[] = "app-group-command";

const char kChromeAppGroupCommandPreference[] =
    "GroupApp.ChromeAppGroupCommand";

const char kChromeAppGroupCommandTimePreference[] = "CommandTime";

const char kChromeAppGroupCommandAppPreference[] = "SourceApp";

const char kChromeAppGroupCommandCommandPreference[] = "Command";

const char kChromeAppGroupCommandParameterPreference[] = "Parameter";

const char kChromeAppGroupOpenURLCommand[] = "openurl";
const char kChromeAppGroupVoiceSearchCommand[] = "voicesearch";
const char kChromeAppGroupNewTabCommand[] = "newtab";

const char kChromeAppClientID[] = "ClientID";
const char kUserMetricsEnabledDate[] = "UserMetricsEnabledDate";
const char kInstallDate[] = "InstallDate";
const char kBrandCode[] = "BrandCode";

NSString* ApplicationGroup() {
  NSBundle* bundle = [NSBundle mainBundle];
  NSString* group = [bundle objectForInfoDictionaryKey:@"KSApplicationGroup"];
  if (![group length]) {
    return kChromeAppGroupIdentifier;
  }
  return group;
}

NSString* ApplicationName(AppGroupApplications application) {
  switch (application) {
    case APP_GROUP_CHROME:
      return base::SysUTF8ToNSString(version_info::GetProductName());
    case APP_GROUP_TODAY_EXTENSION:
      return @"TodayExtension";
  }
}

NSUserDefaults* GetGroupUserDefaults() {
  NSUserDefaults* defaults = nil;
  NSString* applicationGroup = ApplicationGroup();
  if (applicationGroup) {
    defaults = [[[NSUserDefaults alloc] initWithSuiteName:applicationGroup]
        autorelease];
    if (defaults)
      return defaults;
  }

  // On a device, the entitlements should always provide an application group to
  // the application. This is not the case on simulator.
  DCHECK(TARGET_IPHONE_SIMULATOR);
  return [NSUserDefaults standardUserDefaults];
}

}  // namespace app_group
