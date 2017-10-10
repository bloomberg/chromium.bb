// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/service_worker_payment_instrument.h"

#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/payment_app_provider.h"
#include "ui/gfx/image/image_skia.h"

namespace payments {

// Service worker payment app provides icon through bitmap, so set 0 as invalid
// resource Id.
ServiceWorkerPaymentInstrument::ServiceWorkerPaymentInstrument(
    content::BrowserContext* context,
    const GURL& top_level_origin,
    const GURL& frame_origin,
    const PaymentRequestSpec* spec,
    std::unique_ptr<content::StoredPaymentApp> stored_payment_app_info)
    : PaymentInstrument(0, PaymentInstrument::Type::SERVICE_WORKER_APP),
      browser_context_(context),
      top_level_origin_(top_level_origin),
      frame_origin_(frame_origin),
      spec_(spec),
      stored_payment_app_info_(std::move(stored_payment_app_info)),
      weak_ptr_factory_(this) {
  DCHECK(browser_context_);
  DCHECK(top_level_origin_.is_valid());
  DCHECK(frame_origin_.is_valid());
  DCHECK(spec_);

  if (stored_payment_app_info_->icon) {
    icon_image_ =
        gfx::ImageSkia::CreateFrom1xBitmap(*(stored_payment_app_info_->icon))
            .DeepCopy();
  }
}

ServiceWorkerPaymentInstrument::~ServiceWorkerPaymentInstrument() {
  if (delegate_) {
    // If there's a payment handler in progress, abort it before destroying this
    // so that it can close its window. Since the PaymentRequest will be
    // destroyed, pass an empty callback to the payment handler.
    content::PaymentAppProvider::GetInstance()->AbortPayment(
        browser_context_, stored_payment_app_info_->registration_id,
        base::Bind([](bool) {}));
  }
}

void ServiceWorkerPaymentInstrument::InvokePaymentApp(Delegate* delegate) {
  delegate_ = delegate;

  content::PaymentAppProvider::GetInstance()->InvokePaymentApp(
      browser_context_, stored_payment_app_info_->registration_id,
      CreatePaymentRequestEventData(),
      base::BindOnce(&ServiceWorkerPaymentInstrument::OnPaymentAppInvoked,
                     weak_ptr_factory_.GetWeakPtr()));
}

mojom::PaymentRequestEventDataPtr
ServiceWorkerPaymentInstrument::CreatePaymentRequestEventData() {
  mojom::PaymentRequestEventDataPtr event_data =
      mojom::PaymentRequestEventData::New();

  event_data->top_level_origin = top_level_origin_;
  event_data->payment_request_origin = frame_origin_;

  if (spec_->details().id.has_value())
    event_data->payment_request_id = spec_->details().id.value();

  event_data->total = spec_->details().total->amount.Clone();

  std::unordered_set<std::string> supported_methods;
  supported_methods.insert(stored_payment_app_info_->enabled_methods.begin(),
                           stored_payment_app_info_->enabled_methods.end());
  for (const auto& modifier : spec_->details().modifiers) {
    std::vector<std::string>::const_iterator it =
        modifier->method_data->supported_methods.begin();
    for (; it != modifier->method_data->supported_methods.end(); it++) {
      if (supported_methods.find(*it) != supported_methods.end())
        break;
    }
    if (it == modifier->method_data->supported_methods.end())
      continue;

    event_data->modifiers.emplace_back(modifier.Clone());
  }

  for (const auto& data : spec_->method_data()) {
    std::vector<std::string>::const_iterator it =
        data->supported_methods.begin();
    for (; it != data->supported_methods.end(); it++) {
      if (supported_methods.find(*it) != supported_methods.end())
        break;
    }
    if (it == data->supported_methods.end())
      continue;

    event_data->method_data.push_back(data.Clone());
  }

  return event_data;
}

void ServiceWorkerPaymentInstrument::OnPaymentAppInvoked(
    mojom::PaymentHandlerResponsePtr response) {
  DCHECK(delegate_);

  if (delegate_ != nullptr) {
    delegate_->OnInstrumentDetailsReady(response->method_name,
                                        response->stringified_details);
    delegate_ = nullptr;
  }
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
  for (const auto& modifier_supported_method : method) {
    if (base::ContainsValue(stored_payment_app_info_->enabled_methods,
                            modifier_supported_method)) {
      return true;
    }
  }
  return false;
}

const gfx::ImageSkia* ServiceWorkerPaymentInstrument::icon_image_skia() const {
  return icon_image_.get();
}

}  // namespace payments
