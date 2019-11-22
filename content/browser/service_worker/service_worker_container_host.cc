// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_container_host.h"

#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_object_host.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_registration_object_host.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/common/content_client.h"

namespace content {

ServiceWorkerContainerHost::ServiceWorkerContainerHost(
    blink::mojom::ServiceWorkerProviderType type,
    ServiceWorkerProviderHost* provider_host,
    base::WeakPtr<ServiceWorkerContextCore> context)
    : type_(type), provider_host_(provider_host), context_(std::move(context)) {
  DCHECK(provider_host_);
  DCHECK(context_);
}

ServiceWorkerContainerHost::~ServiceWorkerContainerHost() = default;

blink::mojom::ServiceWorkerRegistrationObjectInfoPtr
ServiceWorkerContainerHost::CreateServiceWorkerRegistrationObjectInfo(
    scoped_refptr<ServiceWorkerRegistration> registration) {
  int64_t registration_id = registration->id();
  auto existing_host = registration_object_hosts_.find(registration_id);
  if (existing_host != registration_object_hosts_.end()) {
    return existing_host->second->CreateObjectInfo();
  }
  registration_object_hosts_[registration_id] =
      std::make_unique<ServiceWorkerRegistrationObjectHost>(
          context_, this, std::move(registration));
  return registration_object_hosts_[registration_id]->CreateObjectInfo();
}

void ServiceWorkerContainerHost::RemoveServiceWorkerRegistrationObjectHost(
    int64_t registration_id) {
  DCHECK(base::Contains(registration_object_hosts_, registration_id));
  registration_object_hosts_.erase(registration_id);
}

blink::mojom::ServiceWorkerObjectInfoPtr
ServiceWorkerContainerHost::CreateServiceWorkerObjectInfoToSend(
    scoped_refptr<ServiceWorkerVersion> version) {
  int64_t version_id = version->version_id();
  auto existing_object_host = service_worker_object_hosts_.find(version_id);
  if (existing_object_host != service_worker_object_hosts_.end()) {
    return existing_object_host->second->CreateCompleteObjectInfoToSend();
  }
  service_worker_object_hosts_[version_id] =
      std::make_unique<ServiceWorkerObjectHost>(context_, this,
                                                std::move(version));
  return service_worker_object_hosts_[version_id]
      ->CreateCompleteObjectInfoToSend();
}

base::WeakPtr<ServiceWorkerObjectHost>
ServiceWorkerContainerHost::GetOrCreateServiceWorkerObjectHost(
    scoped_refptr<ServiceWorkerVersion> version) {
  if (!context_ || !version)
    return nullptr;

  const int64_t version_id = version->version_id();
  auto existing_object_host = service_worker_object_hosts_.find(version_id);
  if (existing_object_host != service_worker_object_hosts_.end())
    return existing_object_host->second->AsWeakPtr();

  service_worker_object_hosts_[version_id] =
      std::make_unique<ServiceWorkerObjectHost>(context_, this,
                                                std::move(version));
  return service_worker_object_hosts_[version_id]->AsWeakPtr();
}

void ServiceWorkerContainerHost::RemoveServiceWorkerObjectHost(
    int64_t version_id) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  DCHECK(base::Contains(service_worker_object_hosts_, version_id));
  service_worker_object_hosts_.erase(version_id);
}

bool ServiceWorkerContainerHost::IsContainerForServiceWorker() const {
  return type_ == blink::mojom::ServiceWorkerProviderType::kForServiceWorker;
}

bool ServiceWorkerContainerHost::IsContainerForClient() const {
  switch (type_) {
    case blink::mojom::ServiceWorkerProviderType::kForWindow:
    case blink::mojom::ServiceWorkerProviderType::kForDedicatedWorker:
    case blink::mojom::ServiceWorkerProviderType::kForSharedWorker:
      return true;
    case blink::mojom::ServiceWorkerProviderType::kForServiceWorker:
      return false;
    case blink::mojom::ServiceWorkerProviderType::kUnknown:
      break;
  }
  NOTREACHED() << type_;
  return false;
}

blink::mojom::ServiceWorkerClientType ServiceWorkerContainerHost::client_type()
    const {
  switch (type_) {
    case blink::mojom::ServiceWorkerProviderType::kForWindow:
      return blink::mojom::ServiceWorkerClientType::kWindow;
    case blink::mojom::ServiceWorkerProviderType::kForDedicatedWorker:
      return blink::mojom::ServiceWorkerClientType::kDedicatedWorker;
    case blink::mojom::ServiceWorkerProviderType::kForSharedWorker:
      return blink::mojom::ServiceWorkerClientType::kSharedWorker;
    case blink::mojom::ServiceWorkerProviderType::kForServiceWorker:
    case blink::mojom::ServiceWorkerProviderType::kUnknown:
      break;
  }
  NOTREACHED() << type_;
  return blink::mojom::ServiceWorkerClientType::kWindow;
}

void ServiceWorkerContainerHost::UpdateUrls(
    const GURL& url,
    const GURL& site_for_cookies,
    const base::Optional<url::Origin>& top_frame_origin) {
  DCHECK(!url.has_ref());
  url_ = url;
  site_for_cookies_ = site_for_cookies;
  top_frame_origin_ = top_frame_origin;
}

bool ServiceWorkerContainerHost::AllowServiceWorker(const GURL& scope,
                                                    const GURL& script_url,
                                                    int render_process_id,
                                                    int frame_id) {
  DCHECK(context_);
  if (ServiceWorkerContext::IsServiceWorkerOnUIEnabled()) {
    return GetContentClient()->browser()->AllowServiceWorkerOnUI(
        scope, site_for_cookies(), top_frame_origin(), script_url,
        context_->wrapper()->browser_context(),
        base::BindRepeating(&WebContentsImpl::FromRenderFrameHostID,
                            render_process_id, frame_id));
  } else {
    return GetContentClient()->browser()->AllowServiceWorkerOnIO(
        scope, site_for_cookies(), top_frame_origin(), script_url,
        context_->wrapper()->resource_context(),
        base::BindRepeating(&WebContentsImpl::FromRenderFrameHostID,
                            render_process_id, frame_id));
  }
}

}  // namespace content
