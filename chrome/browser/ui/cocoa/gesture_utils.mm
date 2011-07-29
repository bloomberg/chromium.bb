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
  return [defaults boolForKey:@"AppleEnableSwipeNavigateWithScrolls"];
}

// On Lion, returns YES if the scroll direction is "natural"/inverted. All other
// OSes will return NO.
BOOL IsScrollDirectionInverted() {
  if (!base::mac::IsOSLionOrLater())
    return NO;

  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  // The defaults must be synchronized here otherwise a stale value will be
  // returned for an indeterminate amount of time. For some reason, this is
  // not necessary in |-recognizeTwoFingerGestures|.
  [defaults synchronize];
  return [defaults boolForKey:@"com.apple.swipescrolldirection"];
}

}  // namespace gesture_utils
