// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_nsobject.h"
#import "chrome/browser/ui/cocoa/bookmarks/bookmark_menu.h"
#import "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace {

class BookmarkMenuTest : public CocoaTest {
};

TEST_F(BookmarkMenuTest, Basics) {
  scoped_nsobject<BookmarkMenu> menu([[BookmarkMenu alloc]
                                       initWithTitle:@"title"]);
  scoped_nsobject<NSMenuItem> item([[NSMenuItem alloc] initWithTitle:@"item"
                                                              action:NULL
                                                       keyEquivalent:@""]);
  [menu addItem:item];
  long long l = 103849459459598948LL;  // arbitrary
  NSNumber* number = [NSNumber numberWithLongLong:l];
  [menu setRepresentedObject:number];
  EXPECT_EQ(l, [menu id]);
}

}  // namespace
