// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/scoped_nsobject.h"
#include "chrome/browser/cocoa/browser_test_helper.h"
#import "chrome/browser/cocoa/cocoa_test_helper.h"
#import "chrome/browser/cocoa/nswindow_local_state.h"
#include "chrome/common/pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

class NSWindowLocalStateTest : public CocoaTest {
  virtual void SetUp() {
    CocoaTest::SetUp();
    path_ = L"NSWindowLocalStateTest";
    window_ =
        [[NSWindow alloc] initWithContentRect:NSMakeRect(100, 100, 20, 20)
                                    styleMask:NSTitledWindowMask
                                      backing:NSBackingStoreBuffered
                                        defer:NO];
    browser_helper_.profile()->GetPrefs()->RegisterDictionaryPref(path_);
  }

  virtual void TearDown() {
    [window_ close];
    CocoaTest::TearDown();
  }

 public:
  BrowserTestHelper browser_helper_;
  NSWindow* window_;
  const wchar_t* path_;
};

TEST_F(NSWindowLocalStateTest, SaveWindowPlacement) {
  PrefService* prefs = browser_helper_.profile()->GetPrefs();
  ASSERT_TRUE(prefs != NULL);

  // Check to make sure there is no existing pref for window placement.
  ASSERT_TRUE(prefs->GetDictionary(path_) == NULL);

  // Ask the window to save its position, then check that a preference
  // exists.  We're technically passing in a pointer to the user prefs
  // and not the local state prefs, but a PrefService* is a
  // PrefService*, and this is a unittest.
  [window_ saveWindowPositionToPrefs:prefs withPath:path_];
  EXPECT_TRUE(prefs->GetDictionary(path_) != NULL);
  int x, y;
  DictionaryValue* windowPrefs = prefs->GetMutableDictionary(path_);
  windowPrefs->GetInteger(L"x", &x);
  windowPrefs->GetInteger(L"y", &y);
  EXPECT_EQ(x, [window_ frame].origin.x);
  EXPECT_EQ(y, [window_ frame].origin.y);
}

TEST_F(NSWindowLocalStateTest, RestoreWindowPlacement) {
  PrefService* prefs = browser_helper_.profile()->GetPrefs();
  DictionaryValue* windowPrefs = prefs->GetMutableDictionary(path_);

  // Large enough so that the window is on screen without cascasding to a
  // totally new location.
  const int value = 420;
  windowPrefs->SetInteger(L"x", value);
  windowPrefs->SetInteger(L"y", value);
  [window_ restoreWindowPositionFromPrefs:prefs withPath:path_];
  EXPECT_LT(value, [window_ frame].origin.x);
  EXPECT_GT(value, [window_ frame].origin.y);
}
