// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_SERVICE_WORKER_SERVICE_WORKER_FETCH_CONTEXT_IMPL_H_
#define CONTENT_RENDERER_SERVICE_WORKER_SERVICE_WORKER_FETCH_CONTEXT_IMPL_H_

#include "content/common/worker_url_loader_factory_provider.mojom.h"
#include "third_party/WebKit/public/platform/WebWorkerFetchContext.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace content {
class ResourceDispatcher;

class ServiceWorkerFetchContextImpl : public blink::WebWorkerFetchContext {
 public:
  ServiceWorkerFetchContextImpl(
      mojom::WorkerURLLoaderFactoryProviderPtrInfo provider_info,
      int service_worker_provider_id);
  ~ServiceWorkerFetchContextImpl() override;

  // blink::WebWorkerFetchContext implementation:
  void InitializeOnWorkerThread(base::SingleThreadTaskRunner*) override;
  std::unique_ptr<blink::WebURLLoader> CreateURLLoader() override;
  void WillSendRequest(blink::WebURLRequest&) override;
  bool IsControlledByServiceWorker() const override;

 private:
  mojom::WorkerURLLoaderFactoryProviderPtrInfo provider_info_;
  const int service_worker_provider_id_;

  // Initialized on the worker thread when InitializeOnWorkerThread() is called.
  std::unique_ptr<ResourceDispatcher> resource_dispatcher_;
  mojom::WorkerURLLoaderFactoryProviderPtr provider_;
  mojom::URLLoaderFactoryAssociatedPtr url_loader_factory_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_SERVICE_WORKER_SERVICE_WORKER_FETCH_CONTEXT_IMPL_H_
