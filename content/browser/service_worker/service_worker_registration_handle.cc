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
#include "content/public/common/service_worker_modes.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/service_worker_registration.mojom.h"

namespace content {

ServiceWorkerRegistrationHandle::ServiceWorkerRegistrationHandle(
    base::WeakPtr<ServiceWorkerContextCore> context,
    ServiceWorkerDispatcherHost* dispatcher_host,
    base::WeakPtr<ServiceWorkerProviderHost> provider_host,
    ServiceWorkerRegistration* registration)
    : dispatcher_host_(dispatcher_host),
      provider_host_(provider_host),
      provider_id_(provider_host ? provider_host->provider_id()
                                 : kInvalidServiceWorkerProviderId),
      handle_id_(context
                     ? context->GetNewRegistrationHandleId()
                     : blink::mojom::kInvalidServiceWorkerRegistrationHandleId),
      registration_(registration) {
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
