// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/payments/payment_app_context_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/stl_util.h"
#include "content/browser/payments/payment_app_manager.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/public/browser/browser_thread.h"

namespace content {

PaymentAppContextImpl::PaymentAppContextImpl(
    scoped_refptr<ServiceWorkerContextWrapper> service_worker_context)
    : service_worker_context_(std::move(service_worker_context)) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
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

ServiceWorkerContextWrapper* PaymentAppContextImpl::service_worker_context()
    const {
  return service_worker_context_.get();
}

void PaymentAppContextImpl::GetAllManifests(
    const GetAllManifestsCallback& callback) {
  NOTIMPLEMENTED();
}

PaymentAppContextImpl::~PaymentAppContextImpl() {
  DCHECK(services_.empty());
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
}

}  // namespace content
