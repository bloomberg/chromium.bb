// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/payments/payment_app_provider_impl.h"

#include "content/browser/payments/payment_app_context_impl.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_metrics.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/browser/storage_partition_impl.h"
#include "content/common/service_worker/service_worker_status_code.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"

namespace content {
namespace {

void DidGetAllManifestsOnIO(
    const PaymentAppProvider::GetAllManifestsCallback& callback,
    PaymentAppProvider::Manifests manifests) {
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(callback, base::Passed(std::move(manifests))));
}

void GetAllManifestsOnIO(
    scoped_refptr<PaymentAppContextImpl> payment_app_context,
    const PaymentAppProvider::GetAllManifestsCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  payment_app_context->payment_app_database()->ReadAllManifests(
      base::Bind(&DidGetAllManifestsOnIO, callback));
}

void DidDispatchPaymentRequestEvent(
    scoped_refptr<ServiceWorkerVersion> active_version,
    int request_id,
    ServiceWorkerStatusCode service_worker_status,
    base::Time dispatch_event_time) {
  active_version->FinishRequest(request_id,
                                service_worker_status == SERVICE_WORKER_OK,
                                dispatch_event_time);
}

void DispatchPaymentRequestEventError(
    ServiceWorkerStatusCode service_worker_status) {
  NOTIMPLEMENTED();
}

void DispatchPaymentRequestEvent(
    payments::mojom::PaymentAppRequestPtr app_request,
    scoped_refptr<ServiceWorkerVersion> active_version) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(active_version);

  int request_id = active_version->StartRequest(
      ServiceWorkerMetrics::EventType::PAYMENT_REQUEST,
      base::Bind(&DispatchPaymentRequestEventError));

  active_version->event_dispatcher()->DispatchPaymentRequestEvent(
      std::move(app_request),
      base::Bind(&DidDispatchPaymentRequestEvent, active_version, request_id));
}

void DidFindRegistrationOnIO(
    payments::mojom::PaymentAppRequestPtr app_request,
    ServiceWorkerStatusCode service_worker_status,
    scoped_refptr<ServiceWorkerRegistration> service_worker_registration) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (service_worker_status != SERVICE_WORKER_OK)
    return;

  ServiceWorkerVersion* active_version =
      service_worker_registration->active_version();
  DCHECK(active_version);
  active_version->RunAfterStartWorker(
      ServiceWorkerMetrics::EventType::PAYMENT_REQUEST,
      base::Bind(&DispatchPaymentRequestEvent,
                 base::Passed(std::move(app_request)),
                 make_scoped_refptr(active_version)),
      base::Bind(&DispatchPaymentRequestEventError));
}

void FindRegistrationOnIO(
    scoped_refptr<ServiceWorkerContextWrapper> service_worker_context,
    int64_t registration_id,
    payments::mojom::PaymentAppRequestPtr app_request) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  service_worker_context->FindReadyRegistrationForIdOnly(
      registration_id, base::Bind(&DidFindRegistrationOnIO,
                                  base::Passed(std::move(app_request))));
}

}  // namespace

// static
PaymentAppProvider* PaymentAppProvider::GetInstance() {
  return PaymentAppProviderImpl::GetInstance();
}

// static
PaymentAppProviderImpl* PaymentAppProviderImpl::GetInstance() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return base::Singleton<PaymentAppProviderImpl>::get();
}

void PaymentAppProviderImpl::GetAllManifests(
    BrowserContext* browser_context,
    const GetAllManifestsCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  StoragePartitionImpl* partition = static_cast<StoragePartitionImpl*>(
      BrowserContext::GetDefaultStoragePartition(browser_context));
  scoped_refptr<PaymentAppContextImpl> payment_app_context =
      partition->GetPaymentAppContext();

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&GetAllManifestsOnIO, payment_app_context, callback));
}

void PaymentAppProviderImpl::InvokePaymentApp(
    BrowserContext* browser_context,
    int64_t registration_id,
    payments::mojom::PaymentAppRequestPtr app_request) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  StoragePartitionImpl* partition = static_cast<StoragePartitionImpl*>(
      BrowserContext::GetDefaultStoragePartition(browser_context));
  scoped_refptr<ServiceWorkerContextWrapper> service_worker_context =
      partition->GetServiceWorkerContext();

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&FindRegistrationOnIO, std::move(service_worker_context),
                 registration_id, base::Passed(std::move(app_request))));
}

PaymentAppProviderImpl::PaymentAppProviderImpl() {}

PaymentAppProviderImpl::~PaymentAppProviderImpl() {}

}  // namespace content
