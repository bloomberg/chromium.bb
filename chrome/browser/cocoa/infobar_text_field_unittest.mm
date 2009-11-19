// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "chrome/browser/cocoa/cocoa_test_helper.h"
#import "chrome/browser/cocoa/infobar_text_field.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

@interface TextFieldDelegatePong: NSObject {
 @private
  BOOL pong_;
}
@property(readonly) BOOL pong;
@end

@implementation TextFieldDelegatePong
@synthesize pong = pong_;

- (id)init {
  if ((self == [super init])) {
    pong_ = NO;
  }
  return self;
}

- (void)linkClicked {
  pong_ = YES;
}
@end

namespace {

///////////////////////////////////////////////////////////////////////////
// Test fixtures

class InfoBarTextFieldTest : public CocoaTest {
 public:
  InfoBarTextFieldTest() {
    NSRect frame = NSMakeRect(0, 0, 200, 200);
    scoped_nsobject<InfoBarTextField> field(
        [[InfoBarTextField alloc] initWithFrame:frame]);
    field_ = field.get();
    [[test_window() contentView] addSubview:field_];
  }

  InfoBarTextField* field_;
};

////////////////////////////////////////////////////////////////////////////
// Tests

TEST_VIEW(InfoBarTextFieldTest, field_)

TEST_F(InfoBarTextFieldTest, LinkClicked) {
  scoped_nsobject<InfoBarTextField> field(
      [[InfoBarTextField alloc] initWithFrame:NSMakeRect(0, 0, 200, 200)]);
  scoped_nsobject<TextFieldDelegatePong> delegate(
      [[TextFieldDelegatePong alloc] init]);
  [field setDelegate:delegate];

  // Our implementation doesn't look at any of these fields, so they
  // can all be nil.
  [field textView:nil clickedOnLink:nil atIndex:0];
  EXPECT_TRUE([delegate pong]);
}

}  // namespace
