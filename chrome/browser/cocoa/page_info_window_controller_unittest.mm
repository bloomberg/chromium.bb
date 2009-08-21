// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/scoped_nsobject.h"
#import "chrome/browser/cocoa/page_info_window_controller.h"
#include "chrome/browser/cocoa/browser_test_helper.h"
#import "chrome/browser/cocoa/cocoa_test_helper.h"
#include "chrome/common/pref_names.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

class PageInfoWindowControllerTest : public PlatformTest {
  virtual void SetUp() {
    controller_.reset([[PageInfoWindowController alloc] init]);
  }

 public:
  CocoaTestHelper cocoa_helper_;  // Inits Cocoa, creates window, etc...
  BrowserTestHelper helper_;
  scoped_nsobject<PageInfoWindowController> controller_;
};


TEST_F(PageInfoWindowControllerTest, TestImages) {
  [controller_ window];  // Force nib load.
  EXPECT_TRUE([controller_ goodImg]);
  EXPECT_TRUE([controller_ badImg]);
}


TEST_F(PageInfoWindowControllerTest, TestGrow) {
  [controller_ window];  // Force nib load.
  NSRect frame = [[controller_ window] frame];
  [controller_ setShowHistoryBox:YES];
  NSRect newFrame = [[controller_ window] frame];
  EXPECT_GE(newFrame.size.height, frame.size.height);
  EXPECT_LE(newFrame.origin.y, frame.origin.y);
}


TEST_F(PageInfoWindowControllerTest, TestShrink) {
  [controller_ window];  // Force nib to load.
  [controller_ setShowHistoryBox:YES];
  NSRect frame = [[controller_ window] frame];
  [controller_ setShowHistoryBox:NO];
  NSRect newFrame = [[controller_ window] frame];
  EXPECT_LE(newFrame.size.height, frame.size.height);
  EXPECT_GE(newFrame.origin.y, frame.origin.y);
}


TEST_F(PageInfoWindowControllerTest, TestSaveWindowPlacement) {
  PrefService* prefs = helper_.profile()->GetPrefs();
  ASSERT_TRUE(prefs != NULL);

  // Check to make sure there is no existing pref for window placement.
  ASSERT_TRUE(prefs->GetDictionary(prefs::kPageInfoWindowPlacement) == NULL);

  // Ask the window to save its position, then check that a preference
  // exists.  We're technically passing in a pointer to the user prefs
  // and not the local state prefs, but a PrefService* is a
  // PrefService*, and this is a unittest.
  [controller_ saveWindowPositionToPrefs:prefs];
  EXPECT_TRUE(prefs->GetDictionary(prefs::kPageInfoWindowPlacement) != NULL);
}

