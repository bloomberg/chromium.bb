// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/mac/foundation_util.h"
#include "base/memory/scoped_nsobject.h"
#include "chrome/app/chrome_command_ids.h"
#include "testing/platform_test.h"

class MainMenuTest : public PlatformTest {
 public:
  // Recursively find the menu item with the given |tag| in |menu|.
  NSMenuItem* FindMenuItemWithTag(NSMenu* menu, NSInteger tag) {
    NSMenuItem* found = [menu itemWithTag:tag];
    if (found)
      return found;
    NSMenuItem* item;
    for (item in [menu itemArray]) {
      if ([item hasSubmenu]) {
        found = FindMenuItemWithTag([item submenu], tag);
        if (found)
          return found;
      }
    }
    return nil;
  }
};


TEST_F(MainMenuTest, CloseTabPerformClose) {
  scoped_nsobject<NSNib> nib(
      [[NSNib alloc] initWithNibNamed:@"MainMenu"
                               bundle:base::mac::MainAppBundle()]);
  EXPECT_TRUE(nib);

  NSArray* objects = nil;
  EXPECT_TRUE([nib instantiateNibWithOwner:nil
                           topLevelObjects:&objects]);

  // Check that "Close Tab" is mapped to -performClose:. This is needed to
  // ensure the Lion dictionary pop up gets closed on Cmd-W, if it's open.
  // See http://crbug.com/104931 for details.
  BOOL found = NO;
  for (NSUInteger i = 0; i < [objects count]; ++i) {
    if ([[objects objectAtIndex:i] isKindOfClass:[NSMenu class]]) {
      NSMenu* menu = [objects objectAtIndex:i];
      NSMenuItem* closeTabItem = FindMenuItemWithTag(menu, IDC_CLOSE_TAB);
      if (closeTabItem) {
        EXPECT_EQ(@selector(performClose:), [closeTabItem action]);
        found = YES;
        break;
      }
    }
  }
  EXPECT_TRUE(found);
  [objects makeObjectsPerformSelector:@selector(release)];
  [NSApp setMainMenu:nil];
}
