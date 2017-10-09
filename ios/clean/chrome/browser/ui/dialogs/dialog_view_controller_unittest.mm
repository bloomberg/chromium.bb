// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/dialogs/dialog_view_controller.h"

#import "ios/clean/chrome/browser/ui/dialogs/dialog_button_configuration.h"
#import "ios/clean/chrome/browser/ui/dialogs/dialog_button_style.h"
#import "ios/clean/chrome/browser/ui/dialogs/dialog_text_field_configuration.h"
#import "ios/clean/chrome/browser/ui/dialogs/test_helpers/dialog_test_util.h"
#import "ios/clean/chrome/browser/ui/dialogs/test_helpers/test_dialog_view_controller.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using DialogViewControllerTest = PlatformTest;

// Tests that the dialog is properly set up via the DialogConsumer API.
TEST_F(DialogViewControllerTest, VerifySetup) {
  NSString* const kTitle = @"Title";
  NSString* const kMessage = @"message";
  NSArray* const kButtonConfigs = @[ [DialogButtonConfiguration
      configWithText:@"OK"
               style:DialogButtonStyle::DEFAULT] ];
  NSArray* const kTextFieldConfigs =
      @[ [DialogTextFieldConfiguration configWithDefaultText:@"default"
                                             placeholderText:@"placeholder"
                                                      secure:YES] ];
  TestDialogViewController* dialog = [[TestDialogViewController alloc]
      initWithStyle:UIAlertControllerStyleAlert];
  [dialog setDialogTitle:kTitle];
  [dialog setDialogMessage:kMessage];
  [dialog setDialogButtonConfigurations:kButtonConfigs];
  [dialog setDialogTextFieldConfigurations:kTextFieldConfigs];
  // Add the view to a container so that the alert UI is set up.
  UIView* containerView =
      [[UIView alloc] initWithFrame:[UIScreen mainScreen].bounds];
  dialog.view.frame = containerView.bounds;
  [containerView addSubview:dialog.view];
  // Verify setup.
  dialogs_test_util::TestAlertSetup(dialog, kTitle, kMessage, kButtonConfigs,
                                    kTextFieldConfigs);
}
