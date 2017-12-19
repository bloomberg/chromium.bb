// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_CONTENT_PAYMENT_REQUEST_DELEGATE_H_
#define COMPONENTS_PAYMENTS_CONTENT_CONTENT_PAYMENT_REQUEST_DELEGATE_H_

#include "components/payments/core/payment_request_delegate.h"

template <class T>
class scoped_refptr;

namespace payments {

class PaymentManifestWebDataService;
class PaymentRequestDisplayManager;

// The delegate for PaymentRequest that can use content.
class ContentPaymentRequestDelegate : public PaymentRequestDelegate {
 public:
  ~ContentPaymentRequestDelegate() override {}

  // Returns the web data service for caching payment method manifests.
  virtual scoped_refptr<PaymentManifestWebDataService>
  GetPaymentManifestWebDataService() const = 0;

  // Returns the PaymentRequestDisplayManager associated with this
  // PaymentRequest's BrowserContext.
  virtual PaymentRequestDisplayManager* GetDisplayManager() = 0;
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_CONTENT_PAYMENT_REQUEST_DELEGATE_H_
