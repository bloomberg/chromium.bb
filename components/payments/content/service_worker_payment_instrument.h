// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_SERVICE_WORKER_PAYMENT_INSTRUMENT_H_
#define COMPONENTS_PAYMENTS_CONTENT_SERVICE_WORKER_PAYMENT_INSTRUMENT_H_

#include "components/payments/content/payment_request_spec.h"
#include "components/payments/core/payment_instrument.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/stored_payment_app.h"
#include "third_party/WebKit/public/platform/modules/payments/payment_app.mojom.h"
#include "third_party/WebKit/public/platform/modules/payments/payment_request.mojom.h"

namespace payments {

// Represents a service worker based payment app.
class ServiceWorkerPaymentInstrument : public PaymentInstrument {
 public:
  ServiceWorkerPaymentInstrument(
      content::BrowserContext* context,
      const GURL& top_level_origin,
      const GURL& frame_origin,
      const PaymentRequestSpec* spec,
      std::unique_ptr<content::StoredPaymentApp> stored_payment_app_info);
  ~ServiceWorkerPaymentInstrument() override;

  // PaymentInstrument:
  void InvokePaymentApp(Delegate* delegate) override;
  bool IsCompleteForPayment() const override;
  bool IsExactlyMatchingMerchantRequest() const override;
  base::string16 GetMissingInfoLabel() const override;
  bool IsValidForCanMakePayment() const override;
  void RecordUse() override;
  base::string16 GetLabel() const override;
  base::string16 GetSublabel() const override;
  bool IsValidForModifier(
      const std::vector<std::string>& method,
      const std::vector<std::string>& supported_networks,
      const std::set<autofill::CreditCard::CardType>& supported_types,
      bool supported_types_specified) const override;
  const gfx::ImageSkia* icon_image_skia() const override;

 private:
  void OnPaymentAppInvoked(mojom::PaymentHandlerResponsePtr response);
  mojom::PaymentRequestEventDataPtr CreatePaymentRequestEventData();

  content::BrowserContext* browser_context_;
  GURL top_level_origin_;
  GURL frame_origin_;
  const PaymentRequestSpec* spec_;
  std::unique_ptr<content::StoredPaymentApp> stored_payment_app_info_;
  std::unique_ptr<gfx::ImageSkia> icon_image_;

  // Weak pointer is fine here since the owner of this object is
  // PaymentRequestState which also owns PaymentResponseHelper.
  Delegate* delegate_;

  base::WeakPtrFactory<ServiceWorkerPaymentInstrument> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerPaymentInstrument);
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_SERVICE_WORKER_PAYMENT_INSTRUMENT_H_
