// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_PAYMENT_REQUEST_DELEGATE_H_
#define COMPONENTS_PAYMENTS_PAYMENT_REQUEST_DELEGATE_H_

namespace payments {

class PaymentRequest;

class PaymentRequestDelegate {
 public:
  virtual ~PaymentRequestDelegate() {}

  // Shows the Payment Request dialog for the given |request|.
  virtual void ShowPaymentRequestDialog(PaymentRequest* request) = 0;
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_PAYMENT_REQUEST_DELEGATE_H_
