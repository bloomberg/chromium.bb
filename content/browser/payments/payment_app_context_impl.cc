// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/payments/payment_app_context_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/stl_util.h"
#include "content/browser/payments/payment_app_manager.h"
#include "content/public/browser/browser_thread.h"

namespace content {

PaymentAppContextImpl::PaymentAppContextImpl() : is_shutdown_(false) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

void PaymentAppContextImpl::Init(
    scoped_refptr<ServiceWorkerContextWrapper> service_worker_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!is_shutdown_);

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&PaymentAppContextImpl::CreatePaymentAppDatabaseOnIO, this,
                 service_worker_context));
}

void PaymentAppContextImpl::Shutdown() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  BrowserThread::PostTaskAndReply(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&PaymentAppContextImpl::ShutdownOnIO, this),
      base::Bind(&PaymentAppContextImpl::DidShutdown, this));
}

void PaymentAppContextImpl::CreatePaymentAppManager(
    mojo::InterfaceRequest<payments::mojom::PaymentAppManager> request) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&PaymentAppContextImpl::CreatePaymentAppManagerOnIO, this,
                 base::Passed(&request)));
}

void PaymentAppContextImpl::PaymentAppManagerHadConnectionError(
    PaymentAppManager* payment_app_manager) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(base::ContainsKey(payment_app_managers_, payment_app_manager));

  payment_app_managers_.erase(payment_app_manager);
}

PaymentAppDatabase* PaymentAppContextImpl::payment_app_database() const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return payment_app_database_.get();
}

PaymentAppContextImpl::~PaymentAppContextImpl() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(is_shutdown_);
}

void PaymentAppContextImpl::CreatePaymentAppDatabaseOnIO(
    scoped_refptr<ServiceWorkerContextWrapper> service_worker_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  payment_app_database_ =
      base::MakeUnique<PaymentAppDatabase>(service_worker_context);
}

void PaymentAppContextImpl::CreatePaymentAppManagerOnIO(
    mojo::InterfaceRequest<payments::mojom::PaymentAppManager> request) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  PaymentAppManager* payment_app_manager =
      new PaymentAppManager(this, std::move(request));
  payment_app_managers_[payment_app_manager] =
      base::WrapUnique(payment_app_manager);
}

void PaymentAppContextImpl::ShutdownOnIO() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  payment_app_managers_.clear();
  payment_app_database_.reset();
}

void PaymentAppContextImpl::DidShutdown() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  is_shutdown_ = true;
}

}  // namespace content
