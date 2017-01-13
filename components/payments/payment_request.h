// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_PAYMENT_REQUEST_H_
#define COMPONENTS_PAYMENTS_PAYMENT_REQUEST_H_

#include <memory>

#include "base/optional.h"
#include "components/payments/currency_formatter.h"
#include "components/payments/payment_request.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace content {
class WebContents;
}

namespace payments {

class PaymentRequestDelegate;
class PaymentRequestWebContentsManager;

class PaymentRequest : payments::mojom::PaymentRequest {
 public:
  PaymentRequest(
      content::WebContents* web_contents,
      std::unique_ptr<PaymentRequestDelegate> delegate,
      PaymentRequestWebContentsManager* manager,
      mojo::InterfaceRequest<payments::mojom::PaymentRequest> request);
  ~PaymentRequest() override;

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

  // Returns the CurrencyFormatter instance for this PaymentRequest.
  // |locale_name| should be the result of the browser's GetApplicationLocale().
  // Note: Having multiple currencies per PaymentRequest is not supported; hence
  // the CurrencyFormatter is cached here.
  CurrencyFormatter* GetOrCreateCurrencyFormatter(
      const std::string& currency_code,
      const base::Optional<std::string> currency_system,
      const std::string& locale_name);

  payments::mojom::PaymentDetails* details() { return details_.get(); }

  content::WebContents* web_contents() { return web_contents_; }

 private:
  content::WebContents* web_contents_;
  std::unique_ptr<PaymentRequestDelegate> delegate_;
  // |manager_| owns this PaymentRequest.
  PaymentRequestWebContentsManager* manager_;
  mojo::Binding<payments::mojom::PaymentRequest> binding_;
  payments::mojom::PaymentRequestClientPtr client_;
  payments::mojom::PaymentDetailsPtr details_;
  std::unique_ptr<CurrencyFormatter> currency_formatter_;

  DISALLOW_COPY_AND_ASSIGN(PaymentRequest);
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_PAYMENT_REQUEST_H_
