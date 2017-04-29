// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <list>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/views/payments/payment_request_browsertest_base.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view_ids.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/field_types.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments {

class PaymentRequestShippingOptionViewControllerTest
    : public PaymentRequestBrowserTestBase {
 protected:
  PaymentRequestShippingOptionViewControllerTest()
      : PaymentRequestBrowserTestBase(
            "/payment_request_dynamic_shipping_test.html") {}

 private:
  DISALLOW_COPY_AND_ASSIGN(PaymentRequestShippingOptionViewControllerTest);
};

IN_PROC_BROWSER_TEST_F(PaymentRequestShippingOptionViewControllerTest,
                       OrderSummaryReflectsShippingOption) {
  // In MI state, shipping is $5.00.
  autofill::AutofillProfile michigan = autofill::test::GetFullProfile2();
  michigan.set_use_count(100U);
  AddAutofillProfile(michigan);
  // A Canadian address will have no shipping options.
  autofill::AutofillProfile canada = autofill::test::GetFullProfile();
  canada.SetRawInfo(autofill::ADDRESS_HOME_COUNTRY, base::ASCIIToUTF16("CA"));
  canada.set_use_count(50U);
  AddAutofillProfile(canada);

  InvokePaymentRequestUI();

  // There is no shipping option section, because no address has been selected.
  EXPECT_EQ(nullptr, dialog_view()->GetViewByID(static_cast<int>(
                         DialogViewID::PAYMENT_SHEET_SHIPPING_OPTION_SECTION)));

  // Go to the shipping address screen and select the first address (MI state).
  OpenShippingAddressSectionScreen();
  ResetEventObserverForSequence(std::list<DialogEvent>{
      DialogEvent::BACK_NAVIGATION, DialogEvent::SPEC_DONE_UPDATING});
  ClickOnChildInListViewAndWait(
      /* child_index=*/0, /*total_num_children=*/2,
      DialogViewID::SHIPPING_ADDRESS_SHEET_LIST_VIEW);

  // Michigan address is selected and has standard shipping.
  std::vector<base::string16> shipping_address_labels = GetProfileLabelValues(
      DialogViewID::PAYMENT_SHEET_SHIPPING_ADDRESS_SECTION);
  EXPECT_EQ(base::ASCIIToUTF16("Jane A. Smith"), shipping_address_labels[0]);
  EXPECT_EQ(
      base::ASCIIToUTF16("ACME, 123 Main Street, Unit 1, Greensdale, MI 48838"),
      shipping_address_labels[1]);
  EXPECT_EQ(base::ASCIIToUTF16("13105557889"), shipping_address_labels[2]);

  // The shipping option section exists, and the shipping option is shown.
  std::vector<base::string16> shipping_option_labels =
      GetShippingOptionLabelValues(
          DialogViewID::PAYMENT_SHEET_SHIPPING_OPTION_SECTION);
  EXPECT_EQ(base::ASCIIToUTF16("Standard shipping in US"),
            shipping_option_labels[0]);
  EXPECT_EQ(base::ASCIIToUTF16("$5.00"), shipping_option_labels[1]);

  // Go to the shipping address screen and select the second address (Canada).
  OpenShippingAddressSectionScreen();
  ResetEventObserverForSequence(std::list<DialogEvent>{
      DialogEvent::BACK_NAVIGATION, DialogEvent::SPEC_DONE_UPDATING});
  ClickOnChildInListViewAndWait(
      /* child_index=*/1, /*total_num_children=*/2,
      DialogViewID::SHIPPING_ADDRESS_SHEET_LIST_VIEW);

  // There is no longer shipping option section, because no shipping options are
  // available for Canada.
  EXPECT_EQ(nullptr, dialog_view()->GetViewByID(static_cast<int>(
                         DialogViewID::PAYMENT_SHEET_SHIPPING_OPTION_SECTION)));
}

}  // namespace payments
