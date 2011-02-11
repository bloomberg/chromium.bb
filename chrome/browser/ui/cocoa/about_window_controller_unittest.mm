// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#import "base/scoped_nsobject.h"
#import "chrome/browser/cocoa/keystone_glue.h"
#import "chrome/browser/ui/cocoa/about_window_controller.h"
#include "chrome/browser/ui/cocoa/browser_test_helper.h"
#include "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"

namespace {

void PostAutoupdateStatusNotification(AutoupdateStatus status,
                                             NSString* version) {
  NSNumber* statusNumber = [NSNumber numberWithInt:status];
  NSMutableDictionary* dictionary =
      [NSMutableDictionary dictionaryWithObjects:&statusNumber
                                         forKeys:&kAutoupdateStatusStatus
                                           count:1];
  if (version) {
    [dictionary setObject:version forKey:kAutoupdateStatusVersion];
  }

  NSNotificationCenter* center = [NSNotificationCenter defaultCenter];
  [center postNotificationName:kAutoupdateStatusNotification
                        object:nil
                      userInfo:dictionary];
}

class AboutWindowControllerTest : public CocoaTest {
 public:
  virtual void SetUp() {
    CocoaTest::SetUp();
    about_window_controller_ =
        [[AboutWindowController alloc] initWithProfile:nil];
    EXPECT_TRUE([about_window_controller_ window]);
  }

  virtual void TearDown() {
    [about_window_controller_ close];
    CocoaTest::TearDown();
  }

  AboutWindowController* about_window_controller_;
};

TEST_F(AboutWindowControllerTest, TestCopyright) {
  NSString* text = [[AboutWindowController legalTextBlock] string];

  // Make sure we have the word "Copyright" in it, which is present in all
  // locales.
  NSRange range = [text rangeOfString:@"Copyright"];
  EXPECT_NE(NSNotFound, range.location);
}

TEST_F(AboutWindowControllerTest, RemovesLinkAnchors) {
  NSString* text = [[AboutWindowController legalTextBlock] string];

  // Make sure that we removed the "BEGIN_LINK" and "END_LINK" anchors.
  NSRange range = [text rangeOfString:@"BEGIN_LINK"];
  EXPECT_EQ(NSNotFound, range.location);

  range = [text rangeOfString:@"END_LINK"];
  EXPECT_EQ(NSNotFound, range.location);
}

TEST_F(AboutWindowControllerTest, AwakeNibSetsString) {
  NSAttributedString* legal_text = [AboutWindowController legalTextBlock];
  NSAttributedString* text_storage =
      [[about_window_controller_ legalText] textStorage];

  EXPECT_TRUE([legal_text isEqualToAttributedString:text_storage]);
}

TEST_F(AboutWindowControllerTest, TestButton) {
  NSButton* button = [about_window_controller_ updateButton];
  ASSERT_TRUE(button);

  // Not enabled until we know if updates are available.
  ASSERT_FALSE([button isEnabled]);
  PostAutoupdateStatusNotification(kAutoupdateAvailable, nil);
  ASSERT_TRUE([button isEnabled]);

  // Make sure the button is hooked up
  ASSERT_EQ([button target], about_window_controller_);
  ASSERT_EQ([button action], @selector(updateNow:));
}

// Doesn't confirm correctness, but does confirm something happens.
TEST_F(AboutWindowControllerTest, TestCallbacks) {
  NSString *lastText = [[about_window_controller_ updateText]
                        stringValue];
  PostAutoupdateStatusNotification(kAutoupdateCurrent, @"foo");
  ASSERT_NSNE(lastText, [[about_window_controller_ updateText] stringValue]);

  lastText = [[about_window_controller_ updateText] stringValue];
  PostAutoupdateStatusNotification(kAutoupdateCurrent, @"foo");
  ASSERT_NSEQ(lastText, [[about_window_controller_ updateText] stringValue]);

  lastText = [[about_window_controller_ updateText] stringValue];
  PostAutoupdateStatusNotification(kAutoupdateCurrent, @"bar");
  ASSERT_NSNE(lastText, [[about_window_controller_ updateText] stringValue]);

  lastText = [[about_window_controller_ updateText] stringValue];
  PostAutoupdateStatusNotification(kAutoupdateAvailable, nil);
  ASSERT_NSNE(lastText, [[about_window_controller_ updateText] stringValue]);

  lastText = [[about_window_controller_ updateText] stringValue];
  PostAutoupdateStatusNotification(kAutoupdateCheckFailed, nil);
  ASSERT_NSNE(lastText, [[about_window_controller_ updateText] stringValue]);

#if 0
  // TODO(mark): The kAutoupdateInstalled portion of the test is disabled
  // because it leaks restart dialogs.  If the About box is revised to use
  // a button within the box to advise a restart instead of popping dialogs,
  // these tests should be enabled.

  lastText = [[about_window_controller_ updateText] stringValue];
  PostAutoupdateStatusNotification(kAutoupdateInstalled, @"ver");
  ASSERT_NSNE(lastText, [[about_window_controller_ updateText] stringValue]);

  lastText = [[about_window_controller_ updateText] stringValue];
  PostAutoupdateStatusNotification(kAutoupdateInstalled, nil);
  ASSERT_NSNE(lastText, [[about_window_controller_ updateText] stringValue]);
#endif

  lastText = [[about_window_controller_ updateText] stringValue];
  PostAutoupdateStatusNotification(kAutoupdateInstallFailed, nil);
  ASSERT_NSNE(lastText, [[about_window_controller_ updateText] stringValue]);
}

}  // namespace
