// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/browser/profile_chooser_controller.h"

#include "base/memory/scoped_ptr.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/prefs/pref_service_syncable.h"
#include "chrome/browser/profiles/avatar_menu.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/ui/cocoa/cocoa_profile_test.h"

class ProfileChooserControllerTest : public CocoaProfileTest {
 public:
  ProfileChooserControllerTest() {
  }

  virtual void SetUp() {
    CocoaProfileTest::SetUp();

    testing_profile_manager()->
        CreateTestingProfile("test1", scoped_ptr<PrefServiceSyncable>(),
                             ASCIIToUTF16("Test 1"), 1, std::string(),
                             TestingProfile::TestingFactories());
    testing_profile_manager()->
        CreateTestingProfile("test2", scoped_ptr<PrefServiceSyncable>(),
                             ASCIIToUTF16("Test 2"), 0, std::string(),
                             TestingProfile::TestingFactories());

    menu_ = new AvatarMenu(testing_profile_manager()->profile_info_cache(),
                           NULL, NULL);
    menu_->RebuildMenu();

    // There should be the default profile + two profiles we created.
    EXPECT_EQ(3U, menu_->GetNumberOfItems());

    NSRect frame = [test_window() frame];
    NSPoint point = NSMakePoint(NSMidX(frame), NSMidY(frame));
    controller_ = [[ProfileChooserController alloc] initWithBrowser:browser()
                                                         anchoredAt:point];
  }

  ProfileChooserController* controller() { return controller_; }
  AvatarMenu* menu() { return menu_; }

 private:
  // Weak; releases self.
  ProfileChooserController* controller_;

  // Weak; owned by |controller_|.
  AvatarMenu* menu_;

  DISALLOW_COPY_AND_ASSIGN(ProfileChooserControllerTest);
};

TEST_F(ProfileChooserControllerTest, InitialLayout) {
  [controller() showWindow:nil];
  NSArray* subviews = [[[controller() window] contentView] subviews];

  // Three profiles means we shoud have one active card, two "other" profiles,
  // one separator and one option buttons view.
  EXPECT_EQ(5U, [subviews count]);

  // There should be three buttons in the option buttons view.
  NSArray* buttonSubviews = [[subviews objectAtIndex:0] subviews];
  const SEL buttonSelectors[] = { @selector(showUserManager:),
                                  @selector(addNewProfile:),
                                  @selector(switchToGuestProfile:) };
  EXPECT_EQ(3U, [buttonSubviews count]);
  for (NSUInteger i = 0; i < [buttonSubviews count]; ++i) {
    NSButton* button = static_cast<NSButton*>([buttonSubviews objectAtIndex:i]);
    EXPECT_EQ(buttonSelectors[i], [button action]);
    EXPECT_EQ(controller(), [button target]);
  }

  // There should be a separator.
  EXPECT_TRUE([[subviews objectAtIndex:1] isKindOfClass:[NSBox class]]);

  // There should be two "other profiles" items.
  for (NSUInteger i = 2; i < 4; ++i) {
    int profileIndex = i - 1;  // The separator is a view but not a profile.
    NSButton* button = static_cast<NSButton*>([subviews objectAtIndex:i]);
    EXPECT_EQ(menu()->GetItemAt(profileIndex).name,
              base::SysNSStringToUTF16([button title]));
    EXPECT_EQ(profileIndex, [button tag]);
    EXPECT_EQ(@selector(switchToProfile:), [button action]);
    EXPECT_EQ(controller(), [button target]);
  }

  // There should be a profile avatar and a name in the active card view.
  NSArray* activeCardSubviews = [[subviews objectAtIndex:4] subviews];
  EXPECT_EQ(2U, [activeCardSubviews count]);

  NSView* activeProfileImage = [activeCardSubviews objectAtIndex:0];
  EXPECT_TRUE([activeProfileImage isKindOfClass:[NSImageView class]]);

  NSView* activeProfileName = [activeCardSubviews objectAtIndex:1];
  EXPECT_TRUE([activeProfileName isKindOfClass:[NSTextField class]]);
  EXPECT_EQ(menu()->GetItemAt(0).name, base::SysNSStringToUTF16(
      [static_cast<NSTextField*>(activeProfileName) stringValue]));

  [controller() close];
}

