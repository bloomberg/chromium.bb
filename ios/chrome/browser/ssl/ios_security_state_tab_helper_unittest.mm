// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ssl/ios_security_state_tab_helper.h"

#include "components/security_state/core/security_state.h"
#import "ios/chrome/browser/ssl/insecure_input_tab_helper.h"
#import "ios/web/public/navigation_item.h"
#import "ios/web/public/navigation_manager.h"
#include "ios/web/public/ssl_status.h"
#import "ios/web/public/test/web_test_with_web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// This test fixture creates an IOSSecurityStateTabHelper and an
// InsecureInputTabHelper for the WebState, then loads a non-secure
// HTML document.
class IOSSecurityStateTabHelperTest : public web::WebTestWithWebState {
 protected:
  void SetUp() override {
    web::WebTestWithWebState::SetUp();
    InsecureInputTabHelper::CreateForWebState(web_state());
    insecure_input_ = InsecureInputTabHelper::FromWebState(web_state());
    ASSERT_TRUE(insecure_input_);

    IOSSecurityStateTabHelper::CreateForWebState(web_state());
    LoadHtml(@"<html><body></body></html>", GURL("http://chromium.test"));
  }

  // Returns the InsecureInputEventData for current WebState().
  security_state::InsecureInputEventData GetInsecureInput() const {
    security_state::SecurityInfo info;
    IOSSecurityStateTabHelper::FromWebState(web_state())
        ->GetSecurityInfo(&info);
    return info.insecure_input_events;
  }

  InsecureInputTabHelper* insecure_input_;
};

// Ensures that |insecure_field_edited| is set only when an editing event has
// been reported.
TEST_F(IOSSecurityStateTabHelperTest, SecurityInfoAfterEditing) {
  // Verify |insecure_field_edited| is not set prematurely.
  security_state::InsecureInputEventData events = GetInsecureInput();
  EXPECT_FALSE(events.insecure_field_edited);

  // Simulate an edit and verify |insecure_field_edited| is noted in the
  // insecure_input_events.
  insecure_input_->DidEditFieldInInsecureContext();
  events = GetInsecureInput();
  EXPECT_TRUE(events.insecure_field_edited);
}

// Ensures that |password_field_shown| is set only when a password field has
// been reported.
TEST_F(IOSSecurityStateTabHelperTest, SecurityInfoWithInsecurePasswordField) {
  // Verify |password_field_shown| is not set prematurely.
  security_state::InsecureInputEventData events = GetInsecureInput();
  EXPECT_FALSE(events.password_field_shown);

  // Simulate a password field display and verify |password_field_shown|
  // is noted in the insecure_input_events.
  insecure_input_->DidShowPasswordFieldInInsecureContext();
  events = GetInsecureInput();
  EXPECT_TRUE(events.password_field_shown);
}

// Ensures that |credit_card_field_edited| is set only when a credit card field
// interaction has been reported.
TEST_F(IOSSecurityStateTabHelperTest, SecurityInfoWithInsecureCreditCardField) {
  // Verify |credit_card_field_edited| is not set prematurely.
  security_state::InsecureInputEventData events = GetInsecureInput();
  EXPECT_FALSE(events.credit_card_field_edited);

  // Simulate a credit card field display and verify |credit_card_field_edited|
  // is noted in the insecure_input_events.
  insecure_input_->DidInteractWithNonsecureCreditCardInput();
  events = GetInsecureInput();
  EXPECT_TRUE(events.credit_card_field_edited);
}
