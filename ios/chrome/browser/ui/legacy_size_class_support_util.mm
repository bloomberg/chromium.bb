// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/legacy_size_class_support_util.h"

#include "base/logging.h"
#import "base/ios/ios_util.h"
#import "ios/chrome/browser/ui/ui_util.h"

namespace {
// The height of an iPhone 6 in portrait.  An iPhone whose side length is
// greater than this value is considered to be of a REGULAR size class in that
// dimension.
const CGFloat kIPhone6PortraitHeight = 667.0f;

// Returns the SizeClassIdiom for a screen with a side of |side_length| points
// in a given dimension.
SizeClassIdiom SizeClassForSideLength(CGFloat side_length) {
  // These functions are only for use in pre-iOS8.  This simplifies the logic
  // for iPad size classes, as it eliminates the need to consider multitasking.
  DCHECK(!base::ios::IsRunningOnIOS8OrLater());
  // iPads always have a REGULAR width size class.
  if (IsIPadIdiom())
    return REGULAR;
  // A side whose length is greater than |kIPhone6PortraitHeight| is REGULAR.
  if (side_length > kIPhone6PortraitHeight)
    return REGULAR;
  // All other iPhone widths are COMPACT.
  return COMPACT;
}
}

SizeClassIdiom CurrentWidthSizeClass() {
  return SizeClassForSideLength(CurrentScreenWidth());
}

SizeClassIdiom CurrentHeightSizeClass() {
  return SizeClassForSideLength(CurrentScreenHeight());
}
