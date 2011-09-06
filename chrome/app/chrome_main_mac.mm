// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app/chrome_main_mac.h"

#import <Cocoa/Cocoa.h>

#include <string>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/logging.h"
#import "base/mac/foundation_util.h"
#import "base/mac/scoped_nsautorelease_pool.h"
#include "base/sys_string_conversions.h"
#include "chrome/browser/policy/policy_path_parser.h"
#include "chrome/common/chrome_paths_internal.h"
#include "policy/policy_constants.h"

void CheckUserDataDirPolicy(FilePath* user_data_dir) {
  base::mac::ScopedNSAutoreleasePool pool;

  // Since the configuration management infrastructure is not initialized when
  // this code runs, read the policy preference directly.
  NSString* key = base::SysUTF8ToNSString(policy::key::kUserDataDir);
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  NSString* value = [defaults stringForKey:key];
  if (value && [defaults objectIsForcedForKey:key]) {
    std::string string_value = base::SysNSStringToUTF8(value);
    // Now replace any vars the user might have used.
    string_value =
        policy::path_parser::ExpandPathVariables(string_value);
    *user_data_dir = FilePath(string_value);
  }
}

void SetUpBundleOverrides() {
  base::mac::ScopedNSAutoreleasePool pool;

  base::mac::SetOverrideAppBundlePath(chrome::GetFrameworkBundlePath());

  NSBundle* base_bundle = chrome::OuterAppBundle();
  base::mac::SetBaseBundleID([[base_bundle bundleIdentifier] UTF8String]);
}
