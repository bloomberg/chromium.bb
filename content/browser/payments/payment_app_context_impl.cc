// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/payments/payment_app_context_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/stl_util.h"
#include "content/browser/payments/payment_app_database.h"
#include "content/browser/payments/payment_app_manager.h"
#include "content/public/browser/browser_thread.h"

namespace content {

PaymentAppContextImpl::PaymentAppContextImpl() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

void PaymentAppContextImpl::Init(
    scoped_refptr<ServiceWorkerContextWrapper> service_worker_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&PaymentAppContextImpl::CreatePaymentAppDatabaseOnIO, this,
                 service_worker_context));
}

void PaymentAppContextImpl::Shutdown() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&PaymentAppContextImpl::ShutdownOnIO, this));
}

void PaymentAppContextImpl::CreateService(
    mojo::InterfaceRequest<payments::mojom::PaymentAppManager> request) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&PaymentAppContextImpl::CreateServiceOnIOThread, this,
                 base::Passed(&request)));
}

void PaymentAppContextImpl::ServiceHadConnectionError(
    PaymentAppManager* service) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(base::ContainsKey(services_, service));

  services_.erase(service);
}

PaymentAppDatabase* PaymentAppContextImpl::payment_app_database() const {
  return payment_app_database_.get();
}

void PaymentAppContextImpl::GetAllManifests(
    const GetAllManifestsCallback& callback) {
  NOTIMPLEMENTED();
}

PaymentAppContextImpl::~PaymentAppContextImpl() {
  DCHECK(services_.empty());
  DCHECK(!payment_app_database_);
}

void PaymentAppContextImpl::CreatePaymentAppDatabaseOnIO(
    scoped_refptr<ServiceWorkerContextWrapper> service_worker_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  payment_app_database_ =
      base::WrapUnique(new PaymentAppDatabase(service_worker_context));
}

void PaymentAppContextImpl::CreateServiceOnIOThread(
    mojo::InterfaceRequest<payments::mojom::PaymentAppManager> request) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  PaymentAppManager* service = new PaymentAppManager(this, std::move(request));
  services_[service] = base::WrapUnique(service);
}

void PaymentAppContextImpl::ShutdownOnIO() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  services_.clear();
  payment_app_database_.reset();
}

}  // namespace content
