// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/worker/application_cache_host_for_shared_worker.h"

namespace content {

ApplicationCacheHostForSharedWorker::ApplicationCacheHostForSharedWorker(
    blink::WebApplicationCacheHostClient* client,
    int appcache_host_id,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : WebApplicationCacheHostImpl(client,
                                  appcache_host_id,
                                  MSG_ROUTING_NONE,
                                  std::move(task_runner)) {}

ApplicationCacheHostForSharedWorker::~ApplicationCacheHostForSharedWorker() =
    default;

void ApplicationCacheHostForSharedWorker::WillStartMainResourceRequest(
    const blink::WebURL& url,
    const blink::WebString& method,
    const WebApplicationCacheHost* spawning_host) {}

void ApplicationCacheHostForSharedWorker::DidReceiveResponseForMainResource(
    const blink::WebURLResponse&) {}

void ApplicationCacheHostForSharedWorker::SelectCacheWithoutManifest() {}

bool ApplicationCacheHostForSharedWorker::SelectCacheWithManifest(
    const blink::WebURL& manifestURL) {
  return true;
}

void ApplicationCacheHostForSharedWorker::LogMessage(
    blink::mojom::ConsoleMessageLevel log_level,
    const std::string& message) {}

void ApplicationCacheHostForSharedWorker::SetSubresourceFactory(
    network::mojom::URLLoaderFactoryPtr url_loader_factory) {}

}  // namespace content
