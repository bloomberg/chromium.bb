// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_handle.h"

#include "base/memory/ptr_util.h"
#include "content/browser/service_worker/service_worker_client_utils.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_dispatcher_host.h"
#include "content/browser/service_worker/service_worker_provider_host.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_type_converters.h"
#include "content/common/service_worker/service_worker_types.h"
#include "content/common/service_worker/service_worker_utils.h"
#include "content/public/common/browser_side_navigation_policy.h"

namespace content {

namespace {

using StatusCallback = base::OnceCallback<void(ServiceWorkerStatusCode)>;
using SetExtendableMessageEventSourceCallback =
    base::OnceCallback<bool(mojom::ExtendableMessageEventPtr*)>;

void DispatchExtendableMessageEventAfterStartWorker(
    scoped_refptr<ServiceWorkerVersion> worker,
    blink::TransferableMessage message,
    const url::Origin& source_origin,
    const base::Optional<base::TimeDelta>& timeout,
    StatusCallback callback,
    SetExtendableMessageEventSourceCallback set_source_callback,
    ServiceWorkerStatusCode start_worker_status) {
  if (start_worker_status != SERVICE_WORKER_OK) {
    std::move(callback).Run(start_worker_status);
    return;
  }

  mojom::ExtendableMessageEventPtr event = mojom::ExtendableMessageEvent::New();
  event->message = std::move(message);
  event->source_origin = source_origin;
  if (!std::move(set_source_callback).Run(&event)) {
    std::move(callback).Run(SERVICE_WORKER_ERROR_FAILED);
    return;
  }

  int request_id;
  if (timeout) {
    request_id = worker->StartRequestWithCustomTimeout(
        ServiceWorkerMetrics::EventType::MESSAGE, std::move(callback), *timeout,
        ServiceWorkerVersion::CONTINUE_ON_TIMEOUT);
  } else {
    request_id = worker->StartRequest(ServiceWorkerMetrics::EventType::MESSAGE,
                                      std::move(callback));
  }
  worker->event_dispatcher()->DispatchExtendableMessageEvent(
      std::move(event), worker->CreateSimpleEventCallback(request_id));
}

void StartWorkerToDispatchExtendableMessageEvent(
    scoped_refptr<ServiceWorkerVersion> worker,
    blink::TransferableMessage message,
    const url::Origin& source_origin,
    const base::Optional<base::TimeDelta>& timeout,
    StatusCallback callback,
    SetExtendableMessageEventSourceCallback set_source_callback) {
  // If not enough time is left to actually process the event don't even
  // bother starting the worker and sending the event.
  if (timeout && *timeout < base::TimeDelta::FromMilliseconds(100)) {
    std::move(callback).Run(SERVICE_WORKER_ERROR_TIMEOUT);
    return;
  }

  worker->RunAfterStartWorker(
      ServiceWorkerMetrics::EventType::MESSAGE,
      base::BindOnce(&DispatchExtendableMessageEventAfterStartWorker, worker,
                     std::move(message), source_origin, timeout,
                     std::move(callback), std::move(set_source_callback)));
}

bool SetSourceClientInfo(
    blink::mojom::ServiceWorkerClientInfoPtr source_client_info,
    mojom::ExtendableMessageEventPtr* event) {
  DCHECK(source_client_info && !source_client_info->client_uuid.empty());
  (*event)->source_info_for_client = std::move(source_client_info);
  // Hide the client url if the client has a unique origin.
  if ((*event)->source_origin.unique())
    (*event)->source_info_for_client->url = GURL();
  return true;
}

bool SetSourceServiceWorkerInfo(scoped_refptr<ServiceWorkerVersion> worker,
                                base::WeakPtr<ServiceWorkerProviderHost>
                                    source_service_worker_provider_host,
                                mojom::ExtendableMessageEventPtr* event) {
  // The service worker execution context may have been destroyed by the time we
  // get here.
  if (!source_service_worker_provider_host)
    return false;

  DCHECK_EQ(blink::mojom::ServiceWorkerProviderType::kForServiceWorker,
            source_service_worker_provider_host->provider_type());
  blink::mojom::ServiceWorkerObjectInfoPtr source_worker_info =
      worker->provider_host()->GetOrCreateServiceWorkerHandle(
          source_service_worker_provider_host->running_hosted_version());

  (*event)->source_info_for_service_worker = std::move(source_worker_info);
  // Hide the service worker url if the service worker has a unique origin.
  if ((*event)->source_origin.unique())
    (*event)->source_info_for_service_worker->url = GURL();
  return true;
}

void DispatchExtendableMessageEventFromClient(
    scoped_refptr<ServiceWorkerVersion> worker,
    blink::TransferableMessage message,
    const url::Origin& source_origin,
    StatusCallback callback,
    blink::mojom::ServiceWorkerClientInfoPtr source_client_info) {
  // |source_client_info| may be null if a client sent the message but its
  // info could not be retrieved.
  if (!source_client_info) {
    std::move(callback).Run(SERVICE_WORKER_ERROR_FAILED);
    return;
  }

  StartWorkerToDispatchExtendableMessageEvent(
      worker, std::move(message), source_origin, base::nullopt /* timeout */,
      std::move(callback),
      base::BindOnce(&SetSourceClientInfo, std::move(source_client_info)));
}

void DispatchExtendableMessageEventFromServiceWorker(
    scoped_refptr<ServiceWorkerVersion> worker,
    blink::TransferableMessage message,
    const url::Origin& source_origin,
    const base::Optional<base::TimeDelta>& timeout,
    StatusCallback callback,
    base::WeakPtr<ServiceWorkerProviderHost>
        source_service_worker_provider_host) {
  if (!source_service_worker_provider_host) {
    std::move(callback).Run(SERVICE_WORKER_ERROR_FAILED);
    return;
  }

  DCHECK_EQ(blink::mojom::ServiceWorkerProviderType::kForServiceWorker,
            source_service_worker_provider_host->provider_type());
  StartWorkerToDispatchExtendableMessageEvent(
      worker, std::move(message), source_origin, timeout, std::move(callback),
      base::BindOnce(&SetSourceServiceWorkerInfo, worker,
                     source_service_worker_provider_host));
}

}  // namespace

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

void ServiceWorkerHandle::PostMessage(::blink::TransferableMessage message,
                                      const url::Origin& source_origin) {
  // When this method is called the encoded_message inside message could just
  // point to the IPC message's buffer. But that buffer can become invalid
  // before the message is passed on to the service worker, so make sure
  // message owns its data.
  message.EnsureDataIsOwned();

  DispatchExtendableMessageEvent(std::move(message), source_origin,
                                 base::DoNothing());
}

void ServiceWorkerHandle::TerminateForTesting(
    TerminateForTestingCallback callback) {
  version_->StopWorker(std::move(callback));
}

void ServiceWorkerHandle::DispatchExtendableMessageEvent(
    ::blink::TransferableMessage message,
    const url::Origin& source_origin,
    StatusCallback callback) {
  if (!context_ || !provider_host_) {
    std::move(callback).Run(SERVICE_WORKER_ERROR_FAILED);
    return;
  }
  switch (provider_host_->provider_type()) {
    case blink::mojom::ServiceWorkerProviderType::kForWindow:
    // TODO(leonhsl): Move kForSharedWorker to the kUnknown block to clarify
    // that currently a shared worker can not postMessage to a service worker.
    case blink::mojom::ServiceWorkerProviderType::kForSharedWorker:
      service_worker_client_utils::GetClient(
          provider_host_.get(),
          base::BindOnce(&DispatchExtendableMessageEventFromClient, version_,
                         std::move(message), source_origin,
                         std::move(callback)));
      return;
    case blink::mojom::ServiceWorkerProviderType::kForServiceWorker: {
      // Clamp timeout to the sending worker's remaining timeout, to prevent
      // postMessage from keeping workers alive forever.
      base::TimeDelta timeout =
          provider_host_->running_hosted_version()->remaining_timeout();

      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE,
          base::BindOnce(&DispatchExtendableMessageEventFromServiceWorker,
                         version_, std::move(message), source_origin,
                         base::make_optional(timeout), std::move(callback),
                         provider_host_));
      return;
    }
    case blink::mojom::ServiceWorkerProviderType::kUnknown:
      break;
  }
  NOTREACHED() << provider_host_->provider_type();
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
