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
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "mojo/common/time.mojom.h"

namespace content {
namespace {

using ServiceWorkerStartCallback =
    base::OnceCallback<void(scoped_refptr<ServiceWorkerVersion>,
                            ServiceWorkerStatusCode)>;

// Note that one and only one of the callbacks from this class must/should be
// called.
class RespondWithCallbacks
    : public payments::mojom::PaymentHandlerResponseCallback {
 public:
  RespondWithCallbacks(
      ServiceWorkerMetrics::EventType event_type,
      scoped_refptr<ServiceWorkerVersion> service_worker_version,
      PaymentAppProvider::InvokePaymentAppCallback callback)
      : service_worker_version_(service_worker_version),
        invoke_payment_app_callback_(std::move(callback)),
        binding_(this),
        weak_ptr_factory_(this) {
    request_id_ = service_worker_version->StartRequest(
        event_type, base::BindOnce(&RespondWithCallbacks::OnErrorStatus,
                                   weak_ptr_factory_.GetWeakPtr()));
  }

  RespondWithCallbacks(
      ServiceWorkerMetrics::EventType event_type,
      scoped_refptr<ServiceWorkerVersion> service_worker_version,
      PaymentAppProvider::PaymentEventResultCallback callback)
      : service_worker_version_(service_worker_version),
        payment_event_result_callback_(std::move(callback)),
        binding_(this),
        weak_ptr_factory_(this) {
    request_id_ = service_worker_version->StartRequest(
        event_type, base::BindOnce(&RespondWithCallbacks::OnErrorStatus,
                                   weak_ptr_factory_.GetWeakPtr()));
  }

  payments::mojom::PaymentHandlerResponseCallbackPtr
  CreateInterfacePtrAndBind() {
    payments::mojom::PaymentHandlerResponseCallbackPtr callback_proxy;
    binding_.Bind(mojo::MakeRequest(&callback_proxy));
    return callback_proxy;
  }

  void OnResponseForPaymentRequest(
      payments::mojom::PaymentHandlerResponsePtr response,
      base::Time dispatch_event_time) override {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    service_worker_version_->FinishRequest(request_id_, false,
                                           std::move(dispatch_event_time));
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::BindOnce(std::move(invoke_payment_app_callback_),
                       std::move(response)));

    CloseClientWindows();
    delete this;
  }

  void OnResponseForCanMakePayment(bool can_make_payment,
                                   base::Time dispatch_event_time) override {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    service_worker_version_->FinishRequest(request_id_, false,
                                           std::move(dispatch_event_time));
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::BindOnce(std::move(payment_event_result_callback_),
                       can_make_payment));
    delete this;
  }

  void OnResponseForAbortPayment(bool payment_aborted,
                                 base::Time dispatch_event_time) override {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    service_worker_version_->FinishRequest(request_id_, false,
                                           std::move(dispatch_event_time));
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::BindOnce(std::move(payment_event_result_callback_),
                       payment_aborted));

    CloseClientWindows();
    delete this;
  }

  void OnErrorStatus(ServiceWorkerStatusCode service_worker_status) {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    DCHECK(service_worker_status != SERVICE_WORKER_OK);

    if (event_type_ == ServiceWorkerMetrics::EventType::PAYMENT_REQUEST) {
      BrowserThread::PostTask(
          BrowserThread::UI, FROM_HERE,
          base::BindOnce(std::move(invoke_payment_app_callback_),
                         payments::mojom::PaymentHandlerResponse::New()));
    } else if (event_type_ ==
                   ServiceWorkerMetrics::EventType::CAN_MAKE_PAYMENT ||
               event_type_ == ServiceWorkerMetrics::EventType::ABORT_PAYMENT) {
      BrowserThread::PostTask(
          BrowserThread::UI, FROM_HERE,
          base::BindOnce(std::move(payment_event_result_callback_), false));
    }

    if (event_type_ == ServiceWorkerMetrics::EventType::PAYMENT_REQUEST ||
        event_type_ == ServiceWorkerMetrics::EventType::ABORT_PAYMENT) {
      CloseClientWindows();
    }
    delete this;
  }

  int request_id() { return request_id_; }

 private:
  ~RespondWithCallbacks() override {}

  // Close all the windows in the payment handler service worker scope.
  // Note that this will close not only the windows opened through
  // PaymentRequestEvent.openWindow and Clients.openWindow(), but also the
  // windows opened through typing the url in the ominibox only if they are in
  // the payment handler service worker scope.
  void CloseClientWindows() {
    std::vector<std::pair<int, int>> ids;
    for (const auto& controllee : service_worker_version_->controllee_map()) {
      if (controllee.second->provider_type() ==
          SERVICE_WORKER_PROVIDER_FOR_WINDOW) {
        ids.emplace_back(std::make_pair(controllee.second->process_id(),
                                        controllee.second->frame_id()));
      }
    }
    if (ids.size() == 0)
      return;

    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::BindOnce(&RespondWithCallbacks::CloseClientWindowsOnUIThread,
                       ids));
  }

  static void CloseClientWindowsOnUIThread(
      const std::vector<std::pair<int, int>>& ids) {
    for (const auto& id : ids) {
      RenderFrameHost* frame_host =
          RenderFrameHost::FromID(id.first, id.second);
      if (frame_host == nullptr)
        continue;

      WebContents* web_contents = WebContents::FromRenderFrameHost(frame_host);
      if (web_contents == nullptr)
        continue;

      web_contents->Close();
    }
  }

  int request_id_;
  ServiceWorkerMetrics::EventType event_type_;
  scoped_refptr<ServiceWorkerVersion> service_worker_version_;
  PaymentAppProvider::InvokePaymentAppCallback invoke_payment_app_callback_;
  PaymentAppProvider::PaymentEventResultCallback payment_event_result_callback_;
  mojo::Binding<payments::mojom::PaymentHandlerResponseCallback> binding_;

  base::WeakPtrFactory<RespondWithCallbacks> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(RespondWithCallbacks);
};

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

void DispatchAbortPaymentEvent(
    PaymentAppProvider::PaymentEventResultCallback callback,
    scoped_refptr<ServiceWorkerVersion> active_version,
    ServiceWorkerStatusCode service_worker_status) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (service_worker_status != SERVICE_WORKER_OK) {
    std::move(callback).Run(false);
    return;
  }

  DCHECK(active_version);

  int event_finish_id = active_version->StartRequest(
      ServiceWorkerMetrics::EventType::CAN_MAKE_PAYMENT,
      base::BindOnce(&ServiceWorkerUtils::NoOpStatusCallback));

  // This object self-deletes after either success or error callback is invoked.
  RespondWithCallbacks* invocation_callbacks =
      new RespondWithCallbacks(ServiceWorkerMetrics::EventType::ABORT_PAYMENT,
                               active_version, std::move(callback));

  active_version->event_dispatcher()->DispatchAbortPaymentEvent(
      invocation_callbacks->request_id(),
      invocation_callbacks->CreateInterfacePtrAndBind(),
      active_version->CreateSimpleEventCallback(event_finish_id));
}

void DispatchCanMakePaymentEvent(
    payments::mojom::CanMakePaymentEventDataPtr event_data,
    PaymentAppProvider::PaymentEventResultCallback callback,
    scoped_refptr<ServiceWorkerVersion> active_version,
    ServiceWorkerStatusCode service_worker_status) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (service_worker_status != SERVICE_WORKER_OK) {
    std::move(callback).Run(false);
    return;
  }

  DCHECK(active_version);

  int event_finish_id = active_version->StartRequest(
      ServiceWorkerMetrics::EventType::CAN_MAKE_PAYMENT,
      base::BindOnce(&ServiceWorkerUtils::NoOpStatusCallback));

  // This object self-deletes after either success or error callback is invoked.
  RespondWithCallbacks* invocation_callbacks = new RespondWithCallbacks(
      ServiceWorkerMetrics::EventType::CAN_MAKE_PAYMENT, active_version,
      std::move(callback));

  active_version->event_dispatcher()->DispatchCanMakePaymentEvent(
      invocation_callbacks->request_id(), std::move(event_data),
      invocation_callbacks->CreateInterfacePtrAndBind(),
      active_version->CreateSimpleEventCallback(event_finish_id));
}

void DispatchPaymentRequestEvent(
    payments::mojom::PaymentRequestEventDataPtr event_data,
    PaymentAppProvider::InvokePaymentAppCallback callback,
    scoped_refptr<ServiceWorkerVersion> active_version,
    ServiceWorkerStatusCode service_worker_status) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (service_worker_status != SERVICE_WORKER_OK) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::BindOnce(std::move(callback),
                       payments::mojom::PaymentHandlerResponse::New()));
    return;
  }

  DCHECK(active_version);

  int event_finish_id = active_version->StartRequest(
      ServiceWorkerMetrics::EventType::PAYMENT_REQUEST,
      base::BindOnce(&ServiceWorkerUtils::NoOpStatusCallback));

  // This object self-deletes after either success or error callback is invoked.
  RespondWithCallbacks* invocation_callbacks =
      new RespondWithCallbacks(ServiceWorkerMetrics::EventType::PAYMENT_REQUEST,
                               active_version, std::move(callback));

  active_version->event_dispatcher()->DispatchPaymentRequestEvent(
      invocation_callbacks->request_id(), std::move(event_data),
      invocation_callbacks->CreateInterfacePtrAndBind(),
      active_version->CreateSimpleEventCallback(event_finish_id));
}

void DidFindRegistrationOnIO(
    ServiceWorkerStartCallback callback,
    ServiceWorkerStatusCode service_worker_status,
    scoped_refptr<ServiceWorkerRegistration> service_worker_registration) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (service_worker_status != SERVICE_WORKER_OK) {
    std::move(callback).Run(nullptr, service_worker_status);
    return;
  }

  ServiceWorkerVersion* active_version =
      service_worker_registration->active_version();
  DCHECK(active_version);

  auto done_callback = base::AdaptCallbackForRepeating(base::BindOnce(
      std::move(callback), base::WrapRefCounted(active_version)));

  active_version->RunAfterStartWorker(
      ServiceWorkerMetrics::EventType::PAYMENT_REQUEST,
      base::BindOnce(done_callback, service_worker_status), done_callback);
}

void FindRegistrationOnIO(
    scoped_refptr<ServiceWorkerContextWrapper> service_worker_context,
    int64_t registration_id,
    ServiceWorkerStartCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  service_worker_context->FindReadyRegistrationForIdOnly(
      registration_id,
      base::Bind(&DidFindRegistrationOnIO, base::Passed(std::move(callback))));
}

void StartServiceWorkerForDispatch(BrowserContext* browser_context,
                                   int64_t registration_id,
                                   ServiceWorkerStartCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  StoragePartitionImpl* partition = static_cast<StoragePartitionImpl*>(
      BrowserContext::GetDefaultStoragePartition(browser_context));
  scoped_refptr<ServiceWorkerContextWrapper> service_worker_context =
      partition->GetServiceWorkerContext();

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::BindOnce(&FindRegistrationOnIO, std::move(service_worker_context),
                     registration_id, std::move(callback)));
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
    payments::mojom::PaymentRequestEventDataPtr event_data,
    InvokePaymentAppCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  StartServiceWorkerForDispatch(
      browser_context, registration_id,
      base::BindOnce(&DispatchPaymentRequestEvent, std::move(event_data),
                     std::move(callback)));
}

void PaymentAppProviderImpl::CanMakePayment(
    BrowserContext* browser_context,
    int64_t registration_id,
    payments::mojom::CanMakePaymentEventDataPtr event_data,
    PaymentEventResultCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  StartServiceWorkerForDispatch(
      browser_context, registration_id,
      base::BindOnce(&DispatchCanMakePaymentEvent, std::move(event_data),
                     std::move(callback)));
}

void PaymentAppProviderImpl::AbortPayment(BrowserContext* browser_context,
                                          int64_t registration_id,
                                          PaymentEventResultCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  StartServiceWorkerForDispatch(
      browser_context, registration_id,
      base::BindOnce(&DispatchAbortPaymentEvent, std::move(callback)));
}

PaymentAppProviderImpl::PaymentAppProviderImpl() {}

PaymentAppProviderImpl::~PaymentAppProviderImpl() {}

}  // namespace content
