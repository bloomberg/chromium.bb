// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_SERVICE_WORKER_WEB_SERVICE_WORKER_NETWORK_PROVIDER_BASE_IMPL_H_
#define CONTENT_RENDERER_SERVICE_WORKER_WEB_SERVICE_WORKER_NETWORK_PROVIDER_BASE_IMPL_H_

#include "third_party/blink/public/platform/modules/service_worker/web_service_worker_network_provider.h"

namespace content {

class ServiceWorkerNetworkProvider;

// This is the base implementation of WebServiceWorkerNetworkProvider. All
// functions are called on the main thread only.
class WebServiceWorkerNetworkProviderBaseImpl
    : public blink::WebServiceWorkerNetworkProvider {
 public:
  explicit WebServiceWorkerNetworkProviderBaseImpl(
      std::unique_ptr<ServiceWorkerNetworkProvider>);
  ~WebServiceWorkerNetworkProviderBaseImpl() override;

  // Implement blink::WebServiceWorkerNetworkProvider.
  blink::mojom::ControllerServiceWorkerMode IsControlledByServiceWorker() final;
  int64_t ControllerServiceWorkerID() final;

  ServiceWorkerNetworkProvider* provider() { return provider_.get(); }

 private:
  std::unique_ptr<ServiceWorkerNetworkProvider> provider_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_SERVICE_WORKER_WEB_SERVICE_WORKER_NETWORK_PROVIDER_BASE_IMPL_H_
