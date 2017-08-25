// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/dialogs/test_helpers/dialog_test_util.h"

#import "ios/clean/chrome/browser/ui/dialogs/dialog_button_configuration.h"
#import "ios/clean/chrome/browser/ui/dialogs/dialog_mediator+subclassing.h"
#import "ios/clean/chrome/browser/ui/dialogs/dialog_text_field_configuration.h"
#import "ios/clean/chrome/browser/ui/dialogs/dialog_view_controller.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace dialogs_test_util {

void TestAlertSetup(DialogViewController* view_controller,
                    NSString* title,
                    NSString* message,
                    NSArray<DialogButtonConfiguration*>* buttons,
                    NSArray<DialogTextFieldConfiguration*>* fields) {
  EXPECT_TRUE(view_controller.viewLoaded);
  EXPECT_NSEQ(title, view_controller.title);
  EXPECT_NSEQ(message, view_controller.message);
  ASSERT_EQ(view_controller.actions.count, buttons.count);
  for (NSUInteger index = 0; index < buttons.count; ++index) {
    UIAlertAction* button_action = view_controller.actions[index];
    DialogButtonConfiguration* button_config = buttons[index];
    EXPECT_NSEQ(button_config.text, button_action.title);
    UIAlertActionStyle button_style = [DialogViewController
        alertStyleForDialogButtonStyle:button_config.style];
    EXPECT_EQ(button_style, button_action.style);
  }
  ASSERT_EQ(view_controller.textFields.count, fields.count);
  for (NSUInteger index = 0; index < fields.count; ++index) {
    UITextField* text_field = view_controller.textFields[index];
    DialogTextFieldConfiguration* field_config = fields[index];
    EXPECT_NSEQ(field_config.defaultText, text_field.text);
    EXPECT_NSEQ(field_config.placeholderText, text_field.placeholder);
    EXPECT_EQ(field_config.secure, text_field.secureTextEntry);
  }
}

void TestAlertSetup(DialogViewController* view_controller,
                    DialogMediator* mediator) {
  TestAlertSetup(view_controller, [mediator dialogTitle],
                 [mediator dialogMessage], [mediator buttonConfigs],
                 [mediator textFieldConfigs]);
}

}  // namespace dialogs_test_util
