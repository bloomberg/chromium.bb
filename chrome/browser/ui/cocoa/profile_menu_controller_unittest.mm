// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/profile_menu_controller.h"

#include "base/memory/scoped_nsobject.h"
#include "chrome/browser/profiles/avatar_menu_model.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/cocoa/cocoa_profile_test.h"
#include "chrome/browser/ui/cocoa/run_loop_testing.h"
#include "grit/generated_resources.h"
#include "testing/gtest_mac.h"
#include "ui/base/l10n/l10n_util_mac.h"

class ProfileMenuControllerTest : public CocoaProfileTest {
 public:
  ProfileMenuControllerTest() {
    item_.reset([[NSMenuItem alloc] initWithTitle:@"Users"
                                           action:nil
                                    keyEquivalent:@""]);
    controller_.reset(
        [[ProfileMenuController alloc] initWithMainMenuItem:item_]);
  }

  virtual void SetUp() {
    CocoaProfileTest::SetUp();
    ASSERT_TRUE(profile());

    // Spin the runloop so |-initializeMenu| gets called.
    chrome::testing::NSRunLoopRunAllPending();
  }

  void TestBottomItems() {
    NSMenu* menu = [controller() menu];
    NSInteger count = [menu numberOfItems];

    ASSERT_GE(count, 5);

    NSMenuItem* item = [menu itemAtIndex:count - 4];
    EXPECT_TRUE([item isSeparatorItem]);

    item = [menu itemAtIndex:count - 3];
    EXPECT_EQ(@selector(editProfile:), [item action]);

    item = [menu itemAtIndex:count - 2];
    EXPECT_TRUE([item isSeparatorItem]);

    item = [menu itemAtIndex:count - 1];
    EXPECT_EQ(@selector(newProfile:), [item action]);
  }

  void VerifyProfileNamedIsActive(NSString* title, int line) {
    for (NSMenuItem* item in [[controller() menu] itemArray]) {
      if ([[item title] isEqualToString:title]) {
        EXPECT_EQ(NSOnState, [item state]) << [[item title] UTF8String]
          << " (from line " << line << ")";
      } else {
        EXPECT_EQ(NSOffState, [item state]) << [[item title] UTF8String]
          << " (from line " << line << ")";
      }
    }
  }

  ProfileMenuController* controller() { return controller_.get(); }

 private:
  scoped_nsobject<NSMenuItem> item_;
  scoped_nsobject<ProfileMenuController> controller_;
};

TEST_F(ProfileMenuControllerTest, InitializeMenu) {
  NSMenu* menu = [controller() menu];
  // "Testing Profile", <sep>, Edit, <sep>, New.
  ASSERT_EQ(5, [menu numberOfItems]);

  NSMenuItem* item = [menu itemAtIndex:0];
  EXPECT_EQ(@selector(switchToProfile:), [item action]);

  TestBottomItems();
}

TEST_F(ProfileMenuControllerTest, CreateItemWithTitle) {
  NSMenuItem* item =
      [controller() createItemWithTitle:@"Title"
                                 action:@selector(someSelector:)];
  EXPECT_NSEQ(@"Title", [item title]);
  EXPECT_EQ(controller(), [item target]);
  EXPECT_EQ(@selector(someSelector:), [item action]);
  EXPECT_NSEQ(@"", [item keyEquivalent]);
}

TEST_F(ProfileMenuControllerTest, RebuildMenu) {
  NSMenu* menu = [controller() menu];
  EXPECT_EQ(5, [menu numberOfItems]);

  // Create some more profiles on the manager.
  TestingProfileManager* manager = testing_profile_manager();
  manager->CreateTestingProfile("Profile 2");
  manager->CreateTestingProfile("Profile 3");

  // Verify that the menu got rebuilt.
  ASSERT_EQ(7, [menu numberOfItems]);

  NSMenuItem* item = [menu itemAtIndex:0];
  EXPECT_EQ(@selector(switchToProfile:), [item action]);

  item = [menu itemAtIndex:1];
  EXPECT_EQ(@selector(switchToProfile:), [item action]);

  item = [menu itemAtIndex:2];
  EXPECT_EQ(@selector(switchToProfile:), [item action]);

  TestBottomItems();
}

TEST_F(ProfileMenuControllerTest, InitialActiveBrowser) {
  [controller() activeBrowserChangedTo:NULL];
  VerifyProfileNamedIsActive(l10n_util::GetNSString(IDS_DEFAULT_PROFILE_NAME),
                             __LINE__);
}

// Note: BrowserList::SetLastActive() is typically called as part of
// BrowserWindow::Show() and when a Browser becomes active. We don't need a full
// BrowserWindow, so it is called manually.
TEST_F(ProfileMenuControllerTest, SetActiveAndRemove) {
  NSMenu* menu = [controller() menu];
  TestingProfileManager* manager = testing_profile_manager();
  TestingProfile* profile2 = manager->CreateTestingProfile("Profile 2");
  TestingProfile* profile3 = manager->CreateTestingProfile("Profile 3");
  ASSERT_EQ(7, [menu numberOfItems]);

  // Create a browser and "show" it.
  scoped_ptr<Browser> p2_browser(new Browser(Browser::TYPE_TABBED, profile2));
  BrowserList::SetLastActive(p2_browser.get());
  VerifyProfileNamedIsActive(@"Profile 2", __LINE__);

  // Close the browser and make sure it's still active.
  p2_browser.reset();
  VerifyProfileNamedIsActive(@"Profile 2", __LINE__);

  // Open a new browser and make sure it takes effect.
  scoped_ptr<Browser> p3_browser(new Browser(Browser::TYPE_TABBED, profile3));
  BrowserList::SetLastActive(p3_browser.get());
  VerifyProfileNamedIsActive(@"Profile 3", __LINE__);

  p3_browser.reset();
  VerifyProfileNamedIsActive(@"Profile 3", __LINE__);
}
