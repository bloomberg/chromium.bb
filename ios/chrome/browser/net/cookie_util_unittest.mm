// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/net/cookie_util.h"

#import <Foundation/Foundation.h>

#include "testing/gtest/include/gtest/gtest.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Date of the last cookie deletion.
NSString* const kLastCookieDeletionDate = @"LastCookieDeletionDate";

}  // namespace

TEST(CookieUtil, ShouldClearSessionCookies) {
  time_t start_test_time;
  time(&start_test_time);
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  // Delete cookies if the key is not present.
  [defaults removeObjectForKey:kLastCookieDeletionDate];
  EXPECT_TRUE(cookie_util::ShouldClearSessionCookies());
  // The deletion time should be created.
  time_t deletion_time = [defaults integerForKey:kLastCookieDeletionDate];
  time_t now;
  time(&now);
  EXPECT_LE(start_test_time, deletion_time);
  EXPECT_LE(deletion_time, now);
  // Cookies are not deleted again.
  EXPECT_FALSE(cookie_util::ShouldClearSessionCookies());

  // Set the deletion time before the machine was started.
  // Sometime in year 1980.
  [defaults setInteger:328697227 forKey:kLastCookieDeletionDate];
  EXPECT_TRUE(cookie_util::ShouldClearSessionCookies());
  EXPECT_LE(now, [defaults integerForKey:kLastCookieDeletionDate]);
}
