// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/payments/test_chrome_payment_request_delegate.h"

#include "content/public/browser/web_contents.h"

namespace payments {

TestChromePaymentRequestDelegate::TestChromePaymentRequestDelegate(
    content::WebContents* web_contents,
    PaymentRequestDialog::ObserverForTest* observer)
    : ChromePaymentRequestDelegate(web_contents), observer_(observer) {}

void TestChromePaymentRequestDelegate::ShowPaymentRequestDialog(
    PaymentRequest* request) {
  PaymentRequestDialog::ShowWebModalPaymentDialog(
      new PaymentRequestDialog(request, observer_), request);
}

}  // namespace payments
