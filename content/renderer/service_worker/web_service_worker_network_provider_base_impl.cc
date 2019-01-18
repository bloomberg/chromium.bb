// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/service_worker/web_service_worker_network_provider_base_impl.h"

#include "content/renderer/service_worker/service_worker_network_provider.h"

namespace content {

WebServiceWorkerNetworkProviderBaseImpl::
    WebServiceWorkerNetworkProviderBaseImpl(
        std::unique_ptr<ServiceWorkerNetworkProvider> provider)
    : provider_(std::move(provider)) {}

WebServiceWorkerNetworkProviderBaseImpl::
    ~WebServiceWorkerNetworkProviderBaseImpl() = default;

blink::mojom::ControllerServiceWorkerMode
WebServiceWorkerNetworkProviderBaseImpl::IsControlledByServiceWorker() {
  return provider_->IsControlledByServiceWorker();
}

int64_t WebServiceWorkerNetworkProviderBaseImpl::ControllerServiceWorkerID() {
  if (provider_->context())
    return provider_->context()->GetControllerVersionId();
  return blink::mojom::kInvalidServiceWorkerVersionId;
}

}  // namespace content
