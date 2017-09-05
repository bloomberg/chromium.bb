// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/payments/payment_manager.h"

#include <utility>

#include "base/bind.h"
#include "base/optional.h"
#include "content/browser/payments/payment_app.pb.h"
#include "content/browser/payments/payment_app_context_impl.h"
#include "content/browser/payments/payment_app_database.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/public/browser/browser_thread.h"

namespace content {

PaymentManager::~PaymentManager() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

PaymentManager::PaymentManager(
    PaymentAppContextImpl* payment_app_context,
    mojo::InterfaceRequest<payments::mojom::PaymentManager> request)
    : payment_app_context_(payment_app_context),
      binding_(this, std::move(request)),
      weak_ptr_factory_(this) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(payment_app_context);

  binding_.set_connection_error_handler(base::BindOnce(
      &PaymentManager::OnConnectionError, base::Unretained(this)));
}

void PaymentManager::Init(const std::string& context,
                          const std::string& scope) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  should_set_payment_app_info_ = true;
  context_ = GURL(context);
  scope_ = GURL(scope);
}

void PaymentManager::DeletePaymentInstrument(
    const std::string& instrument_key,
    PaymentManager::DeletePaymentInstrumentCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  payment_app_context_->payment_app_database()->DeletePaymentInstrument(
      scope_, instrument_key, std::move(callback));
}

void PaymentManager::GetPaymentInstrument(
    const std::string& instrument_key,
    PaymentManager::GetPaymentInstrumentCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  payment_app_context_->payment_app_database()->ReadPaymentInstrument(
      scope_, instrument_key, std::move(callback));
}

void PaymentManager::KeysOfPaymentInstruments(
    PaymentManager::KeysOfPaymentInstrumentsCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  payment_app_context_->payment_app_database()->KeysOfPaymentInstruments(
      scope_, std::move(callback));
}

void PaymentManager::HasPaymentInstrument(
    const std::string& instrument_key,
    PaymentManager::HasPaymentInstrumentCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  payment_app_context_->payment_app_database()->HasPaymentInstrument(
      scope_, instrument_key, std::move(callback));
}

void PaymentManager::SetPaymentInstrument(
    const std::string& instrument_key,
    payments::mojom::PaymentInstrumentPtr details,
    PaymentManager::SetPaymentInstrumentCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (should_set_payment_app_info_) {
    payment_app_context_->payment_app_database()->WritePaymentInstrument(
        scope_, instrument_key, std::move(details),
        base::BindOnce(
            &PaymentManager::SetPaymentInstrumentIntermediateCallback,
            weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  } else {
    payment_app_context_->payment_app_database()->WritePaymentInstrument(
        scope_, instrument_key, std::move(details), std::move(callback));
  }
}

void PaymentManager::SetPaymentInstrumentIntermediateCallback(
    PaymentManager::SetPaymentInstrumentCallback callback,
    payments::mojom::PaymentHandlerStatus status) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (status != payments::mojom::PaymentHandlerStatus::SUCCESS ||
      !should_set_payment_app_info_) {
    std::move(callback).Run(status);
    return;
  }

  payment_app_context_->payment_app_database()->FetchAndWritePaymentAppInfo(
      context_, scope_, user_hint_, std::move(callback));
  should_set_payment_app_info_ = false;
}

void PaymentManager::ClearPaymentInstruments(
    ClearPaymentInstrumentsCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  payment_app_context_->payment_app_database()->ClearPaymentInstruments(
      scope_, std::move(callback));
}

void PaymentManager::SetUserHint(const std::string& user_hint) {
  user_hint_ = user_hint;
  if (should_set_payment_app_info_)
    return;

  payment_app_context_->payment_app_database()->SetPaymentAppUserHint(
      scope_, user_hint_);
}

void PaymentManager::OnConnectionError() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  payment_app_context_->PaymentManagerHadConnectionError(this);
}

}  // namespace content
