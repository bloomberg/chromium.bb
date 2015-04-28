// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/geolocation/CLLocation+OmniboxGeolocation.h"

#import <objc/runtime.h>

#include "base/mac/scoped_nsobject.h"

namespace {

// The key needed for objc_setAssociatedObject. Any value will do, because the
// address is the key.
static char g_acquisitionIntervalKey = 'k';

// Number of seconds before a location is no longer considered to be fresh
// enough to use for an Omnibox query.
const NSTimeInterval kLocationIsFreshAge = 24.0 * 60.0 * 60.0;  // 24 hours

// Number of seconds before we will try to refresh the device location.
const NSTimeInterval kLocationShouldRefreshAge = 5.0 * 60.0;  // 5 minutes

}  // namespace

@implementation CLLocation (OmniboxGeolocation)

- (NSTimeInterval)cr_acquisitionInterval {
  NSNumber* interval =
      objc_getAssociatedObject(self, &g_acquisitionIntervalKey);
  return [interval doubleValue];
}

- (void)cr_setAcquisitionInterval:(NSTimeInterval)interval {
  base::scoped_nsobject<NSNumber> boxedInterval(
      [[NSNumber alloc] initWithDouble:interval]);
  objc_setAssociatedObject(self, &g_acquisitionIntervalKey, boxedInterval.get(),
                           OBJC_ASSOCIATION_RETAIN);
}

- (BOOL)cr_isFreshEnough {
  NSTimeInterval age = -[self.timestamp timeIntervalSinceNow];
  return (age >= 0) && (age <= kLocationIsFreshAge);
}

- (BOOL)cr_shouldRefresh {
  NSTimeInterval age = -[self.timestamp timeIntervalSinceNow];
  // Note: if age < 0 (the location is from the future), don't believe it.
  return (age < 0) || (age > kLocationShouldRefreshAge);
}

@end
