// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_WORKER_WORKER_WEBAPPLICATIONCACHEHOST_IMPL_H_
#define CHROME_WORKER_WORKER_WEBAPPLICATIONCACHEHOST_IMPL_H_

#include "webkit/appcache/web_application_cache_host_impl.h"

namespace content {

// Information used to construct and initialize an appcache host
// for a worker.
struct WorkerAppCacheInitInfo {
  int parent_process_id;
  int64 main_resource_appcache_id;  // Only valid for shared workers.

  WorkerAppCacheInitInfo()
      : parent_process_id(0),
        main_resource_appcache_id(0) {
  }
  WorkerAppCacheInitInfo(
      int process_id, int64 cache_id)
      : parent_process_id(process_id),
        main_resource_appcache_id(cache_id) {
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
  // These overrides are stubbed out.
  virtual void willStartMainResourceRequest(
      WebKit::WebURLRequest&, const WebKit::WebFrame*);
  virtual void didReceiveResponseForMainResource(
      const WebKit::WebURLResponse&);
  virtual void didReceiveDataForMainResource(const char* data, int len);
  virtual void didFinishLoadingMainResource(bool success);

  // Cache selection is also different for workers. We know at construction
  // time what cache to select and do so then.
  // These overrides are stubbed out.
  virtual void selectCacheWithoutManifest();
  virtual bool selectCacheWithManifest(const WebKit::WebURL& manifestURL);
};

}  // namespace content

#endif  // CHROME_WORKER_WORKER_WEBAPPLICATIONCACHEHOST_IMPL_H_
