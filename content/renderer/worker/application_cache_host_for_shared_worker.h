// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_WORKER_APPLICATION_CACHE_HOST_FOR_SHARED_WORKER_H_
#define CONTENT_RENDERER_WORKER_APPLICATION_CACHE_HOST_FOR_SHARED_WORKER_H_

#include "content/renderer/appcache/web_application_cache_host_impl.h"

namespace content {

class ApplicationCacheHostForSharedWorker final
    : public WebApplicationCacheHostImpl {
 public:
  ApplicationCacheHostForSharedWorker(
      blink::WebApplicationCacheHostClient* client,
      int appcache_host_id,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);
  ~ApplicationCacheHostForSharedWorker() override;

  // Main resource loading is different for workers. The main resource is
  // loaded by the worker using WorkerClassicScriptLoader.
  // These overrides are stubbed out.
  void WillStartMainResourceRequest(
      const blink::WebURL& url,
      const blink::WebString& method,
      const WebApplicationCacheHost* spawning_host) override;
  void DidReceiveResponseForMainResource(const blink::WebURLResponse&) override;

  // Cache selection is also different for workers. We know at construction
  // time what cache to select and do so then.
  // These overrides are stubbed out.
  void SelectCacheWithoutManifest() override;
  bool SelectCacheWithManifest(const blink::WebURL& manifestURL) override;

  // blink::mojom::AppCacheFrontend:
  void LogMessage(blink::mojom::ConsoleMessageLevel log_level,
                  const std::string& message) override;
  void SetSubresourceFactory(
      network::mojom::URLLoaderFactoryPtr url_loader_factory) override;
};

}  // namespace content

#endif  // CONTENT_RENDERER_WORKER_APPLICATION_CACHE_HOST_FOR_SHARED_WORKER_H_
