// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/scoped_ptr.h"
#include "base/scoped_nsobject.h"
#include "base/string_util.h"
#include "base/sys_string_conversions.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/find_notification_details.h"
#import "chrome/browser/cocoa/cocoa_test_helper.h"
#import "chrome/browser/cocoa/find_bar_cocoa_controller.h"
#import "chrome/browser/cocoa/find_pasteboard.h"
#import "chrome/browser/cocoa/find_bar_text_field.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

// Expose private variables to make testing easier.
@interface FindBarCocoaController(Testing)
- (NSView*)findBarView;
- (NSString*)findText;
- (FindBarTextField*)findTextField;
@end

@implementation FindBarCocoaController(Testing)
- (NSView*)findBarView {
  return findBarView_;
}

- (NSString*)findText {
  return [findText_ stringValue];
}

- (NSTextField*)findTextField {
  return findText_;
}
@end

namespace {

class FindBarCocoaControllerTest : public PlatformTest {
 public:
  virtual void SetUp() {
    PlatformTest::SetUp();

    // TODO(rohitrao): We don't really need to do this once per test.
    // Consider moving it to SetUpTestCase().
    controller_.reset([[FindBarCocoaController alloc] init]);
    [helper_.contentView() addSubview:[controller_ view]];
  }

 protected:
  CocoaTestHelper helper_;
  scoped_nsobject<FindBarCocoaController> controller_;
};

TEST_F(FindBarCocoaControllerTest, ShowAndHide) {
  NSView* findBarView = [controller_ findBarView];

  ASSERT_GT([findBarView frame].origin.y, 0);
  ASSERT_FALSE([controller_ isFindBarVisible]);

  [controller_ showFindBar:NO];
  EXPECT_EQ([findBarView frame].origin.y, 0);
  EXPECT_TRUE([controller_ isFindBarVisible]);

  [controller_ hideFindBar:NO];
  EXPECT_GT([findBarView frame].origin.y, 0);
  EXPECT_FALSE([controller_ isFindBarVisible]);
}

TEST_F(FindBarCocoaControllerTest, SetFindText) {
  NSTextField* findTextField = [controller_ findTextField];

  // Start by making the find bar visible.
  [controller_ showFindBar:NO];
  EXPECT_TRUE([controller_ isFindBarVisible]);

  // Set the find text.
  const NSString* kFindText = @"Google";
  [controller_ setFindText:kFindText];
  EXPECT_EQ(
      NSOrderedSame,
      [[findTextField stringValue] compare:kFindText]);

  // Call clearResults, which doesn't actually clear the find text but
  // simply sets it back to what it was before.  This is silly, but
  // matches the behavior on other platforms.  |details| isn't used by
  // our implementation of clearResults, so it's ok to pass in an
  // empty |details|.
  FindNotificationDetails details;
  [controller_ clearResults:details];
  EXPECT_EQ(
      NSOrderedSame,
      [[findTextField stringValue] compare:kFindText]);
}

TEST_F(FindBarCocoaControllerTest, ResultLabelUpdatesCorrectly) {
  // TODO(rohitrao): Test this.  It may involve creating some dummy
  // FindNotificationDetails objects.
}

TEST_F(FindBarCocoaControllerTest, FindTextIsGlobal) {
  scoped_nsobject<FindBarCocoaController> otherController(
      [[FindBarCocoaController alloc] init]);
  [helper_.contentView() addSubview:[otherController view]];

  // Setting the text in one controller should update the other controller's
  // text as well.
  const NSString* kFindText = @"Respect to the man in the ice cream van";
  [controller_ setFindText:kFindText];
  EXPECT_EQ(
      NSOrderedSame,
      [[controller_ findText] compare:kFindText]);
  EXPECT_EQ(
      NSOrderedSame,
      [[otherController.get() findText] compare:kFindText]);
}

TEST_F(FindBarCocoaControllerTest, SettingFindTextUpdatesFindPboard) {
  const NSString* kFindText =
      @"It's not a bird, it's not a plane, it must be Dave who's on the train";
  [controller_ setFindText:kFindText];
  EXPECT_EQ(
      NSOrderedSame,
      [[[FindPasteboard sharedInstance] findText] compare:kFindText]);
}

}  // namespace
