// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/confirm_quit_panel_controller.h"

#include "base/memory/scoped_nsobject.h"
#import "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/cocoa/confirm_quit.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "testing/gtest_mac.h"
#include "ui/base/accelerators/accelerator_cocoa.h"

namespace {

class ConfirmQuitPanelControllerTest : public CocoaTest {
 public:
  NSString* TestString(NSString* str) {
    str = [str stringByReplacingOccurrencesOfString:@"{Cmd}"
                                         withString:@"\u2318"];
    str = [str stringByReplacingOccurrencesOfString:@"{Ctrl}"
                                         withString:@"\u2303"];
    str = [str stringByReplacingOccurrencesOfString:@"{Opt}"
                                         withString:@"\u2325"];
    str = [str stringByReplacingOccurrencesOfString:@"{Shift}"
                                         withString:@"\u21E7"];
    return str;
  }
};


TEST_F(ConfirmQuitPanelControllerTest, ShowAndDismiss) {
  ConfirmQuitPanelController* controller =
      [ConfirmQuitPanelController sharedController];
  // Test singleton.
  EXPECT_EQ(controller, [ConfirmQuitPanelController sharedController]);
  [controller showWindow:nil];
  [controller dismissPanel];  // Releases self.
  // The controller should still be the singleton instance until after the
  // animation runs and the window closes. That will happen after this test body
  // finishes executing.
  EXPECT_EQ(controller, [ConfirmQuitPanelController sharedController]);
}

TEST_F(ConfirmQuitPanelControllerTest, KeyCombinationForAccelerator) {
  Class controller = [ConfirmQuitPanelController class];

  ui::AcceleratorCocoa item = ui::AcceleratorCocoa(@"q", NSCommandKeyMask);
  EXPECT_NSEQ(TestString(@"{Cmd}Q"),
              [controller keyCombinationForAccelerator:item]);

  item = ui::AcceleratorCocoa(@"c", NSCommandKeyMask | NSShiftKeyMask);
  EXPECT_NSEQ(TestString(@"{Cmd}{Shift}C"),
              [controller keyCombinationForAccelerator:item]);

  item = ui::AcceleratorCocoa(@"h",
      NSCommandKeyMask | NSShiftKeyMask | NSAlternateKeyMask);
  EXPECT_NSEQ(TestString(@"{Cmd}{Opt}{Shift}H"),
              [controller keyCombinationForAccelerator:item]);

  item = ui::AcceleratorCocoa(@"r",
      NSCommandKeyMask | NSShiftKeyMask | NSAlternateKeyMask |
      NSControlKeyMask);
  EXPECT_NSEQ(TestString(@"{Cmd}{Ctrl}{Opt}{Shift}R"),
              [controller keyCombinationForAccelerator:item]);

  item = ui::AcceleratorCocoa(@"o", NSControlKeyMask);
  EXPECT_NSEQ(TestString(@"{Ctrl}O"),
              [controller keyCombinationForAccelerator:item]);

  item = ui::AcceleratorCocoa(@"m", NSShiftKeyMask | NSControlKeyMask);
  EXPECT_NSEQ(TestString(@"{Ctrl}{Shift}M"),
              [controller keyCombinationForAccelerator:item]);

  item = ui::AcceleratorCocoa(@"e", NSCommandKeyMask | NSAlternateKeyMask);
  EXPECT_NSEQ(TestString(@"{Cmd}{Opt}E"),
              [controller keyCombinationForAccelerator:item]);
}

TEST_F(ConfirmQuitPanelControllerTest, PrefMigration) {
  TestingProfileManager manager(
      static_cast<TestingBrowserProcess*>(g_browser_process));
  ASSERT_TRUE(manager.SetUp());

  TestingProfile* profile1 = manager.CreateTestingProfile("one");
  TestingProfile* profile2 = manager.CreateTestingProfile("two");
  TestingProfile* profile3 = manager.CreateTestingProfile("three");

  PrefService* local_state = g_browser_process->local_state();
  EXPECT_FALSE(local_state->HasPrefPath(prefs::kConfirmToQuitEnabled));

  // Case 1: Default (disabled) across all profiles remains set but disabled.
  confirm_quit::MigratePrefToLocalState();
  EXPECT_TRUE(local_state->HasPrefPath(prefs::kConfirmToQuitEnabled));
  EXPECT_FALSE(local_state->GetBoolean(prefs::kConfirmToQuitEnabled));

  local_state->ClearPref(prefs::kConfirmToQuitEnabled);
  EXPECT_FALSE(local_state->HasPrefPath(prefs::kConfirmToQuitEnabled));

  // Case 2: One profile disabled, one enabled, and one default results in
  // set and enabled.
  profile2->GetPrefs()->SetBoolean(prefs::kConfirmToQuitEnabled, true);
  profile3->GetPrefs()->SetBoolean(prefs::kConfirmToQuitEnabled, false);
  confirm_quit::MigratePrefToLocalState();
  EXPECT_TRUE(local_state->HasPrefPath(prefs::kConfirmToQuitEnabled));
  EXPECT_FALSE(profile1->GetPrefs()->HasPrefPath(prefs::kConfirmToQuitEnabled));
  EXPECT_FALSE(profile2->GetPrefs()->HasPrefPath(prefs::kConfirmToQuitEnabled));
  EXPECT_FALSE(profile3->GetPrefs()->HasPrefPath(prefs::kConfirmToQuitEnabled));
  EXPECT_TRUE(local_state->GetBoolean(prefs::kConfirmToQuitEnabled));

  local_state->ClearPref(prefs::kConfirmToQuitEnabled);
  EXPECT_FALSE(local_state->HasPrefPath(prefs::kConfirmToQuitEnabled));

  // Case 3: All explicitly disabled results in set but disabled.
  profile1->GetPrefs()->SetBoolean(prefs::kConfirmToQuitEnabled, false);
  profile2->GetPrefs()->SetBoolean(prefs::kConfirmToQuitEnabled, false);
  profile3->GetPrefs()->SetBoolean(prefs::kConfirmToQuitEnabled, false);
  confirm_quit::MigratePrefToLocalState();
  EXPECT_TRUE(local_state->HasPrefPath(prefs::kConfirmToQuitEnabled));
  EXPECT_FALSE(profile1->GetPrefs()->HasPrefPath(prefs::kConfirmToQuitEnabled));
  EXPECT_FALSE(profile2->GetPrefs()->HasPrefPath(prefs::kConfirmToQuitEnabled));
  EXPECT_FALSE(profile3->GetPrefs()->HasPrefPath(prefs::kConfirmToQuitEnabled));
  EXPECT_FALSE(local_state->GetBoolean(prefs::kConfirmToQuitEnabled));

  local_state->ClearPref(prefs::kConfirmToQuitEnabled);
  EXPECT_FALSE(local_state->HasPrefPath(prefs::kConfirmToQuitEnabled));

  // Case 4: Only migrate once by migrating the default value (false) first,
  // enabling it one already-migrated profile, and then migrating again. The
  // new pref will remain disabled because migration happens once.
  confirm_quit::MigratePrefToLocalState();
  EXPECT_TRUE(local_state->HasPrefPath(prefs::kConfirmToQuitEnabled));
  EXPECT_FALSE(local_state->GetBoolean(prefs::kConfirmToQuitEnabled));
  profile1->GetPrefs()->SetBoolean(prefs::kConfirmToQuitEnabled, true);
  confirm_quit::MigratePrefToLocalState();
  EXPECT_FALSE(local_state->GetBoolean(prefs::kConfirmToQuitEnabled));
  EXPECT_TRUE(profile1->GetPrefs()->GetBoolean(prefs::kConfirmToQuitEnabled));

  manager.DeleteTestingProfile("one");
  manager.DeleteTestingProfile("two");
  manager.DeleteTestingProfile("three");
}

}  // namespace
