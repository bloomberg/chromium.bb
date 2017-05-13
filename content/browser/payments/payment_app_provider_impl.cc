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
#include "content/common/service_worker/service_worker_utils.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/common/time.mojom.h"

namespace content {
namespace {

class ResponseCallback : public payments::mojom::PaymentAppResponseCallback {
 public:
  static payments::mojom::PaymentAppResponseCallbackPtr Create(
      int payment_request_id,
      scoped_refptr<ServiceWorkerVersion> service_worker_version,
      const PaymentAppProvider::InvokePaymentAppCallback callback) {
    ResponseCallback* response_callback = new ResponseCallback(
        payment_request_id, std::move(service_worker_version), callback);
    return response_callback->binding_.CreateInterfacePtrAndBind();
  }
  ~ResponseCallback() override {}

  void OnPaymentAppResponse(payments::mojom::PaymentAppResponsePtr response,
                            base::Time dispatch_event_time) override {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    service_worker_version_->FinishRequest(payment_request_id_, false,
                                           std::move(dispatch_event_time));
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(callback_, base::Passed(std::move(response))));
    delete this;
  }

 private:
  ResponseCallback(int payment_request_id,
                   scoped_refptr<ServiceWorkerVersion> service_worker_version,
                   const PaymentAppProvider::InvokePaymentAppCallback callback)
      : payment_request_id_(payment_request_id),
        service_worker_version_(service_worker_version),
        callback_(callback),
        binding_(this) {}

  int payment_request_id_;
  scoped_refptr<ServiceWorkerVersion> service_worker_version_;
  const PaymentAppProvider::InvokePaymentAppCallback callback_;
  mojo::Binding<payments::mojom::PaymentAppResponseCallback> binding_;
};

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

void DidGetAllPaymentAppsOnIO(
    PaymentAppProvider::GetAllPaymentAppsCallback callback,
    PaymentAppProvider::PaymentApps apps) {
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::BindOnce(std::move(callback), base::Passed(std::move(apps))));
}

void GetAllPaymentAppsOnIO(
    scoped_refptr<PaymentAppContextImpl> payment_app_context,
    PaymentAppProvider::GetAllPaymentAppsCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  payment_app_context->payment_app_database()->ReadAllPaymentApps(
      base::BindOnce(&DidGetAllPaymentAppsOnIO, std::move(callback)));
}

void DispatchPaymentRequestEventError(
    ServiceWorkerStatusCode service_worker_status) {
  NOTIMPLEMENTED();
}

void DispatchPaymentRequestEvent(
    payments::mojom::PaymentAppRequestPtr app_request,
    const PaymentAppProvider::InvokePaymentAppCallback& callback,
    scoped_refptr<ServiceWorkerVersion> active_version) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(active_version);

  int payment_request_id = active_version->StartRequest(
      ServiceWorkerMetrics::EventType::PAYMENT_REQUEST,
      base::Bind(&DispatchPaymentRequestEventError));
  int event_finish_id = active_version->StartRequest(
      ServiceWorkerMetrics::EventType::PAYMENT_REQUEST,
      base::Bind(&ServiceWorkerUtils::NoOpStatusCallback));

  payments::mojom::PaymentAppResponseCallbackPtr response_callback_ptr =
      ResponseCallback::Create(payment_request_id, active_version, callback);
  DCHECK(response_callback_ptr);
  active_version->event_dispatcher()->DispatchPaymentRequestEvent(
      payment_request_id, std::move(app_request),
      std::move(response_callback_ptr),
      active_version->CreateSimpleEventCallback(event_finish_id));
}

void DidFindRegistrationOnIO(
    payments::mojom::PaymentAppRequestPtr app_request,
    const PaymentAppProvider::InvokePaymentAppCallback& callback,
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
                 base::Passed(std::move(app_request)), callback,
                 make_scoped_refptr(active_version)),
      base::Bind(&DispatchPaymentRequestEventError));
}

void FindRegistrationOnIO(
    scoped_refptr<ServiceWorkerContextWrapper> service_worker_context,
    int64_t registration_id,
    payments::mojom::PaymentAppRequestPtr app_request,
    const PaymentAppProvider::InvokePaymentAppCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  service_worker_context->FindReadyRegistrationForIdOnly(
      registration_id,
      base::Bind(&DidFindRegistrationOnIO, base::Passed(std::move(app_request)),
                 callback));
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

void PaymentAppProviderImpl::GetAllPaymentApps(
    BrowserContext* browser_context,
    GetAllPaymentAppsCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  StoragePartitionImpl* partition = static_cast<StoragePartitionImpl*>(
      BrowserContext::GetDefaultStoragePartition(browser_context));
  scoped_refptr<PaymentAppContextImpl> payment_app_context =
      partition->GetPaymentAppContext();

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::BindOnce(&GetAllPaymentAppsOnIO, payment_app_context,
                     std::move(callback)));
}

void PaymentAppProviderImpl::InvokePaymentApp(
    BrowserContext* browser_context,
    int64_t registration_id,
    payments::mojom::PaymentAppRequestPtr app_request,
    const InvokePaymentAppCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  StoragePartitionImpl* partition = static_cast<StoragePartitionImpl*>(
      BrowserContext::GetDefaultStoragePartition(browser_context));
  scoped_refptr<ServiceWorkerContextWrapper> service_worker_context =
      partition->GetServiceWorkerContext();

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&FindRegistrationOnIO, std::move(service_worker_context),
                 registration_id, base::Passed(std::move(app_request)),
                 callback));
}

PaymentAppProviderImpl::PaymentAppProviderImpl() {}

PaymentAppProviderImpl::~PaymentAppProviderImpl() {}

}  // namespace content
