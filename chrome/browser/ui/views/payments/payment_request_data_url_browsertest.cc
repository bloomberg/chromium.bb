// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/payments/payment_request_browsertest_base.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments {

class PaymentRequestDataUrlTest : public PaymentRequestBrowserTestBase {
 protected:
  PaymentRequestDataUrlTest()
      : PaymentRequestBrowserTestBase(
            "data:text/html,<html><head><meta name=\"viewport\" "
            "content=\"width=device-width, initial-scale=1, "
            "maximum-scale=1\"></head><body><button id=\"buy\" onclick=\"try { "
            "(new PaymentRequest([{supportedMethods: ['basic-card']}], {total: "
            "{label: 'Total',  amount: {currency: 'USD', value: "
            "'1.00'}}})).show(); } catch(e) { "
            "document.getElementById('result').innerHTML = e; }\">Data URL "
            "Test</button><div id='result'></div></body></html>") {}
};

IN_PROC_BROWSER_TEST_F(PaymentRequestDataUrlTest, SecurityError) {
  ASSERT_TRUE(content::ExecuteScript(
      GetActiveWebContents(),
      "(function() { document.getElementById('buy').click(); })();"));
  ExpectBodyContains(
      {"SecurityError: Failed to construct 'PaymentRequest': Must be in a "
       "secure context"});
}

}  // namespace payments
