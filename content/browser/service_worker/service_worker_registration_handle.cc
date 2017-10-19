// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_registration_handle.h"

#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_dispatcher_host.h"
#include "content/browser/service_worker/service_worker_handle.h"
#include "content/browser/service_worker/service_worker_provider_host.h"
#include "content/common/service_worker/service_worker_messages.h"
#include "content/common/service_worker/service_worker_types.h"
#include "content/common/service_worker/service_worker_utils.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "content/public/common/service_worker_modes.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/service_worker_registration.mojom.h"

namespace content {

namespace {

const char kNoDocumentURLErrorMessage[] =
    "No URL is associated with the caller's document.";
const char kShutdownErrorMessage[] = "The Service Worker system has shutdown.";
const char kUserDeniedPermissionMessage[] =
    "The user denied permission to use Service Worker.";
const char kInvalidStateErrorMessage[] = "The object is in an invalid state.";
const char kBadMessageImproperOrigins[] =
    "Origins are not matching, or some cannot access service worker.";

WebContents* GetWebContents(int render_process_id, int render_frame_id) {
  RenderFrameHost* rfh =
      RenderFrameHost::FromID(render_process_id, render_frame_id);
  return WebContents::FromRenderFrameHost(rfh);
}

}  // anonymous namespace

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
  auto info = blink::mojom::ServiceWorkerRegistrationObjectInfo::New();
  info->handle_id = handle_id_;
  info->options = blink::mojom::ServiceWorkerRegistrationOptions::New(
      registration_->pattern());
  info->registration_id = registration_->id();
  bindings_.AddBinding(this, mojo::MakeRequest(&info->host_ptr_info));
  if (!remote_registration_)
    info->request = mojo::MakeRequest(&remote_registration_);
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
  if (!provider_host_)
    return;  // Could be nullptr in some tests.
  provider_host_->SendUpdateFoundMessage(handle_id_);
}

void ServiceWorkerRegistrationHandle::Update(UpdateCallback callback) {
  if (!provider_host_ || !context_) {
    std::move(callback).Run(blink::mojom::ServiceWorkerErrorType::kAbort,
                            std::string(kServiceWorkerUpdateErrorPrefix) +
                                std::string(kShutdownErrorMessage));
    return;
  }
  // TODO(falken): This check can be removed once crbug.com/439697 is fixed.
  if (provider_host_->document_url().is_empty()) {
    std::move(callback).Run(blink::mojom::ServiceWorkerErrorType::kSecurity,
                            std::string(kServiceWorkerUpdateErrorPrefix) +
                                std::string(kNoDocumentURLErrorMessage));
    return;
  }

  std::vector<GURL> urls = {provider_host_->document_url(),
                            registration_->pattern()};
  if (!ServiceWorkerUtils::AllOriginsMatchAndCanAccessServiceWorkers(urls)) {
    bindings_.ReportBadMessage(kBadMessageImproperOrigins);
    return;
  }

  if (!GetContentClient()->browser()->AllowServiceWorker(
          registration_->pattern(), provider_host_->topmost_frame_url(),
          dispatcher_host_->resource_context(),
          base::Bind(&GetWebContents, provider_host_->process_id(),
                     provider_host_->frame_id()))) {
    std::move(callback).Run(blink::mojom::ServiceWorkerErrorType::kDisabled,
                            std::string(kServiceWorkerUpdateErrorPrefix) +
                                std::string(kUserDeniedPermissionMessage));
    return;
  }

  if (!registration_->GetNewestVersion()) {
    // This can happen if update() is called during initial script evaluation.
    // Abort the following steps according to the spec.
    std::move(callback).Run(blink::mojom::ServiceWorkerErrorType::kState,
                            std::string(kServiceWorkerUpdateErrorPrefix) +
                                std::string(kInvalidStateErrorMessage));
    return;
  }

  context_->UpdateServiceWorker(
      registration_.get(), false /* force_bypass_cache */,
      false /* skip_script_comparison */, provider_host_.get(),
      base::AdaptCallbackForRepeating(
          base::BindOnce(&ServiceWorkerRegistrationHandle::UpdateComplete,
                         weak_ptr_factory_.GetWeakPtr(), std::move(callback))));
}

void ServiceWorkerRegistrationHandle::UpdateComplete(
    UpdateCallback callback,
    ServiceWorkerStatusCode status,
    const std::string& status_message,
    int64_t registration_id) {
  if (!provider_host_ || !context_) {
    std::move(callback).Run(blink::mojom::ServiceWorkerErrorType::kAbort,
                            std::string(kServiceWorkerUpdateErrorPrefix) +
                                std::string(kShutdownErrorMessage));
    return;
  }

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

void ServiceWorkerRegistrationHandle::SetVersionAttributes(
    ChangedVersionAttributesMask changed_mask,
    ServiceWorkerVersion* installing_version,
    ServiceWorkerVersion* waiting_version,
    ServiceWorkerVersion* active_version) {
  if (!provider_host_)
    return;  // Could be nullptr in some tests.
  provider_host_->SendSetVersionAttributesMessage(handle_id_,
                                                  changed_mask,
                                                  installing_version,
                                                  waiting_version,
                                                  active_version);
}

void ServiceWorkerRegistrationHandle::OnConnectionError() {
  // If there are still bindings, |this| is still being used.
  if (!bindings_.empty())
    return;
  // Will destroy |this|.
  dispatcher_host_->UnregisterServiceWorkerRegistrationHandle(handle_id_);
}

}  // namespace content
