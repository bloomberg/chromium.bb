// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/dialogs/dialog_mediator.h"

#import "base/mac/foundation_util.h"
#import "ios/chrome/browser/ui/coordinators/browser_coordinator+internal.h"
#import "ios/clean/chrome/browser/ui/dialogs/dialog_button_configuration.h"
#import "ios/clean/chrome/browser/ui/dialogs/dialog_button_style.h"
#import "ios/clean/chrome/browser/ui/dialogs/dialog_coordinator+subclassing.h"
#import "ios/clean/chrome/browser/ui/dialogs/dialog_mediator+subclassing.h"
#import "ios/clean/chrome/browser/ui/dialogs/dialog_mediator.h"
#import "ios/clean/chrome/browser/ui/dialogs/dialog_text_field_configuration.h"
#import "ios/clean/chrome/browser/ui/dialogs/dialog_view_controller.h"
#import "ios/clean/chrome/browser/ui/dialogs/test_helpers/dialog_test_util.h"
#import "ios/clean/chrome/browser/ui/dialogs/test_helpers/test_dialog_mediator.h"
#import "ios/clean/chrome/browser/ui/dialogs/test_helpers/test_dialog_view_controller.h"
#import "ios/clean/chrome/browser/ui/overlays/test_helpers/test_overlay_queue.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// A test fixture for DialogMediators.
class DialogMediatorTest : public PlatformTest {
 public:
  DialogMediatorTest()
      : PlatformTest(),
        title_(@"title"),
        message_(@"message"),
        button_config_([DialogButtonConfiguration
            configWithText:@"button"
                     style:DialogButtonStyle::DESTRUCTIVE]),
        text_field_config_([DialogTextFieldConfiguration
            configWithDefaultText:@"defaultText"
                  placeholderText:@"placeholderText"
                           secure:YES]),
        mediator_([[TestDialogMediator alloc] init]) {
    mediator_.dialogTitle = title_;
    mediator_.dialogMessage = message_;
    mediator_.buttonConfigs = @[ button_config_ ];
    mediator_.textFieldConfigs = @[ text_field_config_ ];
  }

 protected:
  __strong NSString* title_;
  __strong NSString* message_;
  __strong DialogButtonConfiguration* button_config_;
  __strong DialogTextFieldConfiguration* text_field_config_;
  __strong TestDialogMediator* mediator_;
};

// Tests that the DialogViewController is populated properly from the
// TestDialogMediator.
TEST_F(DialogMediatorTest, ConsumerPopulation) {
  // Create the consumer and update it with |mediator_|.
  TestDialogViewController* consumer = [[TestDialogViewController alloc]
      initWithStyle:UIAlertControllerStyleAlert];
  [mediator_ updateConsumer:consumer];
  // Add the view to a container view to ensure that the alert UI is set up.
  UIView* container =
      [[UIView alloc] initWithFrame:[UIScreen mainScreen].bounds];
  consumer.view.frame = container.bounds;
  [container addSubview:consumer.view];
  // Verify that the test configuration was properly translated to the alert UI.
  dialogs_test_util::TestAlertSetup(consumer, title_, message_,
                                    mediator_.buttonConfigs,
                                    mediator_.textFieldConfigs);
}
