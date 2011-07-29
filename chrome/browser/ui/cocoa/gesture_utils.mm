// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/gesture_utils.h"

#include "base/mac/mac_util.h"

namespace gesture_utils {

BOOL RecognizeTwoFingerGestures() {
  // Lion or higher will have support for two-finger swipe gestures.
  if (!base::mac::IsOSLionOrLater())
    return NO;

  // A note about preferences:
  // On Lion System Preferences, the swipe gesture behavior is controlled by the
  // setting in Trackpad --> More Gestures --> Swipe between pages. This setting
  // has three values:
  //  A) Scroll left or right with two fingers. This should perform a cool
  //     scrolling animation, but doesn't yet <http://crbug.com/90228>.
  //  B) Swipe left or right with three fingers.
  //  C) Swipe with two or three fingers.
  //
  // The three-finger gesture is controlled by the integer preference
  // |com.apple.trackpad.threeFingerHorizSwipeGesture|. Its value is 0 for (A)
  // and 1 for (B, C).
  //
  // The two-finger gesture is controlled by the preference below and is boolean
  // YES for (A, C) and NO for (B).
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  [defaults synchronize];

  // By default, the preference is not set. When it's not, the intrinsic Lion
  // default (YES) should be returned.
  NSDictionary* prefs = [defaults dictionaryRepresentation];
  NSNumber* value = [prefs objectForKey:@"AppleEnableSwipeNavigateWithScrolls"];
  if (!value)
    return YES;

  // If the preference is set, return the value.
  return [value boolValue];
}

// On Lion, returns YES if the scroll direction is "natural"/inverted. All other
// OSes will return NO.
BOOL IsScrollDirectionInverted() {
  if (!base::mac::IsOSLionOrLater())
    return NO;

  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  [defaults synchronize];

  // By default, the preference is not set. When it's not, the intrinsic Lion
  // default (YES) should be returned.
  NSDictionary* prefs = [defaults dictionaryRepresentation];
  NSNumber* value = [prefs objectForKey:@"com.apple.swipescrolldirection"];
  if (!value)
    return YES;

  return [value boolValue];
}

}  // namespace gesture_utils
