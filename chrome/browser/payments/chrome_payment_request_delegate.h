// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAYMENTS_CHROME_PAYMENT_REQUEST_DELEGATE_H_
#define CHROME_BROWSER_PAYMENTS_CHROME_PAYMENT_REQUEST_DELEGATE_H_

#include "base/macros.h"
#include "components/payments/payment_request_delegate.h"

namespace payments {

class PaymentRequest;

class ChromePaymentRequestDelegate : public PaymentRequestDelegate {
 public:
  ChromePaymentRequestDelegate() {}
  ~ChromePaymentRequestDelegate() override {}

  void ShowPaymentRequestDialog(PaymentRequest* request) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromePaymentRequestDelegate);
};

}  // namespace payments

#endif  // CHROME_BROWSER_PAYMENTS_CHROME_PAYMENT_REQUEST_DELEGATE_H_
