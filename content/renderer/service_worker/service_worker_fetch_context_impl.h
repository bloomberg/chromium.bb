// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_SERVICE_WORKER_SERVICE_WORKER_FETCH_CONTEXT_IMPL_H_
#define CONTENT_RENDERER_SERVICE_WORKER_SERVICE_WORKER_FETCH_CONTEXT_IMPL_H_

#include "content/public/common/shared_url_loader_factory.h"
#include "third_party/WebKit/public/platform/WebWorkerFetchContext.h"
#include "url/gurl.h"

namespace content {
class ResourceDispatcher;
class URLLoaderThrottleProvider;

class ServiceWorkerFetchContextImpl : public blink::WebWorkerFetchContext {
 public:
  ServiceWorkerFetchContextImpl(
      const GURL& worker_script_url,
      std::unique_ptr<SharedURLLoaderFactoryInfo> url_loader_factory_info,
      int service_worker_provider_id,
      std::unique_ptr<URLLoaderThrottleProvider> throttle_provider);
  ~ServiceWorkerFetchContextImpl() override;

  // blink::WebWorkerFetchContext implementation:
  void InitializeOnWorkerThread() override;
  std::unique_ptr<blink::WebURLLoaderFactory> CreateURLLoaderFactory() override;
  std::unique_ptr<blink::WebURLLoaderFactory> WrapURLLoaderFactory(
      mojo::ScopedMessagePipeHandle url_loader_factory_handle) override;
  void WillSendRequest(blink::WebURLRequest&) override;
  bool IsControlledByServiceWorker() const override;
  blink::WebURL SiteForCookies() const override;

 private:
  const GURL worker_script_url_;
  // Consumed on the worker thread to create |url_loader_factory_|.
  std::unique_ptr<SharedURLLoaderFactoryInfo> url_loader_factory_info_;
  const int service_worker_provider_id_;

  // Initialized on the worker thread when InitializeOnWorkerThread() is called.
  std::unique_ptr<ResourceDispatcher> resource_dispatcher_;
  scoped_refptr<SharedURLLoaderFactory> url_loader_factory_;

  std::unique_ptr<URLLoaderThrottleProvider> throttle_provider_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_SERVICE_WORKER_SERVICE_WORKER_FETCH_CONTEXT_IMPL_H_
