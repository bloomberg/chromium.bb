// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/passwords/pending_password_view_controller.h"

#include <Carbon/Carbon.h>  // kVK_Return

#include "base/mac/scoped_cftyperef.h"
#include "base/mac/scoped_nsobject.h"
#include "base/strings/string16.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#import "chrome/browser/ui/cocoa/bubble_combobox.h"
#include "chrome/browser/ui/cocoa/passwords/base_passwords_controller_test.h"
#import "chrome/browser/ui/cocoa/passwords/password_item_views.h"
#import "chrome/browser/ui/cocoa/passwords/save_pending_password_view_controller.h"
#include "chrome/browser/ui/cocoa/test/cocoa_test_helper.h"
#include "chrome/browser/ui/passwords/manage_passwords_bubble_model.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller_mock.h"
#include "chrome/common/chrome_features.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#import "ui/base/cocoa/touch_bar_forward_declarations.h"
#import "ui/events/test/cocoa_test_event_utils.h"
// Gmock matcher
using testing::_;

namespace {

// Touch bar item identifiers.
NSString* const kEditTouchBarId = @"EDIT";
NSString* const kNeverTouchBarId = @"NEVER";
NSString* const kSaveTouchBarId = @"SAVE";

class SavePendingPasswordViewControllerTest
    : public ManagePasswordsControllerTest {
 public:
  void SetUpSavePendingState(bool empty_username) {
    ManagePasswordsControllerTest::SetUpSavePendingState(empty_username);
    controller_.reset([[SavePendingPasswordViewController alloc]
        initWithDelegate:delegate()]);
    [controller_ view];
  }

  SavePendingPasswordViewController* controller() {
    return controller_.get();
  }

 private:
  base::scoped_nsobject<SavePendingPasswordViewController> controller_;
};

TEST_F(SavePendingPasswordViewControllerTest,
       ShouldSavePasswordAndDismissWhenSaveClicked) {
  profile()->GetPrefs()->SetBoolean(
      password_manager::prefs::kWasSignInPasswordPromoClicked, true);
  SetUpSavePendingState(false);
  EXPECT_CALL(
      *ui_controller(),
      SavePassword(
          GetModelAndCreateIfNull()->pending_password().username_value));
  EXPECT_CALL(*ui_controller(), NeverSavePassword()).Times(0);
  [controller().saveButton performClick:nil];

  EXPECT_TRUE([delegate() dismissed]);
}

TEST_F(SavePendingPasswordViewControllerTest,
       EditButtonExistsWhenUsernameCorrectionEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      password_manager::features::kEnableUsernameCorrection);
  SetUpSavePendingState(false);
  EXPECT_TRUE(controller().editButton);
  [controller().editButton performClick:nil];
  EXPECT_FALSE([delegate() dismissed]);
}

TEST_F(SavePendingPasswordViewControllerTest,
       EditButtonShouldNotExistIfUsernameCorrectionDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      password_manager::features::kEnableUsernameCorrection);
  SetUpSavePendingState(false);
  EXPECT_FALSE(controller().editButton);
}

TEST_F(SavePendingPasswordViewControllerTest,
       ShouldMakeUsernameFieldEditableWhenEditClicked) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      password_manager::features::kEnableUsernameCorrection);
  SetUpSavePendingState(false);
  EXPECT_TRUE([controller().editButton isEnabled]);
  [controller().editButton performClick:nil];
  EXPECT_FALSE([controller().editButton isEnabled]);

  // Container only contains current version of the row.
  EXPECT_TRUE([controller().passwordItemContainer subviews].count == 1);

  // The username field is editable and contains the initial username.
  PendingPasswordItemView* row =
      [[controller().passwordItemContainer subviews] objectAtIndex:0];
  EXPECT_TRUE([[row usernameField] isEditable]);
  EXPECT_TRUE([[row usernameField] isSelectable]);
  EXPECT_NSEQ(base::SysUTF16ToNSString(
                  [delegate() model]->pending_password().username_value),
              [[row usernameField] stringValue]);
  EXPECT_FALSE([controller().editButton isEnabled]);
}

TEST_F(SavePendingPasswordViewControllerTest,
       EditableUsernameFieldBecomesLabelAfterLosingFocus) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      password_manager::features::kEnableUsernameCorrection);
  SetUpSavePendingState(false);
  [controller().editButton performClick:nil];
  PendingPasswordItemView* row =
      [[controller().passwordItemContainer subviews] objectAtIndex:0];

  // User modifies the username and loses focus. We expect modified username in
  // the label.
  [[row usernameField] setStringValue:@"newusername"];
  [[NSNotificationCenter defaultCenter]
      postNotificationName:NSControlTextDidEndEditingNotification
                    object:[row usernameField]];

  row = [[controller().passwordItemContainer subviews] objectAtIndex:0];
  EXPECT_FALSE([[row usernameField] isEditable]);
  EXPECT_FALSE([[row usernameField] isSelectable]);
  EXPECT_TRUE([controller().editButton isEnabled]);
  EXPECT_NSEQ(@"newusername", [[row usernameField] stringValue]);
  EXPECT_NSEQ(@"newusername",
              base::SysUTF16ToNSString(
                  [delegate() model]->pending_password().username_value));
}

TEST_F(SavePendingPasswordViewControllerTest,
       EditableUsernameFieldBecomesLabelAfterEscKey) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      password_manager::features::kEnableUsernameCorrection);
  SetUpSavePendingState(false);
  [controller().editButton performClick:nil];
  PendingPasswordItemView* row =
      [[controller().passwordItemContainer subviews] objectAtIndex:0];

  // User modifies the username and presses escape. We expect old username
  // restored in the label.
  [[row usernameField] setStringValue:@"tempusername"];
  [[test_window() contentView] addSubview:[row usernameField]];
  [test_window() makePretendKeyWindowAndSetFirstResponder:[row usernameField]];
  [[[row usernameField] currentEditor]
      keyDown:cocoa_test_event_utils::KeyEventWithKeyCode(kVK_Escape, '\e',
                                                          NSKeyDown, 0)];

  row = [[controller().passwordItemContainer subviews] objectAtIndex:0];
  EXPECT_FALSE([[row usernameField] isEditable]);
  EXPECT_FALSE([[row usernameField] isSelectable]);
  EXPECT_TRUE([controller().editButton isEnabled]);
  EXPECT_NSEQ(base::SysUTF16ToNSString(
                  [delegate() model]->pending_password().username_value),
              [[row usernameField] stringValue]);
  EXPECT_NSNE(@"tempusername",
              base::SysUTF16ToNSString(
                  [delegate() model]->pending_password().username_value));
}

TEST_F(SavePendingPasswordViewControllerTest,
       ShouldNeverAndDismissWhenNeverClicked) {
  SetUpSavePendingState(false);
  EXPECT_CALL(*ui_controller(), SavePassword(_)).Times(0);
  EXPECT_CALL(*ui_controller(), NeverSavePassword());
  [controller().neverButton performClick:nil];

  EXPECT_TRUE([delegate() dismissed]);
}

TEST_F(SavePendingPasswordViewControllerTest, ShouldDismissWhenCrossClicked) {
  SetUpSavePendingState(false);
  EXPECT_CALL(*ui_controller(), SavePassword(_)).Times(0);
  EXPECT_CALL(*ui_controller(), NeverSavePassword()).Times(0);
  [controller().closeButton performClick:nil];

  EXPECT_TRUE([delegate() dismissed]);
}

TEST_F(SavePendingPasswordViewControllerTest,
       ShouldShowPasswordRowWhenUsernameNonEmpty) {
  SetUpSavePendingState(false);
  EXPECT_TRUE([controller() createPasswordView]);
}

TEST_F(SavePendingPasswordViewControllerTest,
       ShouldNotShowPasswordRowWhenUsernameEmptyAndUsernameCorrectionDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      password_manager::features::kEnableUsernameCorrection);
  SetUpSavePendingState(true);
  EXPECT_FALSE([controller() createPasswordView]);
}

TEST_F(SavePendingPasswordViewControllerTest, CloseBubbleAndHandleClick) {
  // A user may press mouse down, some navigation closes the bubble, mouse up
  // still sends the action.
  SetUpSavePendingState(false);
  EXPECT_CALL(*ui_controller(), SavePassword(_)).Times(0);
  EXPECT_CALL(*ui_controller(), NeverSavePassword()).Times(0);
  [delegate() setModel:nil];
  [controller().neverButton performClick:nil];
  [controller().saveButton performClick:nil];
}

// Verifies the touch bar items.
TEST_F(SavePendingPasswordViewControllerTest, TouchBar) {
  SetUpSavePendingState(false);
  if (@available(macOS 10.12.2, *)) {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(features::kDialogTouchBar);

    NSTouchBar* touch_bar = [controller() makeTouchBar];
    NSArray* touch_bar_items = [touch_bar itemIdentifiers];
    EXPECT_EQ(3u, [touch_bar_items count]);
    EXPECT_TRUE([touch_bar_items
        containsObject:[controller() touchBarIdForItem:kEditTouchBarId]]);
    EXPECT_TRUE([touch_bar_items
        containsObject:[controller() touchBarIdForItem:kNeverTouchBarId]]);
    EXPECT_TRUE([touch_bar_items
        containsObject:[controller() touchBarIdForItem:kSaveTouchBarId]]);
  }
}

}  // namespace
