// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "chrome/browser/ui/views/payments/payment_request_browsertest_base.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments {

class PaymentRequestPaymentAppTest : public PaymentRequestBrowserTestBase {
 protected:
  PaymentRequestPaymentAppTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(PaymentRequestPaymentAppTest);
};

IN_PROC_BROWSER_TEST_F(PaymentRequestPaymentAppTest, ShowNotSupportedError) {
  NavigateTo("/payment_request_bobpay_test.html");
  ResetEventObserver(DialogEvent::NOT_SUPPORTED_ERROR);
  ASSERT_TRUE(content::ExecuteScript(GetActiveWebContents(), "buy();"));
  WaitForObservedEvent();
  ExpectBodyContains({"NotSupportedError"});
}

}  // namespace payments
