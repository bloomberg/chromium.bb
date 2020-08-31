// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/payments/payment_app_provider_impl.h"

#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "base/base64.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/supports_user_data.h"
#include "base/task/post_task.h"
#include "base/token.h"
#include "components/payments/core/native_error_strings.h"
#include "components/payments/core/payments_validators.h"
#include "content/browser/devtools/devtools_background_services_context_impl.h"
#include "content/browser/payments/payment_app_context_impl.h"
#include "content/browser/payments/payment_app_installer.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_metrics.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/browser/storage_partition_impl.h"
#include "content/common/service_worker/service_worker_utils.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/devtools_background_services_context.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/permission_type.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/mojom/base/time.mojom.h"
#include "third_party/blink/public/common/service_worker/service_worker_status_code.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_container_type.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image.h"

namespace content {
namespace {

using payments::mojom::CanMakePaymentEventDataPtr;
using payments::mojom::CanMakePaymentEventResponseType;
using payments::mojom::CanMakePaymentResponse;
using payments::mojom::CanMakePaymentResponsePtr;
using payments::mojom::PaymentDetailsModifierPtr;
using payments::mojom::PaymentEventResponseType;
using payments::mojom::PaymentHandlerResponse;
using payments::mojom::PaymentHandlerResponseCallback;
using payments::mojom::PaymentHandlerResponsePtr;
using payments::mojom::PaymentMethodDataPtr;
using payments::mojom::PaymentRequestEventDataPtr;

using ServiceWorkerStartCallback =
    base::OnceCallback<void(scoped_refptr<ServiceWorkerVersion>,
                            blink::ServiceWorkerStatusCode)>;

CanMakePaymentResponsePtr CreateBlankCanMakePaymentResponse(
    CanMakePaymentEventResponseType response_type) {
  return CanMakePaymentResponse::New(response_type, /*can_make_payment=*/false,
                                     /*ready_for_minimal_ui=*/false,
                                     /*account_balance=*/base::nullopt);
}

PaymentHandlerResponsePtr CreateBlankPaymentHandlerResponse(
    PaymentEventResponseType response_type) {
  return PaymentHandlerResponse::New(
      "" /*=method_name*/, "" /*=stringified_details*/, response_type,
      base::nullopt /*=payer_name*/, base::nullopt /*=payer_email*/,
      base::nullopt /*=payer_phone*/, nullptr /*=shipping_address*/,
      base::nullopt /*=shipping_option*/);
}

class InvokeRespondWithCallback;

// A repository to store invoking payment app callback. It is used to abort
// payment when the opened payment handler window is closed before payment
// response is received or timeout.
// Note that there is only one opened payment handler window per browser
// context.
class InvokePaymentAppCallbackRepository {
 public:
  static InvokePaymentAppCallbackRepository* GetInstance() {
    DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
    return base::Singleton<InvokePaymentAppCallbackRepository>::get();
  }

  // Disallow copy and assign.
  InvokePaymentAppCallbackRepository(
      const InvokePaymentAppCallbackRepository& other) = delete;
  InvokePaymentAppCallbackRepository& operator=(
      const InvokePaymentAppCallbackRepository& other) = delete;

  InvokeRespondWithCallback* GetCallback(BrowserContext* browser_context) {
    DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
    auto it = invoke_callbacks_.find(browser_context);
    if (it != invoke_callbacks_.end()) {
      return it->second;
    }
    return nullptr;
  }

  void SetCallback(BrowserContext* browser_context,
                   InvokeRespondWithCallback* callback) {
    DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
    invoke_callbacks_[browser_context] = callback;
  }

  void RemoveCallback(BrowserContext* browser_context) {
    DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
    invoke_callbacks_.erase(browser_context);
  }

 private:
  InvokePaymentAppCallbackRepository() = default;
  ~InvokePaymentAppCallbackRepository() = default;

  friend struct base::DefaultSingletonTraits<
      InvokePaymentAppCallbackRepository>;

  std::map<BrowserContext*, InvokeRespondWithCallback*> invoke_callbacks_;
};

// Abstract base class for event callbacks that are invoked when the payment
// handler resolves the promise passed in to TheEvent.respondWith() method.
class RespondWithCallback : public PaymentHandlerResponseCallback {
 public:
  // Disallow copy and assign.
  RespondWithCallback(const RespondWithCallback& other) = delete;
  RespondWithCallback& operator=(const RespondWithCallback& other) = delete;

  mojo::PendingRemote<PaymentHandlerResponseCallback>
  BindNewPipeAndPassRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

 protected:
  RespondWithCallback(
      BrowserContext* browser_context,
      ServiceWorkerMetrics::EventType event_type,
      scoped_refptr<ServiceWorkerVersion> service_worker_version)
      : browser_context_(browser_context),
        service_worker_version_(service_worker_version) {
    request_id_ = service_worker_version->StartRequest(
        event_type, base::BindOnce(&RespondWithCallback::OnServiceWorkerError,
                                   weak_ptr_factory_.GetWeakPtr()));
  }

  ~RespondWithCallback() override = default;

  // PaymentHandlerResponseCallback implementation.
  void OnResponseForCanMakePayment(
      CanMakePaymentResponsePtr response) override {}

  // PaymentHandlerResponseCallback implementation.
  void OnResponseForPaymentRequest(
      PaymentHandlerResponsePtr response) override {}

  // PaymentHandlerResponseCallback implementation.
  void OnResponseForAbortPayment(bool payment_aborted) override {}

  virtual void OnServiceWorkerError(
      blink::ServiceWorkerStatusCode service_worker_status) = 0;

  void FinishServiceWorkerRequest() {
    service_worker_version_->FinishRequest(request_id_, false);
  }

  void MaybeRecordTimeoutMetric(blink::ServiceWorkerStatusCode status) {
    if (status == blink::ServiceWorkerStatusCode::kErrorTimeout) {
      UMA_HISTOGRAM_BOOLEAN("PaymentRequest.ServiceWorkerStatusCodeTimeout",
                            true);
    }
  }

  void ClearCallbackRepositoryAndCloseWindow() {
    DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());

    InvokePaymentAppCallbackRepository::GetInstance()->RemoveCallback(
        browser_context_);
    base::PostTask(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(&CloseClientWindowOnUIThread, browser_context_));
  }

 private:
  static void CloseClientWindowOnUIThread(BrowserContext* browser_context) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    PaymentAppProvider::GetInstance()->CloseOpenedWindow(browser_context);
  }

  int request_id_;
  BrowserContext* browser_context_;
  scoped_refptr<ServiceWorkerVersion> service_worker_version_;
  mojo::Receiver<PaymentHandlerResponseCallback> receiver_{this};

  base::WeakPtrFactory<RespondWithCallback> weak_ptr_factory_{this};
};

// Self-deleting callback for "canmakepayment" event. Invoked when the payment
// handler resolves the promise passed into CanMakePaymentEvent.respondWith()
// method.
class CanMakePaymentRespondWithCallback : public RespondWithCallback {
 public:
  CanMakePaymentRespondWithCallback(
      BrowserContext* browser_context,
      scoped_refptr<ServiceWorkerVersion> service_worker_version,
      PaymentAppProvider::CanMakePaymentCallback callback)
      : RespondWithCallback(browser_context,
                            ServiceWorkerMetrics::EventType::CAN_MAKE_PAYMENT,
                            service_worker_version),
        callback_(std::move(callback)) {}

  // Disallow copy and assign.
  CanMakePaymentRespondWithCallback(
      const CanMakePaymentRespondWithCallback& other) = delete;
  CanMakePaymentRespondWithCallback& operator=(
      const CanMakePaymentRespondWithCallback& other) = delete;

 private:
  ~CanMakePaymentRespondWithCallback() override = default;

  // PaymentHandlerResponseCallback implementation.
  void OnResponseForCanMakePayment(
      CanMakePaymentResponsePtr response) override {
    DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
    FinishServiceWorkerRequest();
    RunOrPostTaskOnThread(
        FROM_HERE, BrowserThread::UI,
        base::BindOnce(std::move(callback_), std::move(response)));
    delete this;
  }

  // RespondWithCallback implementation.
  void OnServiceWorkerError(
      blink::ServiceWorkerStatusCode service_worker_status) override {
    DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
    DCHECK_NE(service_worker_status, blink::ServiceWorkerStatusCode::kOk);
    MaybeRecordTimeoutMetric(service_worker_status);

    CanMakePaymentEventResponseType response_type =
        CanMakePaymentEventResponseType::BROWSER_ERROR;
    if (service_worker_status ==
        blink::ServiceWorkerStatusCode::kErrorEventWaitUntilRejected) {
      response_type = CanMakePaymentEventResponseType::REJECT;
    } else if (service_worker_status ==
               blink::ServiceWorkerStatusCode::kErrorTimeout) {
      response_type = CanMakePaymentEventResponseType::TIMEOUT;
    }

    RunOrPostTaskOnThread(
        FROM_HERE, BrowserThread::UI,
        base::BindOnce(std::move(callback_),
                       CreateBlankCanMakePaymentResponse(response_type)));
    delete this;
  }

  PaymentAppProvider::CanMakePaymentCallback callback_;
};

// Self-deleting callback for "paymentrequest" event. Invoked when the payment
// handler resolves the promise passed into PaymentRequestEvent.respondWith()
// method.
class InvokeRespondWithCallback : public RespondWithCallback {
 public:
  InvokeRespondWithCallback(
      BrowserContext* browser_context,
      scoped_refptr<ServiceWorkerVersion> service_worker_version,
      PaymentAppProvider::InvokePaymentAppCallback callback)
      : RespondWithCallback(browser_context,
                            ServiceWorkerMetrics::EventType::PAYMENT_REQUEST,
                            service_worker_version),
        callback_(std::move(callback)) {
    InvokePaymentAppCallbackRepository::GetInstance()->SetCallback(
        browser_context, this);
  }

  // Disallow copy and assign.
  InvokeRespondWithCallback(const InvokeRespondWithCallback& other) = delete;
  InvokeRespondWithCallback& operator=(const InvokeRespondWithCallback& other) =
      delete;

  // Called only for "paymentrequest" event.
  void AbortPaymentSinceOpennedWindowClosing(PaymentEventResponseType reason) {
    DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());

    FinishServiceWorkerRequest();
    RespondToPaymentRequestWithErrorAndDeleteSelf(reason);
  }

 private:
  ~InvokeRespondWithCallback() override = default;

  // PaymentHandlerResponseCallback implementation.
  void OnResponseForPaymentRequest(
      PaymentHandlerResponsePtr response) override {
    DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
    FinishServiceWorkerRequest();
    RunOrPostTaskOnThread(
        FROM_HERE, BrowserThread::UI,
        base::BindOnce(std::move(callback_), std::move(response)));

    ClearCallbackRepositoryAndCloseWindow();
    delete this;
  }

  // RespondWithCallback implementation.
  void OnServiceWorkerError(
      blink::ServiceWorkerStatusCode service_worker_status) override {
    DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
    DCHECK_NE(service_worker_status, blink::ServiceWorkerStatusCode::kOk);
    MaybeRecordTimeoutMetric(service_worker_status);

    PaymentEventResponseType response_type =
        PaymentEventResponseType::PAYMENT_EVENT_BROWSER_ERROR;
    if (service_worker_status ==
        blink::ServiceWorkerStatusCode::kErrorEventWaitUntilRejected) {
      response_type = PaymentEventResponseType::PAYMENT_EVENT_REJECT;
    } else if (service_worker_status ==
               blink::ServiceWorkerStatusCode::kErrorTimeout) {
      response_type = PaymentEventResponseType::PAYMENT_EVENT_TIMEOUT;
    }

    RespondToPaymentRequestWithErrorAndDeleteSelf(response_type);
  }

  void RespondToPaymentRequestWithErrorAndDeleteSelf(
      PaymentEventResponseType response_type) {
    DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
    RunOrPostTaskOnThread(
        FROM_HERE, BrowserThread::UI,
        base::BindOnce(std::move(callback_),
                       CreateBlankPaymentHandlerResponse(response_type)));
    ClearCallbackRepositoryAndCloseWindow();
    delete this;
  }

  PaymentAppProvider::InvokePaymentAppCallback callback_;
};

// Self-deleting callback for "abortpayment" event. Invoked when the payment
// handler resolves the promise passed into AbortPayment.respondWith() method.
class AbortRespondWithCallback : public RespondWithCallback {
 public:
  AbortRespondWithCallback(
      BrowserContext* browser_context,
      scoped_refptr<ServiceWorkerVersion> service_worker_version,
      PaymentAppProvider::AbortCallback callback)
      : RespondWithCallback(browser_context,
                            ServiceWorkerMetrics::EventType::ABORT_PAYMENT,
                            service_worker_version),
        callback_(std::move(callback)) {}

  // Disallow copy and assign.
  AbortRespondWithCallback(const AbortRespondWithCallback& other) = delete;
  AbortRespondWithCallback& operator=(const AbortRespondWithCallback& other) =
      delete;

 private:
  ~AbortRespondWithCallback() override = default;

  // PaymentHandlerResponseCallback implementation.
  void OnResponseForAbortPayment(bool payment_aborted) override {
    DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
    FinishServiceWorkerRequest();
    RunOrPostTaskOnThread(
        FROM_HERE, BrowserThread::UI,
        base::BindOnce(std::move(callback_), payment_aborted));

    if (payment_aborted)
      ClearCallbackRepositoryAndCloseWindow();

    delete this;
  }

  // RespondWithCallback implementation.
  void OnServiceWorkerError(
      blink::ServiceWorkerStatusCode service_worker_status) override {
    DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
    DCHECK_NE(service_worker_status, blink::ServiceWorkerStatusCode::kOk);
    MaybeRecordTimeoutMetric(service_worker_status);
    RunOrPostTaskOnThread(
        FROM_HERE, BrowserThread::UI,
        base::BindOnce(std::move(callback_), /*payment_aborted=*/false));
    // Do not call ClearCallbackRepositoryAndCloseWindow() here, because payment
    // has not been aborted. The service worker either rejected, timed out, or
    // threw a JavaScript exception in the "abortpayment" event, but that does
    // not affect the ongoing "paymentrequest" event.
    delete this;
  }

  PaymentAppProvider::AbortCallback callback_;
};

void DidGetAllPaymentAppsOnCoreThread(
    PaymentAppProvider::GetAllPaymentAppsCallback callback,
    PaymentAppProvider::PaymentApps apps) {
  base::PostTask(FROM_HERE, {BrowserThread::UI},
                 base::BindOnce(std::move(callback), std::move(apps)));
}

void GetAllPaymentAppsOnCoreThread(
    scoped_refptr<PaymentAppContextImpl> payment_app_context,
    PaymentAppProvider::GetAllPaymentAppsCallback callback) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());

  payment_app_context->payment_app_database()->ReadAllPaymentApps(
      base::BindOnce(&DidGetAllPaymentAppsOnCoreThread, std::move(callback)));
}

void DispatchAbortPaymentEvent(
    BrowserContext* browser_context,
    PaymentAppProvider::AbortCallback callback,
    scoped_refptr<ServiceWorkerVersion> active_version,
    blink::ServiceWorkerStatusCode service_worker_status) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());

  if (service_worker_status != blink::ServiceWorkerStatusCode::kOk) {
    base::PostTask(FROM_HERE, {BrowserThread::UI},
                   base::BindOnce(std::move(callback), false));
    return;
  }

  DCHECK(active_version);

  int event_finish_id = active_version->StartRequest(
      ServiceWorkerMetrics::EventType::CAN_MAKE_PAYMENT, base::DoNothing());

  // This object self-deletes after either success or error callback is
  // invoked.
  RespondWithCallback* respond_with_callback = new AbortRespondWithCallback(
      browser_context, active_version, std::move(callback));

  active_version->endpoint()->DispatchAbortPaymentEvent(
      respond_with_callback->BindNewPipeAndPassRemote(),
      active_version->CreateSimpleEventCallback(event_finish_id));
}

void DispatchCanMakePaymentEvent(
    BrowserContext* browser_context,
    CanMakePaymentEventDataPtr event_data,
    PaymentAppProvider::CanMakePaymentCallback callback,
    scoped_refptr<ServiceWorkerVersion> active_version,
    blink::ServiceWorkerStatusCode service_worker_status) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());

  if (service_worker_status != blink::ServiceWorkerStatusCode::kOk) {
    base::PostTask(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(std::move(callback),
                       CreateBlankCanMakePaymentResponse(
                           CanMakePaymentEventResponseType::BROWSER_ERROR)));
    return;
  }

  DCHECK(active_version);

  int event_finish_id = active_version->StartRequest(
      ServiceWorkerMetrics::EventType::CAN_MAKE_PAYMENT, base::DoNothing());

  // This object self-deletes after either success or error callback is
  // invoked.
  RespondWithCallback* respond_with_callback =
      new CanMakePaymentRespondWithCallback(browser_context, active_version,
                                            std::move(callback));

  active_version->endpoint()->DispatchCanMakePaymentEvent(
      std::move(event_data), respond_with_callback->BindNewPipeAndPassRemote(),
      active_version->CreateSimpleEventCallback(event_finish_id));
}

void DispatchPaymentRequestEvent(
    BrowserContext* browser_context,
    PaymentRequestEventDataPtr event_data,
    PaymentAppProvider::InvokePaymentAppCallback callback,
    scoped_refptr<ServiceWorkerVersion> active_version,
    blink::ServiceWorkerStatusCode service_worker_status) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());

  if (service_worker_status != blink::ServiceWorkerStatusCode::kOk) {
    base::PostTask(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(
            std::move(callback),
            CreateBlankPaymentHandlerResponse(
                PaymentEventResponseType::PAYMENT_EVENT_BROWSER_ERROR)));
    return;
  }

  DCHECK(active_version);

  int event_finish_id = active_version->StartRequest(
      ServiceWorkerMetrics::EventType::PAYMENT_REQUEST, base::DoNothing());

  // This object self-deletes after either success or error callback is
  // invoked.
  RespondWithCallback* respond_with_callback = new InvokeRespondWithCallback(
      browser_context, active_version, std::move(callback));

  active_version->endpoint()->DispatchPaymentRequestEvent(
      std::move(event_data), respond_with_callback->BindNewPipeAndPassRemote(),
      active_version->CreateSimpleEventCallback(event_finish_id));
}

void DidFindRegistrationOnCoreThread(
    ServiceWorkerStartCallback callback,
    blink::ServiceWorkerStatusCode service_worker_status,
    scoped_refptr<ServiceWorkerRegistration> service_worker_registration) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());

  if (service_worker_status != blink::ServiceWorkerStatusCode::kOk) {
    std::move(callback).Run(nullptr, service_worker_status);
    return;
  }

  ServiceWorkerVersion* active_version =
      service_worker_registration->active_version();
  DCHECK(active_version);
  active_version->RunAfterStartWorker(
      ServiceWorkerMetrics::EventType::PAYMENT_REQUEST,
      base::BindOnce(std::move(callback),
                     base::WrapRefCounted(active_version)));
}

void FindRegistrationOnCoreThread(
    scoped_refptr<ServiceWorkerContextWrapper> service_worker_context,
    int64_t registration_id,
    ServiceWorkerStartCallback callback) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());

  service_worker_context->FindReadyRegistrationForIdOnly(
      registration_id,
      base::BindOnce(&DidFindRegistrationOnCoreThread, std::move(callback)));
}

void StartServiceWorkerForDispatch(BrowserContext* browser_context,
                                   int64_t registration_id,
                                   ServiceWorkerStartCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  StoragePartitionImpl* partition = static_cast<StoragePartitionImpl*>(
      BrowserContext::GetDefaultStoragePartition(browser_context));
  scoped_refptr<ServiceWorkerContextWrapper> service_worker_context =
      partition->GetServiceWorkerContext();

  RunOrPostTaskOnThread(FROM_HERE, ServiceWorkerContext::GetCoreThreadId(),
                        base::BindOnce(&FindRegistrationOnCoreThread,
                                       std::move(service_worker_context),
                                       registration_id, std::move(callback)));
}

void OnInstallPaymentApp(
    const url::Origin& sw_origin,
    PaymentRequestEventDataPtr event_data,
    PaymentAppProvider::RegistrationIdCallback registration_id_callback,
    PaymentAppProvider::InvokePaymentAppCallback callback,
    BrowserContext* browser_context,
    int64_t registration_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (registration_id >= 0 && browser_context != nullptr) {
    std::move(registration_id_callback).Run(registration_id);
    PaymentAppProvider::GetInstance()->InvokePaymentApp(
        browser_context, registration_id, sw_origin, std::move(event_data),
        std::move(callback));
  } else {
    std::move(callback).Run(CreateBlankPaymentHandlerResponse(
        PaymentEventResponseType::PAYMENT_EVENT_BROWSER_ERROR));
  }
}

// Callbacks for checking permissions asynchronously. Owned by the browser
// context to avoid using the browser context after it has been freed. Deleted
// after the callback is invoked.
// Sample usage:
//   PostTask(&PermissionChecker::CheckPermissionForPaymentApps,
//       PermissionChecker::Create(browser_context), std::move(callback));
class PermissionChecker : public base::SupportsUserData::Data {
 public:
  static base::WeakPtr<PermissionChecker> Create(
      BrowserContext* browser_context) {
    auto owned = std::make_unique<PermissionChecker>(browser_context);
    auto weak_pointer_result = owned->weak_ptr_factory_.GetWeakPtr();
    void* key = owned.get();
    browser_context->SetUserData(key, std::move(owned));
    return weak_pointer_result;
  }

  // Do not use this method directly! Use the static PermissionChecker::Create()
  // method instead. (The constructor must be public for std::make_unique<> in
  // the Create() method.)
  explicit PermissionChecker(BrowserContext* browser_context)
      : browser_context_(browser_context) {}
  ~PermissionChecker() override = default;

  // Disallow copy and assign.
  PermissionChecker(const PermissionChecker& other) = delete;
  PermissionChecker& operator=(const PermissionChecker& other) = delete;

  void CheckPermissionForPaymentApps(
      PaymentAppProvider::GetAllPaymentAppsCallback callback,
      PaymentAppProvider::PaymentApps apps) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    PermissionController* permission_controller =
        BrowserContext::GetPermissionController(browser_context_);
    DCHECK(permission_controller);

    PaymentAppProvider::PaymentApps permitted_apps;
    for (auto& app : apps) {
      GURL origin = app.second->scope.GetOrigin();
      if (permission_controller->GetPermissionStatus(
              PermissionType::PAYMENT_HANDLER, origin, origin) ==
          blink::mojom::PermissionStatus::GRANTED) {
        permitted_apps[app.first] = std::move(app.second);
      }
    }

    std::move(callback).Run(std::move(permitted_apps));

    // Deletes this PermissionChecker object.
    browser_context_->RemoveUserData(/*key=*/this);
  }

 private:
  // Owns this PermissionChecker object, so it's always valid.
  BrowserContext* browser_context_;

  base::WeakPtrFactory<PermissionChecker> weak_ptr_factory_{this};
};

void AbortInvokePaymentApp(BrowserContext* browser_context,
                           PaymentEventResponseType reason) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());

  InvokeRespondWithCallback* callback =
      InvokePaymentAppCallbackRepository::GetInstance()->GetCallback(
          browser_context);
  if (callback)
    callback->AbortPaymentSinceOpennedWindowClosing(reason);
}

void AddMethodDataToMap(const std::vector<PaymentMethodDataPtr>& method_data,
                        std::map<std::string, std::string>* out) {
  for (size_t i = 0; i < method_data.size(); ++i) {
    std::string counter =
        method_data.size() == 1 ? "" : " #" + base::NumberToString(i);
    out->emplace("Method Name" + counter, method_data[i]->supported_method);
    out->emplace("Method Data" + counter, method_data[i]->stringified_data);
  }
}

void AddModifiersToMap(const std::vector<PaymentDetailsModifierPtr>& modifiers,
                       std::map<std::string, std::string>* out) {
  for (size_t i = 0; i < modifiers.size(); ++i) {
    std::string prefix =
        "Modifier" +
        (modifiers.size() == 1 ? "" : " #" + base::NumberToString(i));
    out->emplace(prefix + " Method Name",
                 modifiers[i]->method_data->supported_method);
    out->emplace(prefix + " Method Data",
                 modifiers[i]->method_data->stringified_data);
    if (!modifiers[i]->total)
      continue;
    out->emplace(prefix + " Total Currency",
                 modifiers[i]->total->amount->currency);
    out->emplace(prefix + " Total Value", modifiers[i]->total->amount->value);
  }
}

scoped_refptr<DevToolsBackgroundServicesContextImpl> GetDevTools(
    BrowserContext* browser_context,
    const url::Origin& sw_origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(browser_context);
  auto* storage_partition = BrowserContext::GetStoragePartitionForSite(
      browser_context, sw_origin.GetURL(), /*can_create=*/true);
  if (!storage_partition)
    return nullptr;

  scoped_refptr<DevToolsBackgroundServicesContextImpl> dev_tools =
      static_cast<DevToolsBackgroundServicesContextImpl*>(
          storage_partition->GetDevToolsBackgroundServicesContext());
  return dev_tools && dev_tools->IsRecording(
                          DevToolsBackgroundService::kPaymentHandler)
             ? dev_tools
             : nullptr;
}

void OnResponseForCanMakePaymentOnUiThread(
    scoped_refptr<DevToolsBackgroundServicesContextImpl> dev_tools,
    int64_t registration_id,
    const url::Origin& sw_origin,
    const std::string& payment_request_id,
    PaymentAppProvider::CanMakePaymentCallback callback,
    CanMakePaymentResponsePtr response) {
  std::string error_message;
  if (response->account_balance && !response->account_balance->empty() &&
      !payments::PaymentsValidators::IsValidAmountFormat(
          *response->account_balance, &error_message)) {
    mojo::ReportBadMessage(
        payments::errors::kCanMakePaymentEventInvalidAccountBalanceValue);
    response = CreateBlankCanMakePaymentResponse(
        CanMakePaymentEventResponseType::INVALID_ACCOUNT_BALANCE_VALUE);
  }

  if (dev_tools) {
    std::stringstream response_type;
    response_type << response->response_type;
    std::map<std::string, std::string> data = {
        {"Type", response_type.str()},
        {"Can Make Payment", response->can_make_payment ? "true" : "false"}};
    if (base::FeatureList::IsEnabled(features::kWebPaymentsMinimalUI)) {
      data["Ready for Minimal UI"] =
          response->ready_for_minimal_ui ? "true" : "false";
      data["Account Balance"] =
          response->account_balance ? *response->account_balance : "";
    }
    dev_tools->LogBackgroundServiceEvent(
        registration_id, sw_origin, DevToolsBackgroundService::kPaymentHandler,
        "Can make payment response",
        /*instance_id=*/payment_request_id, data);
  }

  std::move(callback).Run(std::move(response));
}

void OnResponseForAbortPaymentOnUiThread(
    scoped_refptr<DevToolsBackgroundServicesContextImpl> dev_tools,
    int64_t registration_id,
    const url::Origin& sw_origin,
    const std::string& payment_request_id,
    PaymentAppProvider::AbortCallback callback,
    bool payment_aborted) {
  if (dev_tools) {
    dev_tools->LogBackgroundServiceEvent(
        registration_id, sw_origin, DevToolsBackgroundService::kPaymentHandler,
        "Abort payment response",
        /*instance_id=*/payment_request_id,
        {{"Payment Aborted", payment_aborted ? "true" : "false"}});
  }

  std::move(callback).Run(payment_aborted);
}

void OnResponseForPaymentRequestOnUiThread(
    scoped_refptr<DevToolsBackgroundServicesContextImpl> dev_tools,
    int64_t registration_id,
    const url::Origin& sw_origin,
    const std::string& payment_request_id,
    PaymentAppProvider::InvokePaymentAppCallback callback,
    PaymentHandlerResponsePtr response) {
  if (dev_tools) {
    std::stringstream response_type;
    response_type << response->response_type;
    dev_tools->LogBackgroundServiceEvent(
        registration_id, sw_origin, DevToolsBackgroundService::kPaymentHandler,
        "Payment response",
        /*instance_id=*/payment_request_id,
        {{"Method Name", response->method_name},
         {"Details", response->stringified_details},
         {"Type", response_type.str()}});
  }

  std::move(callback).Run(std::move(response));
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

  RunOrPostTaskOnThread(
      FROM_HERE, ServiceWorkerContext::GetCoreThreadId(),
      base::BindOnce(
          &GetAllPaymentAppsOnCoreThread, payment_app_context,
          base::BindOnce(&PermissionChecker::CheckPermissionForPaymentApps,
                         PermissionChecker::Create(browser_context),
                         std::move(callback))));
}

void PaymentAppProviderImpl::InvokePaymentApp(
    BrowserContext* browser_context,
    int64_t registration_id,
    const url::Origin& sw_origin,
    PaymentRequestEventDataPtr event_data,
    InvokePaymentAppCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  scoped_refptr<DevToolsBackgroundServicesContextImpl> dev_tools =
      GetDevTools(browser_context, sw_origin);
  if (dev_tools) {
    std::map<std::string, std::string> data = {
        {"Merchant Top Origin", event_data->top_origin.spec()},
        {"Merchant Payment Request Origin",
         event_data->payment_request_origin.spec()},
        {"Total Currency", event_data->total->currency},
        {"Total Value", event_data->total->value},
        {"Instrument Key", event_data->instrument_key},
    };
    AddMethodDataToMap(event_data->method_data, &data);
    AddModifiersToMap(event_data->modifiers, &data);
    dev_tools->LogBackgroundServiceEvent(
        registration_id, sw_origin, DevToolsBackgroundService::kPaymentHandler,
        "Payment request",
        /*instance_id=*/event_data->payment_request_id, data);
  }

  StartServiceWorkerForDispatch(
      browser_context, registration_id,
      base::BindOnce(
          &DispatchPaymentRequestEvent, browser_context, std::move(event_data),
          base::BindOnce(&OnResponseForPaymentRequestOnUiThread, dev_tools,
                         registration_id, sw_origin,
                         event_data->payment_request_id, std::move(callback))));
}

void PaymentAppProviderImpl::InstallAndInvokePaymentApp(
    WebContents* web_contents,
    PaymentRequestEventDataPtr event_data,
    const std::string& app_name,
    const SkBitmap& app_icon,
    const GURL& sw_js_url,
    const GURL& sw_scope,
    bool sw_use_cache,
    const std::string& method,
    const SupportedDelegations& supported_delegations,
    RegistrationIdCallback registration_id_callback,
    InvokePaymentAppCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!sw_js_url.is_valid() || !sw_scope.is_valid() || method.empty()) {
    base::PostTask(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(
            std::move(callback),
            CreateBlankPaymentHandlerResponse(
                PaymentEventResponseType::PAYMENT_EVENT_BROWSER_ERROR)));
    return;
  }

  std::string string_encoded_icon;
  if (!app_icon.empty()) {
    gfx::Image decoded_image = gfx::Image::CreateFrom1xBitmap(app_icon);
    scoped_refptr<base::RefCountedMemory> raw_data =
        decoded_image.As1xPNGBytes();
    base::Base64Encode(
        base::StringPiece(raw_data->front_as<char>(), raw_data->size()),
        &string_encoded_icon);
  }

  PaymentAppInstaller::Install(
      web_contents, app_name, string_encoded_icon, sw_js_url, sw_scope,
      sw_use_cache, method, supported_delegations,
      base::BindOnce(&OnInstallPaymentApp, url::Origin::Create(sw_scope),
                     std::move(event_data), std::move(registration_id_callback),
                     std::move(callback)));
}

void PaymentAppProviderImpl::CanMakePayment(
    BrowserContext* browser_context,
    int64_t registration_id,
    const url::Origin& sw_origin,
    const std::string& payment_request_id,
    CanMakePaymentEventDataPtr event_data,
    CanMakePaymentCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  scoped_refptr<DevToolsBackgroundServicesContextImpl> dev_tools =
      GetDevTools(browser_context, sw_origin);
  if (dev_tools) {
    std::map<std::string, std::string> data = {
        {"Merchant Top Origin", event_data->top_origin.spec()},
        {"Merchant Payment Request Origin",
         event_data->payment_request_origin.spec()}};
    if (event_data->currency &&
        base::FeatureList::IsEnabled(features::kWebPaymentsMinimalUI)) {
      data["Currency"] = *event_data->currency;
    }
    AddMethodDataToMap(event_data->method_data, &data);
    AddModifiersToMap(event_data->modifiers, &data);
    dev_tools->LogBackgroundServiceEvent(
        registration_id, sw_origin, DevToolsBackgroundService::kPaymentHandler,
        "Can make payment",
        /*instance_id=*/payment_request_id, data);
  }

  StartServiceWorkerForDispatch(
      browser_context, registration_id,
      base::BindOnce(&DispatchCanMakePaymentEvent, browser_context,
                     std::move(event_data),
                     base::BindOnce(&OnResponseForCanMakePaymentOnUiThread,
                                    dev_tools, registration_id, sw_origin,
                                    payment_request_id, std::move(callback))));
}

void PaymentAppProviderImpl::AbortPayment(BrowserContext* browser_context,
                                          int64_t registration_id,
                                          const url::Origin& sw_origin,
                                          const std::string& payment_request_id,
                                          AbortCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  scoped_refptr<DevToolsBackgroundServicesContextImpl> dev_tools =
      GetDevTools(browser_context, sw_origin);
  if (dev_tools) {
    dev_tools->LogBackgroundServiceEvent(
        registration_id, sw_origin, DevToolsBackgroundService::kPaymentHandler,
        "Abort payment",
        /*instance_id=*/payment_request_id, {});
  }

  StartServiceWorkerForDispatch(
      browser_context, registration_id,
      base::BindOnce(&DispatchAbortPaymentEvent, browser_context,
                     base::BindOnce(&OnResponseForAbortPaymentOnUiThread,
                                    dev_tools, registration_id, sw_origin,
                                    payment_request_id, std::move(callback))));
}

void PaymentAppProviderImpl::SetOpenedWindow(WebContents* web_contents) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  CloseOpenedWindow(web_contents->GetBrowserContext());

  payment_handler_windows_[web_contents->GetBrowserContext()] =
      std::make_unique<PaymentHandlerWindowObserver>(web_contents);
}

void PaymentAppProviderImpl::CloseOpenedWindow(
    BrowserContext* browser_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  auto it = payment_handler_windows_.find(browser_context);
  if (it != payment_handler_windows_.end()) {
    if (it->second->web_contents() != nullptr) {
      it->second->web_contents()->Close();
    }
    payment_handler_windows_.erase(it);
  }
}

void PaymentAppProviderImpl::OnClosingOpenedWindow(
    BrowserContext* browser_context,
    PaymentEventResponseType reason) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  RunOrPostTaskOnThread(
      FROM_HERE, ServiceWorkerContext::GetCoreThreadId(),
      base::BindOnce(&AbortInvokePaymentApp, browser_context, reason));
}

bool PaymentAppProviderImpl::IsValidInstallablePaymentApp(
    const GURL& manifest_url,
    const GURL& sw_js_url,
    const GURL& sw_scope,
    std::string* error_message) {
  DCHECK(manifest_url.is_valid() && sw_js_url.is_valid() &&
         sw_scope.is_valid());

  // Scope will be checked against service worker js url when registering, but
  // we check it here earlier to avoid presenting unusable payment handlers.
  if (!ServiceWorkerUtils::IsPathRestrictionSatisfiedWithoutHeader(
          sw_scope, sw_js_url, error_message)) {
    return false;
  }

  // TODO(crbug.com/855312): Unify duplicated code between here and
  // ServiceWorkerProviderHost::IsValidRegisterMessage.
  std::vector<GURL> urls = {manifest_url, sw_js_url, sw_scope};
  if (!ServiceWorkerUtils::AllOriginsMatchAndCanAccessServiceWorkers(urls)) {
    *error_message =
        "Origins are not matching, or some origins cannot access service "
        "worker "
        "(manifest:" +
        manifest_url.spec() + " scope:" + sw_scope.spec() +
        " sw:" + sw_js_url.spec() + ")";
    return false;
  }

  return true;
}

ukm::SourceId PaymentAppProviderImpl::GetSourceIdForPaymentAppFromScope(
    const GURL& sw_scope) {
  return ukm::UkmRecorder::GetSourceIdForPaymentAppFromScope(
      sw_scope.GetOrigin());
}

PaymentAppProviderImpl::PaymentAppProviderImpl() = default;

PaymentAppProviderImpl::~PaymentAppProviderImpl() = default;

PaymentAppProviderImpl::PaymentHandlerWindowObserver::
    PaymentHandlerWindowObserver(WebContents* web_contents)
    : WebContentsObserver(web_contents) {}
PaymentAppProviderImpl::PaymentHandlerWindowObserver::
    ~PaymentHandlerWindowObserver() = default;

}  // namespace content
