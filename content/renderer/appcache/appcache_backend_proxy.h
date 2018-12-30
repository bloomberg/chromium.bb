// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_APPCACHE_APPCACHE_BACKEND_PROXY_H_
#define CONTENT_RENDERER_APPCACHE_APPCACHE_BACKEND_PROXY_H_

#include <stdint.h>

#include <vector>

#include "content/common/appcache.mojom.h"
#include "content/common/appcache_interfaces.h"
#include "ipc/ipc_sender.h"
#include "url/gurl.h"

namespace content {

// Sends appcache related messages to the main process.
class AppCacheBackendProxy {
 public:
  AppCacheBackendProxy();
  ~AppCacheBackendProxy();

  // AppCacheBackend methods
  void RegisterHost(int host_id);
  void UnregisterHost(int host_id);
  void SetSpawningHostId(int host_id, int spawning_host_id);
  void SelectCache(int host_id,
                   const GURL& document_url,
                   const int64_t cache_document_was_loaded_from,
                   const GURL& manifest_url);
  void SelectCacheForSharedWorker(int host_id, int64_t appcache_id);
  void MarkAsForeignEntry(int host_id,
                          const GURL& document_url,
                          int64_t cache_document_was_loaded_from);
  AppCacheStatus GetStatus(int host_id);
  bool StartUpdate(int host_id);
  bool SwapCache(int host_id);
  void GetResourceList(int host_id,
                       std::vector<AppCacheResourceInfo>* resource_infos);

 private:
  mojom::AppCacheBackend* GetAppCacheBackendPtr();

  mojom::AppCacheBackendPtr app_cache_backend_ptr_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_APPCACHE_APPCACHE_BACKEND_PROXY_H_
