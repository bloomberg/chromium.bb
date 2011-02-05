// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#import "chrome/browser/ui/cocoa/applescript/browsercrapplication+applescript.h"
#import "chrome/browser/ui/cocoa/applescript/constants_applescript.h"
#import "chrome/browser/ui/cocoa/applescript/window_applescript.h"
#include "chrome/test/in_process_browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "ui/gfx/size.h"

typedef InProcessBrowserTest BrowserCrApplicationAppleScriptTest;

// Create windows of different |Type|.
IN_PROC_BROWSER_TEST_F(BrowserCrApplicationAppleScriptTest, Creation) {
  // Create additional |Browser*| objects of different type.
  Profile* profile = browser()->profile();
  Browser* b1 = Browser::CreateForType(Browser::TYPE_POPUP, profile);
  Browser* b2 = Browser::CreateForApp("", gfx::Size(), profile, true);
  Browser* b3 = Browser::CreateForApp("", gfx::Size(), profile, false);

  EXPECT_EQ(4U, [[NSApp appleScriptWindows] count]);
  for (WindowAppleScript* window in [NSApp appleScriptWindows]) {
    EXPECT_NSEQ(AppleScript::kWindowsProperty,
                [window containerProperty]);
    EXPECT_NSEQ(NSApp, [window container]);
  }

  // Close the additional browsers.
  b1->CloseAllTabs();
  b2->CloseAllTabs();
  b3->CloseAllTabs();
}

// Insert a new window.
IN_PROC_BROWSER_TEST_F(BrowserCrApplicationAppleScriptTest, InsertWindow) {
  // Emulate what applescript would do when creating a new window.
  // Emulate a script like |set var to make new window with properties
  // {visible:false}|.
  scoped_nsobject<WindowAppleScript> aWindow([[WindowAppleScript alloc] init]);
  scoped_nsobject<NSNumber> var([[aWindow.get() uniqueID] copy]);
  [aWindow.get() setValue:[NSNumber numberWithBool:YES] forKey:@"isVisible"];

  [NSApp insertInAppleScriptWindows:aWindow.get()];

  // Represents the window after it is added.
  WindowAppleScript* window = [[NSApp appleScriptWindows] objectAtIndex:0];
  EXPECT_NSEQ([NSNumber numberWithBool:YES],
              [aWindow.get() valueForKey:@"isVisible"]);
  EXPECT_EQ([window container], NSApp);
  EXPECT_NSEQ(AppleScript::kWindowsProperty,
              [window containerProperty]);
  EXPECT_NSEQ(var, [window uniqueID]);
}

// Inserting and deleting windows.
IN_PROC_BROWSER_TEST_F(BrowserCrApplicationAppleScriptTest,
                       InsertAndDeleteWindows) {
  scoped_nsobject<WindowAppleScript> aWindow;
  int count;
  // Create a bunch of windows.
  for (int i = 0; i < 5; ++i) {
    for (int j = 0; j < 3; ++j) {
      aWindow.reset([[WindowAppleScript alloc] init]);
      [NSApp insertInAppleScriptWindows:aWindow.get()];
    }
    count = 3 * i + 4;
    EXPECT_EQ(count, (int)[[NSApp appleScriptWindows] count]);
  }

  // Remove all the windows, just created.
  count = (int)[[NSApp appleScriptWindows] count];
  for (int i = 0; i < 5; ++i) {
    for(int j = 0; j < 3; ++j) {
      [NSApp removeFromAppleScriptWindowsAtIndex:0];
    }
    count = count - 3;
    EXPECT_EQ(count, (int)[[NSApp appleScriptWindows] count]);
  }
}

// Check for objectSpecifer of the root scripting object.
IN_PROC_BROWSER_TEST_F(BrowserCrApplicationAppleScriptTest, ObjectSpecifier) {
  // Should always return nil to indicate its the root scripting object.
  EXPECT_EQ(nil, [NSApp objectSpecifier]);
}

// Bookmark folders at the root level.
IN_PROC_BROWSER_TEST_F(BrowserCrApplicationAppleScriptTest, BookmarkFolders) {
  NSArray* bookmarkFolders = [NSApp bookmarkFolders];
  EXPECT_EQ(2U, [bookmarkFolders count]);

  for (BookmarkFolderAppleScript* bookmarkFolder in bookmarkFolders) {
    EXPECT_EQ(NSApp,
              [bookmarkFolder container]);
    EXPECT_NSEQ(AppleScript::kBookmarkFoldersProperty,
                [bookmarkFolder containerProperty]);
  }

  EXPECT_NSEQ(@"Other Bookmarks", [[NSApp otherBookmarks] title]);
  EXPECT_NSEQ(@"Bookmarks Bar", [[NSApp bookmarksBar] title]);
}
