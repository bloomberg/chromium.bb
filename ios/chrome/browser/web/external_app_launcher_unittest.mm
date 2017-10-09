// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/web/external_app_launcher.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ExternalAppLauncher (Private)
// Returns the Phone/FaceTime call argument from |url|.
// Made available for unit testing.
+ (NSString*)formatCallArgument:(NSURL*)url;
@end

namespace {

using ExternalAppLauncherTest = PlatformTest;

TEST_F(ExternalAppLauncherTest, TestBadFormatCallArgument) {
  EXPECT_NSEQ(@"garbage:",
              [ExternalAppLauncher
                  formatCallArgument:[NSURL URLWithString:@"garbage:"]]);
  EXPECT_NSEQ(@"malformed:////",
              [ExternalAppLauncher
                  formatCallArgument:[NSURL URLWithString:@"malformed:////"]]);
}

TEST_F(ExternalAppLauncherTest, TestFormatCallArgument) {
  EXPECT_NSEQ(
      @"+1234",
      [ExternalAppLauncher
          formatCallArgument:[NSURL URLWithString:@"facetime://+1234"]]);
  EXPECT_NSEQ(
      @"+abcd",
      [ExternalAppLauncher
          formatCallArgument:[NSURL URLWithString:@"facetime-audio://+abcd"]]);
  EXPECT_NSEQ(
      @"+1-650-555-1212",
      [ExternalAppLauncher
          formatCallArgument:[NSURL URLWithString:@"tel://+1-650-555-1212"]]);
  EXPECT_NSEQ(@"75009",
              [ExternalAppLauncher
                  formatCallArgument:[NSURL URLWithString:@"garbage:75009"]]);
}

TEST_F(ExternalAppLauncherTest, TestURLEscapedArgument) {
  EXPECT_NSEQ(@"+1 650 555 1212",
              [ExternalAppLauncher
                  formatCallArgument:
                      [NSURL URLWithString:@"tel://+1%20650%20555%201212"]]);
}

}  // namespace
