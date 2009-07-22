// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>

#include "base/sys_string_conversions.h"
#include "chrome/browser/cocoa/ui_localizer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

typedef PlatformTest UILocalizerTest;

TEST_F(UILocalizerTest, FixUpWindowsStyleLabel) {
  struct TestData {
    NSString* input;
    NSString* output;
  };

  TestData data[] = {
    { @"", @"" },
    { @"nothing", @"nothing" },
    { @"foo &bar", @"foo bar" },
    { @"foo &&bar", @"foo &bar" },
    { @"foo &&&bar", @"foo &bar" },
    { @"&foo &&bar", @"foo &bar" },
    { @"&foo &bar", @"foo bar" },
    { @"foo bar.", @"foo bar." },
    { @"foo bar..", @"foo bar.." },
    { @"foo bar...", @"foo bar\u2026" },
    { @"foo.bar", @"foo.bar" },
    { @"foo..bar", @"foo..bar" },
    { @"foo...bar", @"foo\u2026bar" },
    { @"foo...bar...", @"foo\u2026bar\u2026" },
  };
  for (size_t idx = 0; idx < ARRAYSIZE_UNSAFE(data); ++idx) {
    string16 input16(base::SysNSStringToUTF16(data[idx].input));

    NSString* result = ui_localizer::FixUpWindowsStyleLabel(input16);
    EXPECT_TRUE(result != nil) << "Fixup Failed, idx = " << idx;

    EXPECT_TRUE([data[idx].output isEqualTo:result])
        << "For idx " << idx << ", expected '" << [data[idx].output UTF8String]
        << "', got '" << [result UTF8String] << "'";
  }
}
