// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_SHARED_WORKER_WEB_SERVICE_WORKER_NETWORK_PROVIDER_IMPL_FOR_WORKER_H_
#define CONTENT_RENDERER_SHARED_WORKER_WEB_SERVICE_WORKER_NETWORK_PROVIDER_IMPL_FOR_WORKER_H_

#include "content/renderer/service_worker/web_service_worker_network_provider_base_impl.h"

namespace content {

class ServiceWorkerNetworkProvider;
struct NavigationResponseOverrideParameters;

// This is the implementation of WebServiceWorkerNetworkProvider used for
// shared workers and owned by Blink. All functions are called on the main
// thread only.
class WebServiceWorkerNetworkProviderImplForWorker final
    : public WebServiceWorkerNetworkProviderBaseImpl {
 public:
  WebServiceWorkerNetworkProviderImplForWorker(
      std::unique_ptr<ServiceWorkerNetworkProvider> provider,
      bool is_secure_context,
      std::unique_ptr<NavigationResponseOverrideParameters> response_override);
  ~WebServiceWorkerNetworkProviderImplForWorker() override;

  // Blink calls this method for each request starting with the main script,
  // we tag them with the provider id.
  void WillSendRequest(blink::WebURLRequest& request) override;

  std::unique_ptr<blink::WebURLLoader> CreateURLLoader(
      const blink::WebURLRequest& request,
      std::unique_ptr<blink::scheduler::WebResourceLoadingTaskRunnerHandle>
          task_runner_handle) override;

 private:
  const bool is_secure_context_;
  std::unique_ptr<NavigationResponseOverrideParameters> response_override_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_SHARED_WORKER_WEB_SERVICE_WORKER_NETWORK_PROVIDER_IMPL_FOR_WORKER_H_
