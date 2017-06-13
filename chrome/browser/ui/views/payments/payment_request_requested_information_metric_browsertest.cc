// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/histogram_tester.h"
#include "chrome/browser/ui/views/payments/payment_request_browsertest_base.h"
#include "components/payments/core/journey_logger.h"
#include "content/public/test/browser_test_utils.h"

namespace payments {

class PaymentRequestRequestedInformationNoInfoTest
    : public PaymentRequestBrowserTestBase {
 protected:
  PaymentRequestRequestedInformationNoInfoTest()
      : PaymentRequestBrowserTestBase(
            "/payment_request_no_shipping_test.html") {}

 private:
  DISALLOW_COPY_AND_ASSIGN(PaymentRequestRequestedInformationNoInfoTest);
};

IN_PROC_BROWSER_TEST_F(PaymentRequestRequestedInformationNoInfoTest,
                       RequestedInformationMetric) {
  base::HistogramTester histogram_tester;

  // Show and abort the Payment Request.
  InvokePaymentRequestUI();
  ClickOnCancel();

  // Make sure that only the appropriate enum value was logged.
  histogram_tester.ExpectUniqueSample("PaymentRequest.RequestedInformation",
                                      JourneyLogger::REQUESTED_INFORMATION_NONE,
                                      1);
}

class PaymentRequestRequestedInformationJustShippingTest
    : public PaymentRequestBrowserTestBase {
 protected:
  PaymentRequestRequestedInformationJustShippingTest()
      : PaymentRequestBrowserTestBase(
            "/payment_request_free_shipping_test.html") {}

 private:
  DISALLOW_COPY_AND_ASSIGN(PaymentRequestRequestedInformationJustShippingTest);
};

IN_PROC_BROWSER_TEST_F(PaymentRequestRequestedInformationJustShippingTest,
                       RequestedInformationMetric) {
  base::HistogramTester histogram_tester;

  // Show and abort the Payment Request.
  InvokePaymentRequestUI();
  ClickOnCancel();

  // Make sure that only the appropriate enum value was logged.
  histogram_tester.ExpectUniqueSample(
      "PaymentRequest.RequestedInformation",
      JourneyLogger::REQUESTED_INFORMATION_SHIPPING, 1);
}

class PaymentRequestRequestedInformationJustEmailTest
    : public PaymentRequestBrowserTestBase {
 protected:
  PaymentRequestRequestedInformationJustEmailTest()
      : PaymentRequestBrowserTestBase("/payment_request_email_test.html") {}

 private:
  DISALLOW_COPY_AND_ASSIGN(PaymentRequestRequestedInformationJustEmailTest);
};

IN_PROC_BROWSER_TEST_F(PaymentRequestRequestedInformationJustEmailTest,
                       RequestedInformationMetric) {
  base::HistogramTester histogram_tester;

  // Show and abort the Payment Request.
  InvokePaymentRequestUI();
  ClickOnCancel();

  // Make sure that only the appropriate enum value was logged.
  histogram_tester.ExpectUniqueSample(
      "PaymentRequest.RequestedInformation",
      JourneyLogger::REQUESTED_INFORMATION_EMAIL, 1);
}

class PaymentRequestRequestedInformationJustPhoneTest
    : public PaymentRequestBrowserTestBase {
 protected:
  PaymentRequestRequestedInformationJustPhoneTest()
      : PaymentRequestBrowserTestBase("/payment_request_phone_test.html") {}

 private:
  DISALLOW_COPY_AND_ASSIGN(PaymentRequestRequestedInformationJustPhoneTest);
};

IN_PROC_BROWSER_TEST_F(PaymentRequestRequestedInformationJustPhoneTest,
                       RequestedInformationMetric) {
  base::HistogramTester histogram_tester;

  // Show and abort the Payment Request.
  InvokePaymentRequestUI();
  ClickOnCancel();

  // Make sure that only the appropriate enum value was logged.
  histogram_tester.ExpectUniqueSample(
      "PaymentRequest.RequestedInformation",
      JourneyLogger::REQUESTED_INFORMATION_PHONE, 1);
}

class PaymentRequestRequestedInformationJustNameTest
    : public PaymentRequestBrowserTestBase {
 protected:
  PaymentRequestRequestedInformationJustNameTest()
      : PaymentRequestBrowserTestBase("/payment_request_name_test.html") {}

 private:
  DISALLOW_COPY_AND_ASSIGN(PaymentRequestRequestedInformationJustNameTest);
};

IN_PROC_BROWSER_TEST_F(PaymentRequestRequestedInformationJustNameTest,
                       RequestedInformationMetric) {
  base::HistogramTester histogram_tester;

  // Show and abort the Payment Request.
  InvokePaymentRequestUI();
  ClickOnCancel();

  // Make sure that only the appropriate enum value was logged.
  histogram_tester.ExpectUniqueSample("PaymentRequest.RequestedInformation",
                                      JourneyLogger::REQUESTED_INFORMATION_NAME,
                                      1);
}

class PaymentRequestRequestedInformationEmailAndShippingTest
    : public PaymentRequestBrowserTestBase {
 protected:
  PaymentRequestRequestedInformationEmailAndShippingTest()
      : PaymentRequestBrowserTestBase(
            "/payment_request_email_and_free_shipping_test.html") {}

 private:
  DISALLOW_COPY_AND_ASSIGN(
      PaymentRequestRequestedInformationEmailAndShippingTest);
};

IN_PROC_BROWSER_TEST_F(PaymentRequestRequestedInformationEmailAndShippingTest,
                       RequestedInformationMetric) {
  base::HistogramTester histogram_tester;

  // Show and abort the Payment Request.
  InvokePaymentRequestUI();
  ClickOnCancel();

  // Make sure that only the appropriate enum value was logged.
  histogram_tester.ExpectUniqueSample(
      "PaymentRequest.RequestedInformation",
      JourneyLogger::REQUESTED_INFORMATION_EMAIL |
          JourneyLogger::REQUESTED_INFORMATION_SHIPPING,
      1);
}

class PaymentRequestRequestedInformationPhoneAndShippingTest
    : public PaymentRequestBrowserTestBase {
 protected:
  PaymentRequestRequestedInformationPhoneAndShippingTest()
      : PaymentRequestBrowserTestBase(
            "/payment_request_phone_and_free_shipping_test.html") {}

 private:
  DISALLOW_COPY_AND_ASSIGN(
      PaymentRequestRequestedInformationPhoneAndShippingTest);
};

IN_PROC_BROWSER_TEST_F(PaymentRequestRequestedInformationPhoneAndShippingTest,
                       RequestedInformationMetric) {
  base::HistogramTester histogram_tester;

  // Show and abort the Payment Request.
  InvokePaymentRequestUI();
  ClickOnCancel();

  // Make sure that only the appropriate enum value was logged.
  histogram_tester.ExpectUniqueSample(
      "PaymentRequest.RequestedInformation",
      JourneyLogger::REQUESTED_INFORMATION_PHONE |
          JourneyLogger::REQUESTED_INFORMATION_SHIPPING,
      1);
}

class PaymentRequestRequestedInformationNameAndShippingTest
    : public PaymentRequestBrowserTestBase {
 protected:
  PaymentRequestRequestedInformationNameAndShippingTest()
      : PaymentRequestBrowserTestBase(
            "/payment_request_name_and_free_shipping_test.html") {}

 private:
  DISALLOW_COPY_AND_ASSIGN(
      PaymentRequestRequestedInformationNameAndShippingTest);
};

IN_PROC_BROWSER_TEST_F(PaymentRequestRequestedInformationNameAndShippingTest,
                       RequestedInformationMetric) {
  base::HistogramTester histogram_tester;

  // Show and abort the Payment Request.
  InvokePaymentRequestUI();
  ClickOnCancel();

  // Make sure that only the appropriate enum value was logged.
  histogram_tester.ExpectUniqueSample(
      "PaymentRequest.RequestedInformation",
      JourneyLogger::REQUESTED_INFORMATION_NAME |
          JourneyLogger::REQUESTED_INFORMATION_SHIPPING,
      1);
}

class PaymentRequestRequestedInformationEmailAndPhoneTest
    : public PaymentRequestBrowserTestBase {
 protected:
  PaymentRequestRequestedInformationEmailAndPhoneTest()
      : PaymentRequestBrowserTestBase(
            "/payment_request_email_and_phone_test.html") {}

 private:
  DISALLOW_COPY_AND_ASSIGN(PaymentRequestRequestedInformationEmailAndPhoneTest);
};

IN_PROC_BROWSER_TEST_F(PaymentRequestRequestedInformationEmailAndPhoneTest,
                       RequestedInformationMetric) {
  base::HistogramTester histogram_tester;

  // Show and abort the Payment Request.
  InvokePaymentRequestUI();
  ClickOnCancel();

  // Make sure that only the appropriate enum value was logged.
  histogram_tester.ExpectUniqueSample(
      "PaymentRequest.RequestedInformation",
      JourneyLogger::REQUESTED_INFORMATION_EMAIL |
          JourneyLogger::REQUESTED_INFORMATION_PHONE,
      1);
}

class PaymentRequestRequestedInformationContactDetailsTest
    : public PaymentRequestBrowserTestBase {
 protected:
  PaymentRequestRequestedInformationContactDetailsTest()
      : PaymentRequestBrowserTestBase(
            "/payment_request_contact_details_test.html") {}

 private:
  DISALLOW_COPY_AND_ASSIGN(
      PaymentRequestRequestedInformationContactDetailsTest);
};

IN_PROC_BROWSER_TEST_F(PaymentRequestRequestedInformationContactDetailsTest,
                       RequestedInformationMetric) {
  base::HistogramTester histogram_tester;

  // Show and abort the Payment Request.
  InvokePaymentRequestUI();
  ClickOnCancel();

  // Make sure that only the appropriate enum value was logged.
  histogram_tester.ExpectUniqueSample(
      "PaymentRequest.RequestedInformation",
      JourneyLogger::REQUESTED_INFORMATION_EMAIL |
          JourneyLogger::REQUESTED_INFORMATION_PHONE |
          JourneyLogger::REQUESTED_INFORMATION_NAME,
      1);
}

class PaymentRequestRequestedInformationContactDetailsAndShippingTest
    : public PaymentRequestBrowserTestBase {
 protected:
  PaymentRequestRequestedInformationContactDetailsAndShippingTest()
      : PaymentRequestBrowserTestBase(
            "/payment_request_contact_details_and_free_shipping_test.html") {}

 private:
  DISALLOW_COPY_AND_ASSIGN(
      PaymentRequestRequestedInformationContactDetailsAndShippingTest);
};

IN_PROC_BROWSER_TEST_F(
    PaymentRequestRequestedInformationContactDetailsAndShippingTest,
    RequestedInformationMetric) {
  base::HistogramTester histogram_tester;

  // Show and abort the Payment Request.
  InvokePaymentRequestUI();
  ClickOnCancel();

  // Make sure that only the appropriate enum value was logged.
  histogram_tester.ExpectUniqueSample(
      "PaymentRequest.RequestedInformation",
      JourneyLogger::REQUESTED_INFORMATION_EMAIL |
          JourneyLogger::REQUESTED_INFORMATION_PHONE |
          JourneyLogger::REQUESTED_INFORMATION_NAME |
          JourneyLogger::REQUESTED_INFORMATION_SHIPPING,
      1);
}

}  // namespace payments
