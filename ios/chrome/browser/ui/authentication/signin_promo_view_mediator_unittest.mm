// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin_promo_view_mediator.h"

#import "ios/third_party/material_components_ios/src/components/Buttons/src/MaterialButtons.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

class SigninPromoViewMediatorTest : public PlatformTest {
 protected:
  void SetUp() override {
    mediator_ = [[SigninPromoViewMediator alloc] init];

    signin_promo_view_ = OCMStrictClassMock([SigninPromoView class]);
    primary_button_ = OCMStrictClassMock([MDCFlatButton class]);
    OCMStub([signin_promo_view_ primaryButton]).andReturn(primary_button_);
    secondary_button_ = OCMStrictClassMock([MDCFlatButton class]);
    OCMStub([signin_promo_view_ secondaryButton]).andReturn(secondary_button_);
  }

  void TearDown() override {
    mediator_ = nil;
    EXPECT_OCMOCK_VERIFY((id)signin_promo_view_);
    EXPECT_OCMOCK_VERIFY((id)primary_button_);
    EXPECT_OCMOCK_VERIFY((id)secondary_button_);
  }

  void ExpectColdStateConfiguration() {
    OCMExpect([signin_promo_view_ setMode:SigninPromoViewModeColdState]);
    image_view_profile_image_ = nil;
    primary_button_title_ = nil;
    secondary_button_title_ = nil;
  }

  void CheckColdStateConfiguration() {
    EXPECT_EQ(nil, image_view_profile_image_);
    EXPECT_EQ(nil, primary_button_title_);
    EXPECT_EQ(nil, secondary_button_title_);
  }

  void ExpectWarmStateConfiguration() {
    OCMExpect([signin_promo_view_ setMode:SigninPromoViewModeWarmState]);
    OCMExpect([signin_promo_view_
        setProfileImage:[OCMArg checkWithBlock:^BOOL(id value) {
          image_view_profile_image_ = value;
          return YES;
        }]]);
    primary_button_title_ = nil;
    OCMExpect([primary_button_ setTitle:[OCMArg checkWithBlock:^BOOL(id value) {
                                 primary_button_title_ = value;
                                 return YES;
                               }]
                               forState:UIControlStateNormal]);
    secondary_button_title_ = nil;
    OCMExpect([secondary_button_
        setTitle:[OCMArg checkWithBlock:^BOOL(id value) {
          secondary_button_title_ = value;
          return YES;
        }]
        forState:UIControlStateNormal]);
  }

  void CheckWarmStateConfiguration() {
    EXPECT_NE(nil, image_view_profile_image_);
    NSRange profileNameRange =
        [primary_button_title_ rangeOfString:mediator_.userFullName];
    EXPECT_NE(profileNameRange.length, 0u);
    NSRange profileEmailRange =
        [secondary_button_title_ rangeOfString:mediator_.userEmail];
    EXPECT_NE(profileEmailRange.length, 0u);
  }

  // Mediator used for the tests.
  SigninPromoViewMediator* mediator_;

  // Mocks.
  SigninPromoView* signin_promo_view_;
  MDCFlatButton* primary_button_;
  MDCFlatButton* secondary_button_;

  // Value set by -[SigninPromoView setProfileImage:].
  UIImage* image_view_profile_image_;
  // Value set by -[primary_button_ setTitle: forState:UIControlStateNormal].
  NSString* primary_button_title_;
  // Value set by -[secondary_button_ setTitle: forState:UIControlStateNormal].
  NSString* secondary_button_title_;
};

TEST_F(SigninPromoViewMediatorTest, ColdStateConfigureSigninPromoView) {
  EXPECT_EQ(nil, mediator_.userFullName);
  EXPECT_EQ(nil, mediator_.userEmail);
  EXPECT_EQ(nil, mediator_.userImage);

  ExpectColdStateConfiguration();
  [mediator_ configureSigninPromoView:signin_promo_view_];
  CheckColdStateConfiguration();
}

TEST_F(SigninPromoViewMediatorTest,
       WarmStateConfigureSigninPromoViewWithoutImage) {
  mediator_.userFullName = @"John Doe";
  mediator_.userEmail = @"johndoe@example.com";

  ExpectWarmStateConfiguration();
  [mediator_ configureSigninPromoView:signin_promo_view_];
  CheckWarmStateConfiguration();
}

TEST_F(SigninPromoViewMediatorTest,
       WarmStateConfigureSigninPromoViewWithImage) {
  mediator_.userFullName = @"John Doe";
  mediator_.userEmail = @"johndoe@example.com";
  mediator_.userImage = [[UIImage alloc] init];

  ExpectWarmStateConfiguration();
  [mediator_ configureSigninPromoView:signin_promo_view_];
  CheckWarmStateConfiguration();
  EXPECT_EQ(mediator_.userImage, image_view_profile_image_);
}

// Cold state configuration and then warm state configuration.
TEST_F(SigninPromoViewMediatorTest, TwoConfigureSigninPromoView) {
  ExpectColdStateConfiguration();
  [mediator_ configureSigninPromoView:signin_promo_view_];
  CheckColdStateConfiguration();

  mediator_.userFullName = @"John Doe";
  mediator_.userEmail = @"johndoe@example.com";

  ExpectWarmStateConfiguration();
  [mediator_ configureSigninPromoView:signin_promo_view_];
  CheckWarmStateConfiguration();
}

}  // namespace
