// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/dialogs/java_script_dialogs/java_script_dialog_mediator.h"

#import "base/mac/bind_objc_block.h"
#import "base/mac/foundation_util.h"
#include "components/strings/grit/components_strings.h"
#import "ios/clean/chrome/browser/ui/commands/java_script_dialog_commands.h"
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

// Test dispatcher that performs no-ops for JavaScriptDialogDismissalCommands.
@interface TestJavaScriptDialogDismissalDispatcher
    : NSObject<JavaScriptDialogDismissalCommands>
@end

@implementation TestJavaScriptDialogDismissalDispatcher
- (void)dismissJavaScriptDialog {
}
- (void)dismissJavaScriptDialogWithBlockingConfirmation {
}
@end

namespace {
// Tests whether |button| is a configuration for an OK button.
void TestOKButtonConfig(DialogButtonConfiguration* button) {
  EXPECT_EQ(button.style, DialogButtonStyle::DEFAULT);
  EXPECT_NSEQ(button.text, l10n_util::GetNSString(IDS_OK));
}
// Tests whether |button| is a configuration for a Cancel button.
void TestCancelButtonConfig(DialogButtonConfiguration* button) {
  EXPECT_EQ(button.style, DialogButtonStyle::CANCEL);
  EXPECT_NSEQ(button.text, l10n_util::GetNSString(IDS_CANCEL));
}
}

// A test fixture for DialogMediators.
class JavaScriptDialogMediatorTest : public PlatformTest {
 public:
  JavaScriptDialogMediatorTest()
      : PlatformTest(),
        dispatcher_([[TestJavaScriptDialogDismissalDispatcher alloc] init]),
        callback_(base::BindBlockArc(^(bool, NSString*){})),
        url_("http://test.com"),
        type_(web::JAVASCRIPT_DIALOG_TYPE_ALERT),
        default_text_(nil),
        request_(nil),
        mediator_(nil) {}

  ~JavaScriptDialogMediatorTest() override {
    [request_ finishRequestWithSuccess:NO userInput:nil];
  }

  // Setters for configuring the mediator.
  void set_type(web::JavaScriptDialogType type) { type_ = type; }
  void set_default_text(NSString* text) { default_text_ = text; }

  // Lazily instantiates and returns the mediator.  All custom configuration
  // must be finished before this is called.
  JavaScriptDialogMediator* mediator() {
    if (mediator_)
      return mediator_;
    request_ = [JavaScriptDialogRequest requestWithWebState:&web_state_
                                                       type:type_
                                                  originURL:url_
                                                    message:nil
                                          defaultPromptText:default_text_
                                                   callback:callback_];
    mediator_ = [[JavaScriptDialogMediator alloc] initWithRequest:request_
                                                       dispatcher:dispatcher_];
    return mediator_;
  }

 private:
  // Dummy setup objects.
  __strong TestJavaScriptDialogDismissalDispatcher* dispatcher_;
  web::TestWebState web_state_;
  web::DialogClosedCallback callback_;
  GURL url_;
  // Modified for each test.
  web::JavaScriptDialogType type_;
  __strong NSString* default_text_;
  // The mediator for this test.
  __strong JavaScriptDialogRequest* request_;
  __strong JavaScriptDialogMediator* mediator_;
};

// Tests that JavaScript alerts have only one OK button.
TEST_F(JavaScriptDialogMediatorTest, Alert) {
  EXPECT_EQ([mediator() buttonConfigs].count, 1U);
  TestOKButtonConfig([mediator() buttonConfigs][0]);
  EXPECT_EQ([mediator() textFieldConfigs].count, 0U);
}

// Tests that JavaScript confirmations have an OK and Cancel button.
TEST_F(JavaScriptDialogMediatorTest, Confirmation) {
  set_type(web::JAVASCRIPT_DIALOG_TYPE_CONFIRM);
  EXPECT_EQ([mediator() buttonConfigs].count, 2U);
  TestOKButtonConfig([mediator() buttonConfigs][0]);
  TestCancelButtonConfig([mediator() buttonConfigs][1]);
  EXPECT_EQ([mediator() textFieldConfigs].count, 0U);
}

// Tests that JavaScript prompts have an OK button, Cancel button, and a text
// field with the correct default text.
TEST_F(JavaScriptDialogMediatorTest, Prompt) {
  NSString* const kDefaultText = @"Default text";
  set_type(web::JAVASCRIPT_DIALOG_TYPE_PROMPT);
  set_default_text(kDefaultText);
  EXPECT_EQ([mediator() buttonConfigs].count, 2U);
  TestOKButtonConfig([mediator() buttonConfigs][0]);
  TestCancelButtonConfig([mediator() buttonConfigs][1]);
  EXPECT_EQ([mediator() textFieldConfigs].count, 1U);
  DialogTextFieldConfiguration* text_field = [mediator() textFieldConfigs][0];
  EXPECT_NSEQ(text_field.defaultText, kDefaultText);
}
