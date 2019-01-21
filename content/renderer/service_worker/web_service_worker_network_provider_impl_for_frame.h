// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_SERVICE_WORKER_WEB_SERVICE_WORKER_NETWORK_PROVIDER_IMPL_FOR_FRAME_H_
#define CONTENT_RENDERER_SERVICE_WORKER_WEB_SERVICE_WORKER_NETWORK_PROVIDER_IMPL_FOR_FRAME_H_

#include "content/renderer/service_worker/web_service_worker_network_provider_base_impl.h"

namespace content {

// An WebServiceWorkerNetworkProvider for frame. This wraps
// ServiceWorkerNetworkProvider implementation and is owned by blink.
class WebServiceWorkerNetworkProviderImplForFrame final
    : public WebServiceWorkerNetworkProviderBaseImpl {
 public:
  explicit WebServiceWorkerNetworkProviderImplForFrame(
      std::unique_ptr<ServiceWorkerNetworkProvider> provider);

  // Implements WebServiceWorkerNetworkProviderBaseImpl.
  void WillSendRequest(blink::WebURLRequest& request) override;
  std::unique_ptr<blink::WebURLLoader> CreateURLLoader(
      const blink::WebURLRequest& request,
      std::unique_ptr<blink::scheduler::WebResourceLoadingTaskRunnerHandle>
          task_runner_handle) override;
  void DispatchNetworkQuiet() override;
};

}  // namespace content

#endif  // CONTENT_RENDERER_SERVICE_WORKER_WEB_SERVICE_WORKER_NETWORK_PROVIDER_IMPL_FOR_FRAME_H_
