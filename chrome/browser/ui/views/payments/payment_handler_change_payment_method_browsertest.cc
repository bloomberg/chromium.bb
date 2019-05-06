// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/views/payments/payment_request_browsertest_base.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/re2/src/re2/re2.h"
#include "url/gurl.h"

namespace payments {
namespace {

// Looks for the "supportedMethods" URL and removes its port number.
std::string ClearPortNumber(const std::string& may_contain_method_url) {
  std::string before;
  std::string method;
  std::string after;
  GURL::Replacements port;
  port.ClearPort();
  return re2::RE2::FullMatch(
             may_contain_method_url,
             "(.*\"supportedMethods\":\")(https://.*)(\",\"total\".*)", &before,
             &method, &after)
             ? before + GURL(method).ReplaceComponents(port).spec() + after
             : may_contain_method_url;
}

class PaymentHandlerChangePaymentMethodTest
    : public PaymentRequestBrowserTestBase,
      public testing::WithParamInterface<std::pair<std::string, std::string>> {
 protected:
  PaymentHandlerChangePaymentMethodTest() {}
  ~PaymentHandlerChangePaymentMethodTest() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(PaymentHandlerChangePaymentMethodTest);
};

IN_PROC_BROWSER_TEST_P(PaymentHandlerChangePaymentMethodTest, Test) {
  NavigateTo("/change_payment_method.html");
  std::string actual_output;
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      GetActiveWebContents(), "install();", &actual_output));
  ASSERT_EQ(actual_output, "instruments.set(): Payment handler installed.");

  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      GetActiveWebContents(), GetParam().first, &actual_output));

  // The test expectations are hard-coded, but the embedded test server changes
  // its port number in every test, e.g., https://a.com:34548.
  ASSERT_EQ(ClearPortNumber(actual_output), GetParam().second)
      << "When executing " << GetParam().first;
}

INSTANTIATE_TEST_SUITE_P(
    Tests,
    PaymentHandlerChangePaymentMethodTest,
    testing::Values(
        std::make_pair(
            "testNoHandler();",
            "PaymentRequest.show(): changePaymentMethod() returned: null"),
        std::make_pair("testReject()",
                       "PaymentRequest.show() rejected with: Error for test"),
        std::make_pair(
            "testThrow()",
            "PaymentRequest.show() rejected with: Error: Error for test"),
        std::make_pair(
            "testDetails()",
            "PaymentRequest.show(): changePaymentMethod() returned: "
            "{\"error\":\"Error for test\","
            "\"modifiers\":[{\"data\":{\"soup\":\"potato\"},"
            "\"supportedMethods\":\"https://a.com/\","
            "\"total\":{\"amount\":{\"currency\":\"EUR\",\"value\":\"0.03\"},"
            "\"label\":\"\",\"pending\":false}}],"
            "\"paymentMethodErrors\":{\"country\":\"Unsupported country\"},"
            "\"total\":{\"currency\":\"GBP\",\"value\":\"0.02\"}}")));

}  // namespace
}  // namespace payments
