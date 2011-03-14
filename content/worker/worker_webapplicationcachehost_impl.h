// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_WORKER_WORKER_WEBAPPLICATIONCACHEHOST_IMPL_H_
#define CHROME_WORKER_WORKER_WEBAPPLICATIONCACHEHOST_IMPL_H_
#pragma once

#include "webkit/appcache/web_application_cache_host_impl.h"

// Information used to construct and initialize an appcache host
// for a worker.
struct WorkerAppCacheInitInfo {
  bool is_shared_worker;
  int parent_process_id;
  int parent_appcache_host_id;  // Only valid for dedicated workers.
  int64 main_resource_appcache_id;  // Only valid for shared workers.

  WorkerAppCacheInitInfo()
      : is_shared_worker(false), parent_process_id(0),
        parent_appcache_host_id(0), main_resource_appcache_id(0) {
  }
  WorkerAppCacheInitInfo(
      bool is_shared, int process_id, int host_id, int64 cache_id)
      : is_shared_worker(is_shared), parent_process_id(process_id),
        parent_appcache_host_id(host_id), main_resource_appcache_id(cache_id) {
  }
};

class WorkerWebApplicationCacheHostImpl
    : public appcache::WebApplicationCacheHostImpl {
 public:
  WorkerWebApplicationCacheHostImpl(
      const WorkerAppCacheInitInfo& init_info,
      WebKit::WebApplicationCacheHostClient* client);

  // Main resource loading is different for workers. The resource is
  // loaded by the creator of the worker rather than the worker itself.
  virtual void willStartMainResourceRequest(WebKit::WebURLRequest&) {}
  virtual void didReceiveResponseForMainResource(
      const WebKit::WebURLResponse&) {}
  virtual void didReceiveDataForMainResource(const char* data, int len) {}
  virtual void didFinishLoadingMainResource(bool success) {}

  // Cache selection is also different for workers. We know at construction
  // time what cache to select and do so then.
  virtual void selectCacheWithoutManifest() {}
  virtual bool selectCacheWithManifest(const WebKit::WebURL& manifestURL);
};

#endif  // CHROME_WORKER_WORKER_WEBAPPLICATIONCACHEHOST_IMPL_H_
