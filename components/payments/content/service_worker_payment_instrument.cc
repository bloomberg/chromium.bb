// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/service_worker_payment_instrument.h"

#include "base/strings/utf_string_conversions.h"
#include "ui/gfx/image/image_skia.h"

namespace payments {

// Service worker payment app provides icon through bitmap, so set 0 as invalid
// resource Id.
ServiceWorkerPaymentInstrument::ServiceWorkerPaymentInstrument(
    std::unique_ptr<content::StoredPaymentApp> stored_payment_app_info)
    : PaymentInstrument(0, PaymentInstrument::Type::SERVICE_WORKER_APP),
      stored_payment_app_info_(std::move(stored_payment_app_info)) {
  if (stored_payment_app_info_->icon) {
    icon_image_ =
        gfx::ImageSkia::CreateFrom1xBitmap(*(stored_payment_app_info_->icon))
            .DeepCopy();
  }
}

ServiceWorkerPaymentInstrument::~ServiceWorkerPaymentInstrument() {}

void ServiceWorkerPaymentInstrument::InvokePaymentApp(
    PaymentInstrument::Delegate* delegate) {
  NOTIMPLEMENTED();
}

bool ServiceWorkerPaymentInstrument::IsCompleteForPayment() const {
  return true;
}

bool ServiceWorkerPaymentInstrument::IsExactlyMatchingMerchantRequest() const {
  return true;
}

base::string16 ServiceWorkerPaymentInstrument::GetMissingInfoLabel() const {
  NOTREACHED();
  return base::string16();
}

bool ServiceWorkerPaymentInstrument::IsValidForCanMakePayment() const {
  NOTIMPLEMENTED();
  return true;
}

void ServiceWorkerPaymentInstrument::RecordUse() {
  NOTIMPLEMENTED();
}

base::string16 ServiceWorkerPaymentInstrument::GetLabel() const {
  return base::UTF8ToUTF16(stored_payment_app_info_->name);
}

base::string16 ServiceWorkerPaymentInstrument::GetSublabel() const {
  return base::UTF8ToUTF16(stored_payment_app_info_->scope.GetOrigin().spec());
}

bool ServiceWorkerPaymentInstrument::IsValidForModifier(
    const std::vector<std::string>& method,
    const std::vector<std::string>& supported_networks,
    const std::set<autofill::CreditCard::CardType>& supported_types,
    bool supported_types_specified) const {
  NOTIMPLEMENTED();
  return false;
}

const gfx::ImageSkia* ServiceWorkerPaymentInstrument::icon_image_skia() const {
  return icon_image_.get();
}

}  // namespace payments
