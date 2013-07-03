// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#import "chrome/browser/ui/cocoa/applescript/bookmark_applescript_utils_unittest.h"
#import "chrome/browser/ui/cocoa/applescript/bookmark_item_applescript.h"
#import "chrome/browser/ui/cocoa/applescript/error_applescript.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"
#include "url/gurl.h"

typedef BookmarkAppleScriptTest BookmarkItemAppleScriptTest;

namespace {

// Set and get title.
TEST_F(BookmarkItemAppleScriptTest, GetAndSetTitle) {
  NSArray* bookmarkItems = [bookmarkBar_.get() bookmarkItems];
  BookmarkItemAppleScript* item1 = [bookmarkItems objectAtIndex:0];
  [item1 setTitle:@"Foo"];
  EXPECT_NSEQ(@"Foo", [item1 title]);
}

// Set and get URL.
TEST_F(BookmarkItemAppleScriptTest, GetAndSetURL) {
  NSArray* bookmarkItems = [bookmarkBar_.get() bookmarkItems];
  BookmarkItemAppleScript* item1 = [bookmarkItems objectAtIndex:0];
  [item1 setURL:@"http://foo-bar.org"];
  EXPECT_EQ(GURL("http://foo-bar.org"),
            GURL(base::SysNSStringToUTF8([item1 URL])));

  // If scripter enters invalid URL.
  base::scoped_nsobject<FakeScriptCommand> fakeScriptCommand(
      [[FakeScriptCommand alloc] init]);
  [item1 setURL:@"invalid-url.org"];
  EXPECT_EQ((int)AppleScript::errInvalidURL,
            [fakeScriptCommand.get() scriptErrorNumber]);
}

}  // namespace
