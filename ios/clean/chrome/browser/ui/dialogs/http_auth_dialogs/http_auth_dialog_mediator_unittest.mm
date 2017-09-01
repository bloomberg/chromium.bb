// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/dialogs/http_auth_dialogs/http_auth_dialog_mediator.h"

#import "base/mac/foundation_util.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/clean/chrome/browser/ui/commands/http_auth_dialog_commands.h"
#import "ios/clean/chrome/browser/ui/dialogs/dialog_button_configuration.h"
#import "ios/clean/chrome/browser/ui/dialogs/dialog_button_style.h"
#import "ios/clean/chrome/browser/ui/dialogs/dialog_mediator+subclassing.h"
#import "ios/clean/chrome/browser/ui/dialogs/dialog_text_field_configuration.h"
#import "ios/clean/chrome/browser/ui/dialogs/http_auth_dialogs/http_auth_dialog_request.h"
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

namespace {
// Tests whether |button| uses |style| and |text|
void TestButtonConfig(DialogButtonConfiguration* button,
                      DialogButtonStyle style,
                      NSString* text) {
  EXPECT_EQ(button.style, style);
  EXPECT_NSEQ(button.text, text);
}
// Tests whether |text_field| uses |default_text| and |placeholder_text| and is
// |secure|.
void TestTextFieldConfig(DialogTextFieldConfiguration* text_field,
                         NSString* default_text,
                         NSString* placeholder_text,
                         bool secure) {
  EXPECT_NSEQ(text_field.defaultText, default_text);
  EXPECT_NSEQ(text_field.placeholderText, placeholder_text);
  EXPECT_EQ(text_field.secure, secure);
}
}  // namespace

// A test fixture for HTTPAuthDialogMediators.
class HTTPAuthDialogMediatorTest : public PlatformTest {
 public:
  HTTPAuthDialogMediatorTest() {
    NSURLProtectionSpace* protection_space =
        [[NSURLProtectionSpace alloc] initWithHost:@"host"
                                              port:8080
                                          protocol:@"protocol"
                                             realm:@"realm"
                              authenticationMethod:@"authMethod"];
    NSURLCredential* credential = [[NSURLCredential alloc]
        initWithUser:@"user"
            password:@"password"
         persistence:NSURLCredentialPersistencePermanent];
    request_ =
        [HTTPAuthDialogRequest requestWithWebState:&web_state_
                                   protectionSpace:protection_space
                                        credential:credential
                                          callback:^(NSString*, NSString*){
                                              // no-op.
                                          }];
    mediator_ = [[HTTPAuthDialogMediator alloc] initWithRequest:request_];
  }

  ~HTTPAuthDialogMediatorTest() override {
    [request_ completeAuthenticationWithUsername:nil password:nil];
  }

  HTTPAuthDialogRequest* request() { return request_; }
  NSArray<DialogButtonConfiguration*>* buttons() {
    return [mediator_ buttonConfigs];
  }
  NSArray<DialogTextFieldConfiguration*>* text_fields() {
    return [mediator_ textFieldConfigs];
  }

 private:
  web::TestWebState web_state_;
  __strong HTTPAuthDialogRequest* request_;
  __strong HTTPAuthDialogMediator* mediator_;
};

// Tests that the appropriate buttons and text fields have been added.
TEST_F(HTTPAuthDialogMediatorTest, Alert) {
  EXPECT_EQ(buttons().count, 2U);
  TestButtonConfig(buttons()[0], DialogButtonStyle::DEFAULT,
                   l10n_util::GetNSString(IDS_OK));
  TestButtonConfig(buttons()[1], DialogButtonStyle::CANCEL,
                   l10n_util::GetNSString(IDS_CANCEL));
  EXPECT_EQ(text_fields().count, 2U);
  TestTextFieldConfig(
      text_fields()[0], request().defaultUserNameText,
      l10n_util::GetNSString(IDS_IOS_HTTP_LOGIN_DIALOG_USERNAME_PLACEHOLDER),
      false);
  TestTextFieldConfig(
      text_fields()[1], nil,
      l10n_util::GetNSString(IDS_IOS_HTTP_LOGIN_DIALOG_PASSWORD_PLACEHOLDER),
      true);
}
