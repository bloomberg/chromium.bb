// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/scoped_nsobject.h"
#import "chrome/browser/cocoa/bookmark_menu.h"
#import "chrome/browser/cocoa/cocoa_test_helper.h"
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
  NSValue* value = [NSValue valueWithPointer:menu.get()];
  [menu setRepresentedObject:value];
  EXPECT_EQ((void*)menu.get(), (void*)[menu node]);
}

}  // namespace
