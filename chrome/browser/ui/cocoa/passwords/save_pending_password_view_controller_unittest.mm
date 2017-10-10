// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/passwords/pending_password_view_controller.h"

#include <Carbon/Carbon.h>  // kVK_Return

#include "base/mac/foundation_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/mac/scoped_nsobject.h"
#include "base/strings/string16.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#import "chrome/browser/ui/cocoa/bubble_combobox.h"
#include "chrome/browser/ui/cocoa/passwords/base_passwords_controller_test.h"
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

  void TearDown() override {
    // Destroy all the views as the parent class will look for alive windows.
    controller_.reset();
    ManagePasswordsControllerTest::TearDown();
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
          GetModelAndCreateIfNull()->pending_password().username_value,
          GetModelAndCreateIfNull()->pending_password().password_value));
  EXPECT_CALL(*ui_controller(), NeverSavePassword()).Times(0);
  [controller().saveButton performClick:nil];

  EXPECT_TRUE([delegate() dismissed]);
}

TEST_F(SavePendingPasswordViewControllerTest,
       UsernameEditableWhenFeatureEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      password_manager::features::kEnableUsernameCorrection);
  SetUpSavePendingState(false);
  EXPECT_TRUE(controller().usernameField.editable);
  EXPECT_NSEQ(base::SysUTF16ToNSString(
                  [delegate() model]->pending_password().username_value),
              [controller().usernameField stringValue]);
}

TEST_F(SavePendingPasswordViewControllerTest,
       UsernameStaticWhenFeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      password_manager::features::kEnableUsernameCorrection);
  SetUpSavePendingState(false);
  EXPECT_FALSE(controller().usernameField.editable);
  EXPECT_NSEQ(base::SysUTF16ToNSString(
                  [delegate() model]->pending_password().username_value),
              [controller().usernameField stringValue]);
}

TEST_F(SavePendingPasswordViewControllerTest,
       ShouldSaveEditedUsernameAndDismissWhenSaveClicked) {
  profile()->GetPrefs()->SetBoolean(
      password_manager::prefs::kWasSignInPasswordPromoClicked, true);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      password_manager::features::kEnableUsernameCorrection);
  SetUpSavePendingState(false);

  [controller().usernameField setStringValue:@"editedusername"];
  EXPECT_NSEQ(@"editedusername", [controller().usernameField stringValue]);

  EXPECT_CALL(
      *ui_controller(),
      SavePassword(base::SysNSStringToUTF16(@"editedusername"),
                   [delegate() model]->pending_password().password_value));
  EXPECT_CALL(*ui_controller(), NeverSavePassword()).Times(0);
  [controller().saveButton performClick:nil];

  EXPECT_TRUE([delegate() dismissed]);
}

TEST_F(SavePendingPasswordViewControllerTest, SelectAnotherPassword) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      password_manager::features::kEnablePasswordSelection);
  SetUpSavePendingState(false);

  NSComboBox* passwordSelection =
      base::mac::ObjCCastStrict<NSComboBox>(controller().passwordField);
  EXPECT_FALSE(passwordSelection.editable);
  [passwordSelection selectItemAtIndex:1];
  EXPECT_NSEQ(@"***", [passwordSelection stringValue]);

  EXPECT_CALL(*ui_controller(), SavePassword(base::ASCIIToUTF16("username"),
                                             base::ASCIIToUTF16("111")));
  EXPECT_CALL(*ui_controller(), NeverSavePassword()).Times(0);
  [controller().saveButton performClick:nil];
}

TEST_F(SavePendingPasswordViewControllerTest, ViewAndEditPassword) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      password_manager::features::kEnablePasswordSelection);
  SetUpSavePendingState(false);

  NSComboBox* passwordSelection =
      base::mac::ObjCCastStrict<NSComboBox>(controller().passwordField);
  [controller().eyeButton performClick:nil];
  EXPECT_TRUE(passwordSelection.editable);
  [passwordSelection setStringValue:@"password123"];

  EXPECT_CALL(*ui_controller(),
              SavePassword(base::ASCIIToUTF16("username"),
                           base::ASCIIToUTF16("password123")));
  EXPECT_CALL(*ui_controller(), NeverSavePassword()).Times(0);
  [controller().saveButton performClick:nil];
}

TEST_F(SavePendingPasswordViewControllerTest, ViewAndSelectPassword) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      password_manager::features::kEnablePasswordSelection);
  SetUpSavePendingState(false);

  NSComboBox* passwordSelection =
      base::mac::ObjCCastStrict<NSComboBox>(controller().passwordField);
  [controller().eyeButton performClick:nil];
  EXPECT_TRUE(passwordSelection.editable);
  [passwordSelection selectItemAtIndex:1];
  EXPECT_NSEQ(@"111", [passwordSelection stringValue]);

  EXPECT_CALL(*ui_controller(), SavePassword(base::ASCIIToUTF16("username"),
                                             base::ASCIIToUTF16("111")));
  EXPECT_CALL(*ui_controller(), NeverSavePassword()).Times(0);
  [controller().saveButton performClick:nil];
}

TEST_F(SavePendingPasswordViewControllerTest, ViewEditAndMaskPassword) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      password_manager::features::kEnablePasswordSelection);
  SetUpSavePendingState(false);

  NSComboBox* passwordSelection =
      base::mac::ObjCCastStrict<NSComboBox>(controller().passwordField);
  [controller().eyeButton performClick:nil];
  EXPECT_TRUE(passwordSelection.editable);
  [passwordSelection setStringValue:@"password123"];
  [controller().eyeButton performClick:nil];
  EXPECT_FALSE(passwordSelection.editable);

  EXPECT_CALL(*ui_controller(),
              SavePassword(base::ASCIIToUTF16("username"),
                           base::ASCIIToUTF16("password123")));
  EXPECT_CALL(*ui_controller(), NeverSavePassword()).Times(0);
  [controller().saveButton performClick:nil];
}

TEST_F(SavePendingPasswordViewControllerTest,
       ShouldNeverAndDismissWhenNeverClicked) {
  SetUpSavePendingState(false);
  EXPECT_CALL(*ui_controller(), SavePassword(_, _)).Times(0);
  EXPECT_CALL(*ui_controller(), NeverSavePassword());
  [controller().neverButton performClick:nil];

  EXPECT_TRUE([delegate() dismissed]);
}

TEST_F(SavePendingPasswordViewControllerTest, ShouldDismissWhenCrossClicked) {
  SetUpSavePendingState(false);
  EXPECT_CALL(*ui_controller(), SavePassword(_, _)).Times(0);
  EXPECT_CALL(*ui_controller(), NeverSavePassword()).Times(0);
  [controller().closeButton performClick:nil];

  EXPECT_TRUE([delegate() dismissed]);
}

TEST_F(SavePendingPasswordViewControllerTest,
       ShouldShowUsernameRowWhenUsernameEmpty) {
  SetUpSavePendingState(true);
  EXPECT_TRUE(controller().usernameField);
}

TEST_F(SavePendingPasswordViewControllerTest, CloseBubbleAndHandleClick) {
  // A user may press mouse down, some navigation closes the bubble, mouse up
  // still sends the action.
  SetUpSavePendingState(false);
  EXPECT_CALL(*ui_controller(), SavePassword(_, _)).Times(0);
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
