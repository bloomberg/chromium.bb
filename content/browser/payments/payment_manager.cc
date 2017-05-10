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

  binding_.set_connection_error_handler(
      base::Bind(&PaymentManager::OnConnectionError, base::Unretained(this)));
}

void PaymentManager::Init(const std::string& scope) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  scope_ = GURL(scope);
}

void PaymentManager::SetManifest(
    payments::mojom::PaymentAppManifestPtr manifest,
    SetManifestCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // TODO(zino): Should implement requesting a permission for users to allow
  // the payment app to be registered. Please see http://crbug.com/665949.

  payment_app_context_->payment_app_database()->WriteManifest(
      scope_, std::move(manifest), std::move(callback));
}

void PaymentManager::GetManifest(GetManifestCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  payment_app_context_->payment_app_database()->ReadManifest(
      scope_, std::move(callback));
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

  payment_app_context_->payment_app_database()->WritePaymentInstrument(
      scope_, instrument_key, std::move(details), std::move(callback));
}

void PaymentManager::ClearPaymentInstruments(
    ClearPaymentInstrumentsCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  payment_app_context_->payment_app_database()->ClearPaymentInstruments(
      scope_, std::move(callback));
}

void PaymentManager::OnConnectionError() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  payment_app_context_->PaymentManagerHadConnectionError(this);
}

}  // namespace content
