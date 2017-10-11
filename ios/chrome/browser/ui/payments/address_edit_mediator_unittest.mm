// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/payments/address_edit_mediator.h"

#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/payments/payment_request_unittest_base.h"
#include "testing/platform_test.h"
#include "third_party/ocmock/gtest_support.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

class PaymentRequestAddressEditMediatorTest : public PaymentRequestUnitTestBase,
                                              public PlatformTest {
 protected:
  void SetUp() override {
    PaymentRequestUnitTestBase::SetUp();

    CreateTestPaymentRequest();
  }

  void TearDown() override { PaymentRequestUnitTestBase::TearDown(); }
};

// Tests that the editor's title is correct in various situations.
TEST_F(PaymentRequestAddressEditMediatorTest, Title) {
  // No address, so the title should ask to add an address.
  AddressEditMediator* mediator =
      [[AddressEditMediator alloc] initWithPaymentRequest:payment_request()
                                                  address:nil];
  EXPECT_TRUE([mediator.title
      isEqualToString:l10n_util::GetNSString(IDS_PAYMENTS_ADD_ADDRESS)]);

  // Complete address, to title should prompt to edit the address.
  autofill::AutofillProfile autofill_profile = autofill::test::GetFullProfile();
  mediator =
      [[AddressEditMediator alloc] initWithPaymentRequest:payment_request()
                                                  address:&autofill_profile];
  EXPECT_TRUE([mediator.title
      isEqualToString:l10n_util::GetNSString(IDS_PAYMENTS_EDIT_ADDRESS)]);

  // Some address fields are missing, so title should ask to add a valid
  // address.
  payment_request()->profile_comparator()->Invalidate(autofill_profile);
  autofill::test::SetProfileInfo(
      &autofill_profile, nullptr, nullptr, nullptr, nullptr, nullptr,
      /* address1= */ "", /* address2= */ "",
      /* city= */ "", nullptr, nullptr, /* country= */ "", nullptr);
  mediator =
      [[AddressEditMediator alloc] initWithPaymentRequest:payment_request()
                                                  address:&autofill_profile];
  EXPECT_TRUE([mediator.title
      isEqualToString:l10n_util::GetNSString(IDS_PAYMENTS_ADD_VALID_ADDRESS)]);
}
