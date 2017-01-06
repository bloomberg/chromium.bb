// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAYMENTS_PAYMENT_REQUEST_IMPL_H_
#define CHROME_BROWSER_PAYMENTS_PAYMENT_REQUEST_IMPL_H_

#include "components/payments/payment_request.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace content {
class WebContents;
}

namespace payments {

class PaymentRequestImpl : payments::mojom::PaymentRequest,
                           public base::RefCounted<PaymentRequestImpl> {
 public:
  PaymentRequestImpl(
      content::WebContents* web_contents,
      mojo::InterfaceRequest<payments::mojom::PaymentRequest> request);

  // payments::mojom::PaymentRequest "stub"
  void Init(payments::mojom::PaymentRequestClientPtr client,
            std::vector<payments::mojom::PaymentMethodDataPtr> methodData,
            payments::mojom::PaymentDetailsPtr details,
            payments::mojom::PaymentOptionsPtr options) override;
  void Show() override;
  void UpdateWith(payments::mojom::PaymentDetailsPtr details) override {}
  void Abort() override {}
  void Complete(payments::mojom::PaymentComplete result) override {}
  void CanMakePayment() override {}

  void Cancel();
  void OnError();
  payments::mojom::PaymentDetails* details() { return details_.get(); }

  content::WebContents* web_contents() { return web_contents_; }

 private:
  friend class base::RefCounted<PaymentRequestImpl>;
  ~PaymentRequestImpl() override;

  content::WebContents* web_contents_;
  mojo::Binding<payments::mojom::PaymentRequest> binding_;
  payments::mojom::PaymentRequestClientPtr client_;
  payments::mojom::PaymentDetailsPtr details_;

  DISALLOW_COPY_AND_ASSIGN(PaymentRequestImpl);
};

}  // namespace payments

void CreatePaymentRequestHandler(
    content::WebContents* web_contents,
    mojo::InterfaceRequest<payments::mojom::PaymentRequest> request);

#endif  // CHROME_BROWSER_PAYMENTS_PAYMENT_REQUEST_IMPL_H_
