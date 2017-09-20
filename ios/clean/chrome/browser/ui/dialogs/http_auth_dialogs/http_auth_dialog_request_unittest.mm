// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/dialogs/http_auth_dialogs/http_auth_dialog_request.h"

#import "base/mac/foundation_util.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/dialogs/nsurl_protection_space_util.h"
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

// A test fixture for HTTPAuthDialogMediators.
class HTTPAuthDialogRequestTest : public PlatformTest {
 public:
  HTTPAuthDialogRequestTest()
      : PlatformTest(), callback_username_(nil), callback_password_(nil) {
    protection_space_ =
        [[NSURLProtectionSpace alloc] initWithHost:@"host"
                                              port:8080
                                          protocol:@"protocol"
                                             realm:@"realm"
                              authenticationMethod:@"authMethod"];
    credential_ = [[NSURLCredential alloc]
        initWithUser:@"user"
            password:@"password"
         persistence:NSURLCredentialPersistencePermanent];
    request_ = [HTTPAuthDialogRequest
        requestWithWebState:&web_state_
            protectionSpace:protection_space_
                 credential:credential_
                   callback:^(NSString* username, NSString* password) {
                     callback_username_ = username;
                     callback_password_ = password;
                   }];
  }

  ~HTTPAuthDialogRequestTest() override {
    [request_ completeAuthenticationWithUsername:nil password:nil];
  }

  web::WebState* web_state() { return &web_state_; }
  NSURLProtectionSpace* protection_space() { return protection_space_; }
  NSURLCredential* credential() { return credential_; }
  HTTPAuthDialogRequest* request() { return request_; }
  NSString* callback_username() { return callback_username_; }
  NSString* callback_password() { return callback_password_; }

 private:
  web::TestWebState web_state_;
  __strong NSURLProtectionSpace* protection_space_;
  __strong NSURLCredential* credential_;
  __strong HTTPAuthDialogRequest* request_;
  __strong NSString* callback_username_;
  __strong NSString* callback_password_;
};

// Tests that the factory method creates a request as expected.
TEST_F(HTTPAuthDialogRequestTest, Factory) {
  EXPECT_EQ(web_state(), request().webState);
  EXPECT_NSEQ(l10n_util::GetNSStringWithFixup(IDS_LOGIN_DIALOG_TITLE),
              request().title);
  EXPECT_NSEQ(nsurlprotectionspace_util::MessageForHTTPAuth(protection_space()),
              request().message);
  EXPECT_NSEQ(credential().user, request().defaultUserNameText);
}

// Tests that the request executes the callback as expected.
TEST_F(HTTPAuthDialogRequestTest, Callback) {
  NSString* const kUsername = @"username";
  NSString* const kPassword = @"password";
  [request() completeAuthenticationWithUsername:kUsername password:kPassword];
  EXPECT_NSEQ(kUsername, callback_username());
  EXPECT_NSEQ(kPassword, callback_password());
}
