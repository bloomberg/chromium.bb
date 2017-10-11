// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/first_run_dialog_controller.h"
#include "base/mac/scoped_nsobject.h"
#include "chrome/browser/ui/cocoa/test/cocoa_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

using FirstRunDialogControllerTest = CocoaTest;
using TestController = base::scoped_nsobject<FirstRunDialogViewController>;

TestController MakeTestController(BOOL stats, BOOL browser) {
  return TestController([[FirstRunDialogViewController alloc]
      initWithStatsCheckboxInitiallyChecked:stats
              defaultBrowserCheckboxVisible:browser]);
}

NSView* FindBrowserButton(NSView* view) {
  for (NSView* subview : [view subviews]) {
    if (![subview isKindOfClass:[NSButton class]])
      continue;
    NSString* title = [(NSButton*)subview title];
    if ([title rangeOfString:@"browser"].location != NSNotFound)
      return subview;
  }
  return nil;
}

TEST(FirstRunDialogControllerTest, SetStatsDefault) {
  TestController controller(MakeTestController(YES, YES));
  [controller view];  // Make sure view is actually loaded.
  EXPECT_TRUE([controller isStatsReportingEnabled]);
}

TEST(FirstRunDialogControllerTest, MakeDefaultBrowserDefault) {
  TestController controller(MakeTestController(YES, YES));
  [controller view];
  EXPECT_TRUE([controller isMakeDefaultBrowserEnabled]);
}

TEST(FirstRunDialogControllerTest, ShowBrowser) {
  TestController controller(MakeTestController(YES, YES));
  NSView* checkbox = FindBrowserButton([controller view]);
  EXPECT_FALSE(checkbox.hidden);
}

TEST(FirstRunDialogControllerTest, HideBrowser) {
  TestController controller(MakeTestController(YES, NO));
  NSView* checkbox = FindBrowserButton([controller view]);
  EXPECT_TRUE(checkbox.hidden);
}
