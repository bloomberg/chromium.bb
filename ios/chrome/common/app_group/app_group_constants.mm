// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/common/app_group/app_group_constants.h"

#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"
#include "components/version_info/version_info.h"
#include "ios/chrome/common/ios_app_bundle_id_prefix.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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
const char kChromeAppGroupFocusOmniboxCommand[] = "focusomnibox";

const char kChromeAppClientID[] = "ClientID";
const char kUserMetricsEnabledDate[] = "UserMetricsEnabledDate";
const char kInstallDate[] = "InstallDate";
const char kBrandCode[] = "BrandCode";

NSString* const kShareItemURL = @"URL";
NSString* const kShareItemTitle = @"Title";
NSString* const kShareItemDate = @"Date";
NSString* const kShareItemCancel = @"Cancel";
NSString* const kShareItemType = @"Type";

NSString* ApplicationGroup() {
  NSBundle* bundle = [NSBundle mainBundle];
  NSString* group = [bundle objectForInfoDictionaryKey:@"KSApplicationGroup"];
  if (![group length]) {
    return [NSString stringWithFormat:@"group.%s.chrome",
                                      BUILDFLAG(IOS_APP_BUNDLE_ID_PREFIX), nil];
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
    defaults = [[NSUserDefaults alloc] initWithSuiteName:applicationGroup];
    if (defaults)
      return defaults;
  }

  // On a device, the entitlements should always provide an application group to
  // the application. This is not the case on simulator.
  DCHECK(TARGET_IPHONE_SIMULATOR);
  return [NSUserDefaults standardUserDefaults];
}

NSURL* ShareExtensionItemsFolder() {
  NSURL* groupURL = [[NSFileManager defaultManager]
      containerURLForSecurityApplicationGroupIdentifier:ApplicationGroup()];
  NSURL* readingListURL =
      [groupURL URLByAppendingPathComponent:@"ShareExtensionItems"
                                isDirectory:YES];
  return readingListURL;
}

}  // namespace app_group
