// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/worker/worker_webapplicationcachehost_impl.h"

#include "content/common/appcache/appcache_dispatcher.h"
#include "content/worker/worker_thread.h"

namespace content {

WorkerWebApplicationCacheHostImpl::WorkerWebApplicationCacheHostImpl(
    const WorkerAppCacheInitInfo& init_info,
    WebKit::WebApplicationCacheHostClient* client)
    : WebApplicationCacheHostImpl(client,
          WorkerThread::current()->appcache_dispatcher()->backend_proxy()) {
  backend()->SelectCacheForSharedWorker(host_id(),
                                          init_info.main_resource_appcache_id);
}

void WorkerWebApplicationCacheHostImpl::willStartMainResourceRequest(
    WebKit::WebURLRequest&, const WebKit::WebFrame*) {
}

void WorkerWebApplicationCacheHostImpl::didReceiveResponseForMainResource(
    const WebKit::WebURLResponse&) {
}

void WorkerWebApplicationCacheHostImpl::didReceiveDataForMainResource(
    const char*, int) {
}

void WorkerWebApplicationCacheHostImpl::didFinishLoadingMainResource(
    bool) {
}

void WorkerWebApplicationCacheHostImpl::selectCacheWithoutManifest() {
}

bool WorkerWebApplicationCacheHostImpl::selectCacheWithManifest(
    const WebKit::WebURL&) {
  return true;
}

}  // namespace content
