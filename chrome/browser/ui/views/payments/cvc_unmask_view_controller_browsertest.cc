// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/views/payments/payment_request_browsertest_base.h"
#include "components/autofill/core/browser/autofill_test_utils.h"

namespace payments {

class CvcUnmaskViewControllerTest : public PaymentRequestBrowserTestBase {
 protected:
  CvcUnmaskViewControllerTest()
      : PaymentRequestBrowserTestBase(
            "/payment_request_no_shipping_test.html") {}

 private:
  DISALLOW_COPY_AND_ASSIGN(CvcUnmaskViewControllerTest);
};

IN_PROC_BROWSER_TEST_F(CvcUnmaskViewControllerTest, CvcSentToResponse) {
  AddCreditCard(autofill::test::GetCreditCard());  // Visa.

  InvokePaymentRequestUI();
  ResetEventObserver(DialogEvent::DIALOG_CLOSED);
  PayWithCreditCardAndWait(base::ASCIIToUTF16("012"));

  ExpectBodyContains({"\"cardSecurityCode\": \"012\""});
}

}  // namespace payments
