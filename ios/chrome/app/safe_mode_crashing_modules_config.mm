// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/safe_mode_crashing_modules_config.h"

#include "base/logging.h"

namespace {

NSString* const kStartupCrashModulesKey = @"StartupCrashModules";
NSString* const kModuleFriendlyNameKey = @"ModuleFriendlyName";

}  // namespace

@implementation SafeModeCrashingModulesConfig

+ (SafeModeCrashingModulesConfig*)sharedInstance {
  static SafeModeCrashingModulesConfig* instance =
      [[SafeModeCrashingModulesConfig alloc] init];
  return instance;
}

- (instancetype)init {
  return [super initWithAppId:nil version:nil plist:@"CrashingModules.plist"];
}

- (NSString*)startupCrashModuleFriendlyName:(NSString*)modulePath {
  NSDictionary* configData = [self dictionaryFromConfig];
  NSDictionary* modules = [configData objectForKey:kStartupCrashModulesKey];
  if (modules) {
    DCHECK([modules isKindOfClass:[NSDictionary class]]);
    NSDictionary* module = modules[modulePath];
    if (module) {
      DCHECK([module isKindOfClass:[NSDictionary class]]);
      return module[kModuleFriendlyNameKey];
    }
  }
  return nil;
}

@end
