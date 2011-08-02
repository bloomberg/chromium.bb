// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/window_restore_utils.h"

#import <Foundation/Foundation.h>

#include "base/mac/mac_util.h"

namespace restore_utils {

bool IsWindowRestoreEnabled() {
  if (!base::mac::IsOSLionOrLater())
    return false;

  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  // The defaults must be synchronized here otherwise a stale value will be
  // returned for an indeterminate amount of time.
  [defaults synchronize];

  // By default, the preference is not set. When it's not, the intrinsic Lion
  // default (YES) should be returned.
  NSDictionary* prefs = [defaults dictionaryRepresentation];
  NSNumber* value = [prefs objectForKey:@"NSQuitAlwaysKeepsWindows"];
  if (!value)
    return true;

  return !![value boolValue];
}

}  // namespace restore_utils
