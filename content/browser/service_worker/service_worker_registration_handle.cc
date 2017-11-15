// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_registration_handle.h"

#include "content/browser/service_worker/service_worker_consts.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_dispatcher_host.h"
#include "content/browser/service_worker/service_worker_handle.h"
#include "content/browser/service_worker/service_worker_provider_host.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/service_worker/service_worker_messages.h"
#include "content/common/service_worker/service_worker_types.h"
#include "content/common/service_worker/service_worker_utils.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "content/public/common/service_worker_modes.h"
#include "net/http/http_util.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/service_worker_registration.mojom.h"

namespace content {

ServiceWorkerRegistrationHandle::ServiceWorkerRegistrationHandle(
    base::WeakPtr<ServiceWorkerContextCore> context,
    ServiceWorkerDispatcherHost* dispatcher_host,
    base::WeakPtr<ServiceWorkerProviderHost> provider_host,
    ServiceWorkerRegistration* registration)
    : dispatcher_host_(dispatcher_host),
      provider_host_(provider_host),
      context_(context),
      provider_id_(provider_host ? provider_host->provider_id()
                                 : kInvalidServiceWorkerProviderId),
      handle_id_(context
                     ? context->GetNewRegistrationHandleId()
                     : blink::mojom::kInvalidServiceWorkerRegistrationHandleId),
      registration_(registration),
      weak_ptr_factory_(this) {
  DCHECK(registration_.get());
  registration_->AddListener(this);
  bindings_.set_connection_error_handler(
      base::Bind(&ServiceWorkerRegistrationHandle::OnConnectionError,
                 base::Unretained(this)));

  dispatcher_host_->RegisterServiceWorkerRegistrationHandle(this);
}

ServiceWorkerRegistrationHandle::~ServiceWorkerRegistrationHandle() {
  DCHECK(registration_.get());
  registration_->RemoveListener(this);
}

blink::mojom::ServiceWorkerRegistrationObjectInfoPtr
ServiceWorkerRegistrationHandle::CreateObjectInfo() {
  DCHECK(provider_host_);
  auto info = blink::mojom::ServiceWorkerRegistrationObjectInfo::New();
  info->options = blink::mojom::ServiceWorkerRegistrationOptions::New(
      registration_->pattern());
  info->registration_id = registration_->id();
  bindings_.AddBinding(this, mojo::MakeRequest(&info->host_ptr_info));
  if (!remote_registration_)
    info->request = mojo::MakeRequest(&remote_registration_);

  info->installing = provider_host_->GetOrCreateServiceWorkerHandle(
      registration_->installing_version());
  info->waiting = provider_host_->GetOrCreateServiceWorkerHandle(
      registration_->waiting_version());
  info->active = provider_host_->GetOrCreateServiceWorkerHandle(
      registration_->active_version());
  return info;
}

void ServiceWorkerRegistrationHandle::OnVersionAttributesChanged(
    ServiceWorkerRegistration* registration,
    ChangedVersionAttributesMask changed_mask,
    const ServiceWorkerRegistrationInfo& info) {
  DCHECK_EQ(registration->id(), registration_->id());
  SetVersionAttributes(changed_mask,
                       registration->installing_version(),
                       registration->waiting_version(),
                       registration->active_version());
}

void ServiceWorkerRegistrationHandle::OnRegistrationFailed(
    ServiceWorkerRegistration* registration) {
  DCHECK_EQ(registration->id(), registration_->id());
  ChangedVersionAttributesMask changed_mask(
      ChangedVersionAttributesMask::INSTALLING_VERSION |
      ChangedVersionAttributesMask::WAITING_VERSION |
      ChangedVersionAttributesMask::ACTIVE_VERSION);
  SetVersionAttributes(changed_mask, nullptr, nullptr, nullptr);
}

void ServiceWorkerRegistrationHandle::OnUpdateFound(
    ServiceWorkerRegistration* registration) {
  DCHECK(remote_registration_);
  remote_registration_->UpdateFound();
}

void ServiceWorkerRegistrationHandle::Update(UpdateCallback callback) {
  if (!CanServeRegistrationObjectHostMethods(&callback,
                                             kServiceWorkerUpdateErrorPrefix)) {
    return;
  }

  if (!registration_->GetNewestVersion()) {
    // This can happen if update() is called during initial script evaluation.
    // Abort the following steps according to the spec.
    std::move(callback).Run(
        blink::mojom::ServiceWorkerErrorType::kState,
        std::string(kServiceWorkerUpdateErrorPrefix) +
            std::string(ServiceWorkerConsts::kInvalidStateErrorMessage));
    return;
  }

  context_->UpdateServiceWorker(
      registration_.get(), false /* force_bypass_cache */,
      false /* skip_script_comparison */, provider_host_.get(),
      base::AdaptCallbackForRepeating(
          base::BindOnce(&ServiceWorkerRegistrationHandle::UpdateComplete,
                         weak_ptr_factory_.GetWeakPtr(), std::move(callback))));
}

void ServiceWorkerRegistrationHandle::Unregister(UnregisterCallback callback) {
  if (!CanServeRegistrationObjectHostMethods(
          &callback, kServiceWorkerUnregisterErrorPrefix)) {
    return;
  }

  context_->UnregisterServiceWorker(
      registration_->pattern(),
      base::AdaptCallbackForRepeating(base::BindOnce(
          &ServiceWorkerRegistrationHandle::UnregistrationComplete,
          weak_ptr_factory_.GetWeakPtr(), std::move(callback))));
}

void ServiceWorkerRegistrationHandle::EnableNavigationPreload(
    bool enable,
    EnableNavigationPreloadCallback callback) {
  if (!CanServeRegistrationObjectHostMethods(
          &callback,
          ServiceWorkerConsts::kEnableNavigationPreloadErrorPrefix)) {
    return;
  }

  if (!registration_->active_version()) {
    std::move(callback).Run(
        blink::mojom::ServiceWorkerErrorType::kState,
        std::string(ServiceWorkerConsts::kEnableNavigationPreloadErrorPrefix) +
            std::string(ServiceWorkerConsts::kNoActiveWorkerErrorMessage));
    return;
  }

  context_->storage()->UpdateNavigationPreloadEnabled(
      registration_->id(), registration_->pattern().GetOrigin(), enable,
      base::AdaptCallbackForRepeating(base::BindOnce(
          &ServiceWorkerRegistrationHandle::DidUpdateNavigationPreloadEnabled,
          weak_ptr_factory_.GetWeakPtr(), enable, std::move(callback))));
}

void ServiceWorkerRegistrationHandle::GetNavigationPreloadState(
    GetNavigationPreloadStateCallback callback) {
  if (!CanServeRegistrationObjectHostMethods(
          &callback, ServiceWorkerConsts::kGetNavigationPreloadStateErrorPrefix,
          nullptr)) {
    return;
  }

  std::move(callback).Run(blink::mojom::ServiceWorkerErrorType::kNone,
                          base::nullopt,
                          registration_->navigation_preload_state().Clone());
}

void ServiceWorkerRegistrationHandle::SetNavigationPreloadHeader(
    const std::string& value,
    SetNavigationPreloadHeaderCallback callback) {
  if (!CanServeRegistrationObjectHostMethods(
          &callback,
          ServiceWorkerConsts::kSetNavigationPreloadHeaderErrorPrefix)) {
    return;
  }

  if (!registration_->active_version()) {
    std::move(callback).Run(
        blink::mojom::ServiceWorkerErrorType::kState,
        std::string(
            ServiceWorkerConsts::kSetNavigationPreloadHeaderErrorPrefix) +
            std::string(ServiceWorkerConsts::kNoActiveWorkerErrorMessage));
    return;
  }

  // TODO(falken): Ideally this would match Blink's isValidHTTPHeaderValue.
  // Chrome's check is less restrictive: it allows non-latin1 characters.
  if (!net::HttpUtil::IsValidHeaderValue(value)) {
    bindings_.ReportBadMessage(
        ServiceWorkerConsts::kBadNavigationPreloadHeaderValue);
    return;
  }

  context_->storage()->UpdateNavigationPreloadHeader(
      registration_->id(), registration_->pattern().GetOrigin(), value,
      base::AdaptCallbackForRepeating(base::BindOnce(
          &ServiceWorkerRegistrationHandle::DidUpdateNavigationPreloadHeader,
          weak_ptr_factory_.GetWeakPtr(), value, std::move(callback))));
}

void ServiceWorkerRegistrationHandle::UpdateComplete(
    UpdateCallback callback,
    ServiceWorkerStatusCode status,
    const std::string& status_message,
    int64_t registration_id) {
  if (status != SERVICE_WORKER_OK) {
    std::string error_message;
    blink::mojom::ServiceWorkerErrorType error_type;
    GetServiceWorkerErrorTypeForRegistration(status, status_message,
                                             &error_type, &error_message);
    std::move(callback).Run(error_type,
                            kServiceWorkerUpdateErrorPrefix + error_message);
    return;
  }

  std::move(callback).Run(blink::mojom::ServiceWorkerErrorType::kNone,
                          base::nullopt);
}

void ServiceWorkerRegistrationHandle::UnregistrationComplete(
    UnregisterCallback callback,
    ServiceWorkerStatusCode status) {
  if (status != SERVICE_WORKER_OK) {
    std::string error_message;
    blink::mojom::ServiceWorkerErrorType error_type;
    GetServiceWorkerErrorTypeForRegistration(status, std::string(), &error_type,
                                             &error_message);
    std::move(callback).Run(
        error_type, kServiceWorkerUnregisterErrorPrefix + error_message);
    return;
  }

  std::move(callback).Run(blink::mojom::ServiceWorkerErrorType::kNone,
                          base::nullopt);
}

void ServiceWorkerRegistrationHandle::DidUpdateNavigationPreloadEnabled(
    bool enable,
    EnableNavigationPreloadCallback callback,
    ServiceWorkerStatusCode status) {
  if (status != SERVICE_WORKER_OK) {
    std::move(callback).Run(
        blink::mojom::ServiceWorkerErrorType::kUnknown,
        std::string(ServiceWorkerConsts::kEnableNavigationPreloadErrorPrefix) +
            std::string(ServiceWorkerConsts::kDatabaseErrorMessage));
    return;
  }

  if (registration_)
    registration_->EnableNavigationPreload(enable);
  std::move(callback).Run(blink::mojom::ServiceWorkerErrorType::kNone,
                          base::nullopt);
}

void ServiceWorkerRegistrationHandle::DidUpdateNavigationPreloadHeader(
    const std::string& value,
    SetNavigationPreloadHeaderCallback callback,
    ServiceWorkerStatusCode status) {
  if (status != SERVICE_WORKER_OK) {
    std::move(callback).Run(
        blink::mojom::ServiceWorkerErrorType::kUnknown,
        std::string(
            ServiceWorkerConsts::kSetNavigationPreloadHeaderErrorPrefix) +
            std::string(ServiceWorkerConsts::kDatabaseErrorMessage));
    return;
  }

  if (registration_)
    registration_->SetNavigationPreloadHeader(value);
  std::move(callback).Run(blink::mojom::ServiceWorkerErrorType::kNone,
                          base::nullopt);
}

void ServiceWorkerRegistrationHandle::SetVersionAttributes(
    ChangedVersionAttributesMask changed_mask,
    ServiceWorkerVersion* installing_version,
    ServiceWorkerVersion* waiting_version,
    ServiceWorkerVersion* active_version) {
  if (!provider_host_)
    return;  // Could be nullptr in some tests.
  if (!changed_mask.changed())
    return;

  blink::mojom::ServiceWorkerObjectInfoPtr installing;
  blink::mojom::ServiceWorkerObjectInfoPtr waiting;
  blink::mojom::ServiceWorkerObjectInfoPtr active;
  if (changed_mask.installing_changed()) {
    installing =
        provider_host_->GetOrCreateServiceWorkerHandle(installing_version);
  }
  if (changed_mask.waiting_changed())
    waiting = provider_host_->GetOrCreateServiceWorkerHandle(waiting_version);
  if (changed_mask.active_changed())
    active = provider_host_->GetOrCreateServiceWorkerHandle(active_version);
  DCHECK(remote_registration_);
  remote_registration_->SetVersionAttributes(
      changed_mask.changed(), std::move(installing), std::move(waiting),
      std::move(active));
}

void ServiceWorkerRegistrationHandle::OnConnectionError() {
  // If there are still bindings, |this| is still being used.
  if (!bindings_.empty())
    return;
  // Will destroy |this|.
  dispatcher_host_->UnregisterServiceWorkerRegistrationHandle(handle_id_);
}

template <typename CallbackType, typename... Args>
bool ServiceWorkerRegistrationHandle::CanServeRegistrationObjectHostMethods(
    CallbackType* callback,
    const char* error_prefix,
    Args... args) {
  if (!provider_host_ || !context_) {
    std::move(*callback).Run(
        blink::mojom::ServiceWorkerErrorType::kAbort,
        std::string(error_prefix) +
            std::string(ServiceWorkerConsts::kShutdownErrorMessage),
        args...);
    return false;
  }

  // TODO(falken): This check can be removed once crbug.com/439697 is fixed.
  // (Also see crbug.com/776408)
  if (provider_host_->document_url().is_empty()) {
    std::move(*callback).Run(
        blink::mojom::ServiceWorkerErrorType::kSecurity,
        std::string(error_prefix) +
            std::string(ServiceWorkerConsts::kNoDocumentURLErrorMessage),
        args...);
    return false;
  }

  std::vector<GURL> urls = {provider_host_->document_url(),
                            registration_->pattern()};
  if (!ServiceWorkerUtils::AllOriginsMatchAndCanAccessServiceWorkers(urls)) {
    bindings_.ReportBadMessage(ServiceWorkerConsts::kBadMessageImproperOrigins);
    return false;
  }

  if (!GetContentClient()->browser()->AllowServiceWorker(
          registration_->pattern(), provider_host_->topmost_frame_url(),
          dispatcher_host_->resource_context(),
          base::Bind(&WebContentsImpl::FromRenderFrameHostID,
                     provider_host_->process_id(),
                     provider_host_->frame_id()))) {
    std::move(*callback).Run(
        blink::mojom::ServiceWorkerErrorType::kDisabled,
        std::string(error_prefix) +
            std::string(ServiceWorkerConsts::kUserDeniedPermissionMessage),
        args...);
    return false;
  }

  return true;
}

}  // namespace content
