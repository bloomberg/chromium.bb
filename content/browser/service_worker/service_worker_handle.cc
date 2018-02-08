// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_handle.h"

#include "base/memory/ptr_util.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_dispatcher_host.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_type_converters.h"
#include "content/common/service_worker/service_worker_messages.h"
#include "content/common/service_worker/service_worker_types.h"
#include "content/common/service_worker/service_worker_utils.h"
#include "content/public/common/browser_side_navigation_policy.h"
#include "content/public/common/service_worker_modes.h"

namespace content {

// static
base::WeakPtr<ServiceWorkerHandle> ServiceWorkerHandle::Create(
    ServiceWorkerDispatcherHost* dispatcher_host,
    base::WeakPtr<ServiceWorkerContextCore> context,
    base::WeakPtr<ServiceWorkerProviderHost> provider_host,
    ServiceWorkerVersion* version,
    blink::mojom::ServiceWorkerObjectInfoPtr* out_info) {
  DCHECK(context && provider_host && version && out_info);
  ServiceWorkerHandle* handle =
      new ServiceWorkerHandle(dispatcher_host, context, provider_host, version);
  *out_info = handle->CreateObjectInfo();
  return handle->AsWeakPtr();
}

ServiceWorkerHandle::ServiceWorkerHandle(
    ServiceWorkerDispatcherHost* dispatcher_host,
    base::WeakPtr<ServiceWorkerContextCore> context,
    base::WeakPtr<ServiceWorkerProviderHost> provider_host,
    ServiceWorkerVersion* version)
    : dispatcher_host_(dispatcher_host),
      context_(context),
      provider_host_(provider_host),
      provider_id_(provider_host->provider_id()),
      handle_id_(context->GetNewServiceWorkerHandleId()),
      version_(version),
      weak_ptr_factory_(this) {
  DCHECK(context_ && provider_host_ && version_);
  DCHECK(context_->GetLiveRegistration(version_->registration_id()));
  version_->AddListener(this);
  bindings_.set_connection_error_handler(base::BindRepeating(
      &ServiceWorkerHandle::OnConnectionError, base::Unretained(this)));
  if (dispatcher_host_)
    dispatcher_host_->RegisterServiceWorkerHandle(base::WrapUnique(this));
}

ServiceWorkerHandle::~ServiceWorkerHandle() {
  version_->RemoveListener(this);
}

void ServiceWorkerHandle::OnVersionStateChanged(ServiceWorkerVersion* version) {
  DCHECK(version);
  if (!provider_host_)
    return;
  provider_host_->SendServiceWorkerStateChangedMessage(
      handle_id_,
      mojo::ConvertTo<blink::mojom::ServiceWorkerState>(version->status()));
}

blink::mojom::ServiceWorkerObjectInfoPtr
ServiceWorkerHandle::CreateObjectInfo() {
  auto info = blink::mojom::ServiceWorkerObjectInfo::New();
  info->handle_id = handle_id_;
  info->url = version_->script_url();
  info->state =
      mojo::ConvertTo<blink::mojom::ServiceWorkerState>(version_->status());
  info->version_id = version_->version_id();
  bindings_.AddBinding(this, mojo::MakeRequest(&info->host_ptr_info));
  return info;
}

void ServiceWorkerHandle::RegisterIntoDispatcherHost(
    ServiceWorkerDispatcherHost* dispatcher_host) {
  DCHECK(ServiceWorkerUtils::IsServicificationEnabled() ||
         IsNavigationMojoResponseEnabled());
  DCHECK(!dispatcher_host_);
  dispatcher_host_ = dispatcher_host;
  dispatcher_host_->RegisterServiceWorkerHandle(base::WrapUnique(this));
}

base::WeakPtr<ServiceWorkerHandle> ServiceWorkerHandle::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void ServiceWorkerHandle::OnConnectionError() {
  // If there are still bindings, |this| is still being used.
  if (!bindings_.empty())
    return;
  // S13nServiceWorker: This handle may have been precreated before registering
  // to a dispatcher host. Just self-destruct since we're no longer needed.
  if (!dispatcher_host_) {
    DCHECK(ServiceWorkerUtils::IsServicificationEnabled() ||
           IsNavigationMojoResponseEnabled());
    delete this;
    return;
  }
  // Will destroy |this|.
  dispatcher_host_->UnregisterServiceWorkerHandle(handle_id_);
}

}  // namespace content
