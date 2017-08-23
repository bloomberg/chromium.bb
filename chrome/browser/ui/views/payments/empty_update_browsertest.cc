// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <list>

#include "chrome/browser/ui/views/payments/payment_request_browsertest_base.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view_ids.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments {

class PaymentRequestEmptyUpdateTest : public PaymentRequestBrowserTestBase {
 protected:
  PaymentRequestEmptyUpdateTest()
      : PaymentRequestBrowserTestBase(
            "/payment_request_empty_update_test.html") {}

 private:
  DISALLOW_COPY_AND_ASSIGN(PaymentRequestEmptyUpdateTest);
};

IN_PROC_BROWSER_TEST_F(PaymentRequestEmptyUpdateTest, NoCrash) {
  AddAutofillProfile(autofill::test::GetFullProfile());
  InvokePaymentRequestUI();
  OpenShippingAddressSectionScreen();

  ResetEventObserverForSequence(
      std::list<DialogEvent>{DialogEvent::DIALOG_CLOSED});

  ClickOnChildInListViewAndWait(
      /* child_index=*/0, /*total_num_children=*/1,
      DialogViewID::SHIPPING_ADDRESS_SHEET_LIST_VIEW,
      /*wait_for_animation=*/false);

  // No crash indicates a successful test.
}

}  // namespace payments
