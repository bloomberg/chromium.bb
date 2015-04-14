// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/common/string_util.h"

#include "base/mac/scoped_nsobject.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"

namespace {

TEST(StringUtilTest, ParseStringWithLink) {
  NSArray* const all_test_data = @[
    @{
      @"input" : @"Text without link.",
      @"expected" : @"Text without link.",
      @"link range" : [NSValue valueWithRange:NSMakeRange(NSNotFound, 0)]
    },
    @{
      @"input" : @"Text with empty link BEGIN_LINK END_LINK.",
      @"expected" : @"Text with empty link .",
      @"link range" : [NSValue valueWithRange:NSMakeRange(21, 0)]
    },
    @{
      @"input" : @"Text with BEGIN_LINK and no end link.",
      @"expected" : @"Text with BEGIN_LINK and no end link.",
      @"link range" : [NSValue valueWithRange:NSMakeRange(NSNotFound, 0)]
    },
    @{
      @"input" : @"Text with valid BEGIN_LINK link END_LINK and spaces.",
      @"expected" : @"Text with valid link and spaces.",
      @"link range" : [NSValue valueWithRange:NSMakeRange(16, 4)]
    },
    @{
      @"input" : @"Text with valid BEGIN_LINKlinkEND_LINK and no spaces.",
      @"expected" : @"Text with valid link and no spaces.",
      @"link range" : [NSValue valueWithRange:NSMakeRange(16, 4)]
    }
  ];
  for (NSDictionary* test_data : all_test_data) {
    NSString* input_text = test_data[@"input"];
    NSString* expected_text = test_data[@"expected"];
    NSRange expected_range = [test_data[@"link range"] rangeValue];

    EXPECT_NSEQ(expected_text, ParseStringWithLink(input_text, nullptr));

    // Initialize |range| with some values that are not equal to the expected
    // ones.
    NSRange range = NSMakeRange(1000, 2000);
    EXPECT_NSEQ(expected_text, ParseStringWithLink(input_text, &range));
    EXPECT_EQ(expected_range.location, range.location);
    EXPECT_EQ(expected_range.length, range.length);
  }
}

}  // namespace
