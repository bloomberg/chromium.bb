// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#import "base/scoped_nsobject.h"
#import "chrome/browser/cocoa/about_window_controller.h"
#include "chrome/browser/cocoa/browser_test_helper.h"
#include "chrome/browser/cocoa/cocoa_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace {

class AboutWindowControllerTest : public PlatformTest {
 public:
  virtual void SetUp() {
    PlatformTest::SetUp();
    about_window_controller_.reset([[AboutWindowController alloc]
                                    initWithProfile:nil]);
    // make sure the nib is loaded
    [about_window_controller_ window];
  }

  scoped_nsobject<AboutWindowController> about_window_controller_;
};

TEST_F(AboutWindowControllerTest, TestCopyright) {
  NSString* text = [BuildAboutWindowLegalTextBlock() string];

  // Make sure we have the word "Copyright" in it, which is present in all
  // locales.
  NSRange range = [text rangeOfString:@"Copyright"];
  EXPECT_NE(NSNotFound, range.location);
}

TEST_F(AboutWindowControllerTest, RemovesLinkAnchors) {
  NSString* text = [BuildAboutWindowLegalTextBlock() string];

  // Make sure that we removed the "BEGIN_LINK" and "END_LINK" anchors.
  NSRange range = [text rangeOfString:@"BEGIN_LINK"];
  EXPECT_EQ(NSNotFound, range.location);

  range = [text rangeOfString:@"END_LINK"];
  EXPECT_EQ(NSNotFound, range.location);
}

TEST_F(AboutWindowControllerTest, AwakeNibSetsString) {
  NSAttributedString* legal_text = BuildAboutWindowLegalTextBlock();
  NSAttributedString* text_storage =
      [[about_window_controller_ legalText] textStorage];

  EXPECT_TRUE([legal_text isEqualToAttributedString:text_storage]);
}

TEST_F(AboutWindowControllerTest, TestButton) {
  NSButton* button = [about_window_controller_ updateButton];
  ASSERT_TRUE(button);

  // Not enabled until we know if updates are available.
  ASSERT_FALSE([button isEnabled]);
  [about_window_controller_ upToDateCheckCompleted:YES latestVersion:nil];
  ASSERT_TRUE([button isEnabled]);

  // Make sure the button is hooked up
  ASSERT_EQ([button target], about_window_controller_.get());
  ASSERT_EQ([button action], @selector(updateNow:));

  // Make sure the button is disabled once clicked
  [about_window_controller_ updateNow:about_window_controller_.get()];
  ASSERT_FALSE([button isEnabled]);
}

// Doesn't confirm correctness, but does confirm something happens.
TEST_F(AboutWindowControllerTest, TestCallbacks) {
  NSString *lastText = [[about_window_controller_ updateText]
                         stringValue];
  [about_window_controller_ upToDateCheckCompleted:NO latestVersion:@"foo"];
  ASSERT_FALSE([lastText isEqual:[[about_window_controller_ updateText]
                                   stringValue]]);

  lastText = [[about_window_controller_ updateText] stringValue];
  [about_window_controller_ updateCompleted:NO installs:0];
  ASSERT_FALSE([lastText isEqual:[[about_window_controller_
                                   updateText] stringValue]]);
}

}  // namespace

