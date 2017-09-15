// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/dialogs/java_script_dialogs/dialog_blocking/java_script_dialog_blocking_confirmation_mediator.h"

#import "base/mac/bind_objc_block.h"
#import "base/mac/foundation_util.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/clean/chrome/browser/ui/commands/java_script_dialog_blocking_commands.h"
#import "ios/clean/chrome/browser/ui/dialogs/dialog_button_configuration.h"
#import "ios/clean/chrome/browser/ui/dialogs/dialog_button_style.h"
#import "ios/clean/chrome/browser/ui/dialogs/dialog_mediator+subclassing.h"
#import "ios/clean/chrome/browser/ui/dialogs/dialog_text_field_configuration.h"
#import "ios/clean/chrome/browser/ui/dialogs/java_script_dialogs/java_script_dialog_request.h"
#import "ios/clean/chrome/browser/ui/dialogs/test_helpers/dialog_test_util.h"
#import "ios/clean/chrome/browser/ui/dialogs/test_helpers/test_dialog_view_controller.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Test dispatcher that performs no-ops for
// JavaScriptDialogBlockingDismissalCommands.
@interface TestJavaScriptDialogBlockingDismissalDispatcher
    : NSObject<JavaScriptDialogBlockingDismissalCommands>
@end

@implementation TestJavaScriptDialogBlockingDismissalDispatcher
- (void)dismissJavaScriptDialogBlockingConfirmation {
}
@end

// A test fixture for DialogMediators.
class JavaScriptDialogBlockingConfirmationMediatorTest : public PlatformTest {
 public:
  JavaScriptDialogBlockingConfirmationMediatorTest()
      : PlatformTest(),
        callback_(base::BindBlockArc(^(bool, NSString*){})),
        dispatcher_(
            [[TestJavaScriptDialogBlockingDismissalDispatcher alloc] init]),
        request_(nil),
        mediator_(nil) {
    request_ = [JavaScriptDialogRequest
        requestWithWebState:&web_state_
                       type:web::JAVASCRIPT_DIALOG_TYPE_ALERT
                  originURL:GURL("test.com")
                    message:nil
          defaultPromptText:nil
                   callback:callback_];
    mediator_ = [[JavaScriptDialogBlockingConfirmationMediator alloc]
        initWithRequest:request_
             dispatcher:dispatcher_];
  }

  ~JavaScriptDialogBlockingConfirmationMediatorTest() override {
    [request_ finishRequestWithSuccess:NO userInput:nil];
  }

  DialogMediator* mediator() { return mediator_; }

 private:
  web::TestWebState web_state_;
  web::DialogClosedCallback callback_;
  __strong TestJavaScriptDialogBlockingDismissalDispatcher* dispatcher_;
  __strong JavaScriptDialogRequest* request_;
  __strong JavaScriptDialogBlockingConfirmationMediator* mediator_;
};

// Tests that the confirmation mediator has two buttons with the appropriate
// text and styles.
TEST_F(JavaScriptDialogBlockingConfirmationMediatorTest, MediatorSetup) {
  EXPECT_EQ([mediator() buttonConfigs].count, 2U);
  DialogButtonConfiguration* confirm_button = [mediator() buttonConfigs][0];
  EXPECT_NSEQ(
      confirm_button.text,
      l10n_util::GetNSString(IDS_IOS_JAVA_SCRIPT_DIALOG_BLOCKING_BUTTON_TEXT));
  EXPECT_EQ(confirm_button.style, DialogButtonStyle::DESTRUCTIVE);
  DialogButtonConfiguration* cancel_button = [mediator() buttonConfigs][1];
  EXPECT_NSEQ(cancel_button.text, l10n_util::GetNSString(IDS_CANCEL));
  EXPECT_EQ(cancel_button.style, DialogButtonStyle::CANCEL);
  EXPECT_EQ([mediator() textFieldConfigs].count, 0U);
}
