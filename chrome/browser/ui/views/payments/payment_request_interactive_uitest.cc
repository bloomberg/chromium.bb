// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "chrome/browser/ui/views/payments/payment_request_interactive_uitest_base.h"
#include "components/payments/payment_request.h"
#include "components/payments/payment_request_web_contents_manager.h"

namespace payments {

class PaymentRequestWebContentsManagerTest
    : public PaymentRequestInteractiveTestBase {
 protected:
  PaymentRequestWebContentsManagerTest()
      : PaymentRequestInteractiveTestBase(
            "/payment_request_multiple_requests.html") {}
};

// If the page creates multiple PaymentRequest objects, it should not crash.
IN_PROC_BROWSER_TEST_F(PaymentRequestWebContentsManagerTest, MultipleRequests) {
  const std::vector<PaymentRequest*> payment_requests =
      GetPaymentRequests(GetActiveWebContents());
  EXPECT_EQ(5U, payment_requests.size());
}

class PaymentRequestNoShippingTest : public PaymentRequestInteractiveTestBase {
 protected:
  PaymentRequestNoShippingTest()
      : PaymentRequestInteractiveTestBase(
            "/payment_request_no_shipping_test.html") {}
};

IN_PROC_BROWSER_TEST_F(PaymentRequestNoShippingTest, OpenPaymentRequestSheet) {
  InvokePaymentRequestUI();
}

}  // namespace payments
