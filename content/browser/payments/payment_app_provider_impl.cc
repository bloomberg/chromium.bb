// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/payments/payment_app_provider_impl.h"

#include "base/base64.h"
#include "base/strings/string_util.h"
#include "content/browser/payments/payment_app_context_impl.h"
#include "content/browser/payments/payment_app_installer.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_metrics.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/browser/storage_partition_impl.h"
#include "content/common/service_worker/service_worker_status_code.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/permission_manager.h"
#include "content/public/browser/permission_type.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/mojom/base/time.mojom.h"
#include "third_party/WebKit/public/mojom/service_worker/service_worker_provider_type.mojom.h"
#include "third_party/WebKit/public/platform/modules/permissions/permission_status.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image.h"

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
      BrowserContext* browser_context,
      ServiceWorkerMetrics::EventType event_type,
      scoped_refptr<ServiceWorkerVersion> service_worker_version,
      PaymentAppProvider::InvokePaymentAppCallback callback)
      : browser_context_(browser_context),
        service_worker_version_(service_worker_version),
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

    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::BindOnce(&CloseClientWindowOnUIThread, browser_context_));
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

    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::BindOnce(&CloseClientWindowOnUIThread, browser_context_));
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
      BrowserThread::PostTask(
          BrowserThread::UI, FROM_HERE,
          base::BindOnce(&CloseClientWindowOnUIThread, browser_context_));
    }
    delete this;
  }

  int request_id() { return request_id_; }

 private:
  ~RespondWithCallbacks() override {}

  static void CloseClientWindowOnUIThread(BrowserContext* browser_context) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    PaymentAppProvider::GetInstance()->CloseOpenedWindow(browser_context);
  }

  int request_id_;
  BrowserContext* browser_context_;
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
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          base::BindOnce(std::move(callback), std::move(apps)));
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
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                            base::BindOnce(std::move(callback), false));
    return;
  }

  DCHECK(active_version);

  int event_finish_id = active_version->StartRequest(
      ServiceWorkerMetrics::EventType::CAN_MAKE_PAYMENT, base::DoNothing());

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
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                            base::BindOnce(std::move(callback), false));
    return;
  }

  DCHECK(active_version);

  int event_finish_id = active_version->StartRequest(
      ServiceWorkerMetrics::EventType::CAN_MAKE_PAYMENT, base::DoNothing());

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
    BrowserContext* browser_context,
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
      ServiceWorkerMetrics::EventType::PAYMENT_REQUEST, base::DoNothing());

  // This object self-deletes after either success or error callback is invoked.
  RespondWithCallbacks* invocation_callbacks = new RespondWithCallbacks(
      browser_context, ServiceWorkerMetrics::EventType::PAYMENT_REQUEST,
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
  active_version->RunAfterStartWorker(
      ServiceWorkerMetrics::EventType::PAYMENT_REQUEST,
      base::BindOnce(std::move(callback),
                     base::WrapRefCounted(active_version)));
}

void FindRegistrationOnIO(
    scoped_refptr<ServiceWorkerContextWrapper> service_worker_context,
    int64_t registration_id,
    ServiceWorkerStartCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  service_worker_context->FindReadyRegistrationForIdOnly(
      registration_id,
      base::BindOnce(&DidFindRegistrationOnIO, std::move(callback)));
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

void OnInstallPaymentApp(payments::mojom::PaymentRequestEventDataPtr event_data,
                         PaymentAppProvider::InvokePaymentAppCallback callback,
                         BrowserContext* browser_context,
                         long registration_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (registration_id >= 0 && browser_context != nullptr) {
    PaymentAppProvider::GetInstance()->InvokePaymentApp(
        browser_context, registration_id, std::move(event_data),
        std::move(callback));
  } else {
    std::move(callback).Run(payments::mojom::PaymentHandlerResponse::New());
  }
}

void CheckPermissionForPaymentApps(
    BrowserContext* browser_context,
    PaymentAppProvider::GetAllPaymentAppsCallback callback,
    PaymentAppProvider::PaymentApps apps) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  PermissionManager* permission_manager =
      browser_context->GetPermissionManager();
  if (!permission_manager) {
    std::move(callback).Run(PaymentAppProvider::PaymentApps());
    return;
  }

  PaymentAppProvider::PaymentApps permitted_apps;
  for (auto& app : apps) {
    GURL origin = app.second->scope.GetOrigin();
    if (permission_manager->GetPermissionStatus(PermissionType::PAYMENT_HANDLER,
                                                origin, origin) ==
        blink::mojom::PermissionStatus::GRANTED) {
      permitted_apps[app.first] = std::move(app.second);
    }
  }

  std::move(callback).Run(std::move(permitted_apps));
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
                     base::BindOnce(&CheckPermissionForPaymentApps,
                                    browser_context, std::move(callback))));
}

void PaymentAppProviderImpl::InvokePaymentApp(
    BrowserContext* browser_context,
    int64_t registration_id,
    payments::mojom::PaymentRequestEventDataPtr event_data,
    InvokePaymentAppCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  StartServiceWorkerForDispatch(
      browser_context, registration_id,
      base::BindOnce(&DispatchPaymentRequestEvent, browser_context,
                     std::move(event_data), std::move(callback)));
}

void PaymentAppProviderImpl::InstallAndInvokePaymentApp(
    WebContents* web_contents,
    payments::mojom::PaymentRequestEventDataPtr event_data,
    const std::string& app_name,
    const SkBitmap& app_icon,
    const std::string& sw_js_url,
    const std::string& sw_scope,
    bool sw_use_cache,
    const std::vector<std::string>& enabled_methods,
    InvokePaymentAppCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  DCHECK(base::IsStringUTF8(sw_js_url));
  GURL url = GURL(sw_js_url);
  DCHECK(base::IsStringUTF8(sw_scope));
  GURL scope = GURL(sw_scope);
  if (!url.is_valid() || !scope.is_valid() || enabled_methods.size() == 0) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::BindOnce(std::move(callback),
                       payments::mojom::PaymentHandlerResponse::New()));
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
      web_contents, app_name, string_encoded_icon, url, scope, sw_use_cache,
      enabled_methods,
      base::BindOnce(&OnInstallPaymentApp, std::move(event_data),
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

PaymentAppProviderImpl::PaymentAppProviderImpl() = default;

PaymentAppProviderImpl::~PaymentAppProviderImpl() = default;

PaymentAppProviderImpl::PaymentHandlerWindowObserver::
    PaymentHandlerWindowObserver(WebContents* web_contents)
    : WebContentsObserver(web_contents) {}
PaymentAppProviderImpl::PaymentHandlerWindowObserver::
    ~PaymentHandlerWindowObserver() = default;

}  // namespace content
