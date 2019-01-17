// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_APPCACHE_APPCACHE_BACKEND_IMPL_H_
#define CONTENT_BROWSER_APPCACHE_APPCACHE_BACKEND_IMPL_H_

#include <stdint.h>

#include "content/browser/appcache/appcache_host.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/mojom/appcache/appcache.mojom.h"

namespace content {

class AppCacheServiceImpl;

class CONTENT_EXPORT AppCacheBackendImpl {
 public:
  AppCacheBackendImpl();
  ~AppCacheBackendImpl();

  void Initialize(AppCacheServiceImpl* service,
                  AppCacheFrontend* frontend,
                  int process_id);

  int process_id() const { return process_id_; }

  // Methods to support the AppCacheBackend interface. A false return
  // value indicates an invalid host_id and that no action was taken
  // by the backend impl.
  bool RegisterHost(int host_id);
  bool UnregisterHost(int host_id);
  bool SetSpawningHostId(int host_id, int spawning_host_id);
  bool SelectCache(int host_id,
                   const GURL& document_url,
                   const int64_t cache_document_was_loaded_from,
                   const GURL& manifest_url);
  void GetResourceList(
      int host_id,
      std::vector<blink::mojom::AppCacheResourceInfo>* resource_infos);
  bool SelectCacheForSharedWorker(int host_id, int64_t appcache_id);
  bool MarkAsForeignEntry(int host_id,
                          const GURL& document_url,
                          int64_t cache_document_was_loaded_from);

  // The xxxWithCallback functions take ownership of the callback iff the host
  // is found (and the return value is true). If the result is false, the
  // callback might still be available to the caller of these methods.
  // TODO(mek): Just pass callbacks unconditionally. That is possible if the
  // caller is changed to call BindingSet::ReportBadMessage rather than calling
  // the global mojo::ReportBadMessage.
  bool GetStatusWithCallback(int host_id, GetStatusCallback* callback);
  bool StartUpdateWithCallback(int host_id, StartUpdateCallback* callback);
  bool SwapCacheWithCallback(int host_id, SwapCacheCallback* callback);

  // Returns a pointer to a registered host. The backend retains ownership.
  AppCacheHost* GetHost(int host_id) {
    auto it = hosts_.find(host_id);
    return (it != hosts_.end()) ? (it->second.get()) : nullptr;
  }

  using HostMap = std::unordered_map<int, std::unique_ptr<AppCacheHost>>;
  const HostMap& hosts() { return hosts_; }

  // The AppCacheHost is precreated by the AppCacheNavigationHandleCore class
  // when a navigation is initiated. We register the host with the backend in
  // this function and ignore registrations for this host id from the renderer.
  void RegisterPrecreatedHost(std::unique_ptr<AppCacheHost> host);

 private:
  AppCacheServiceImpl* service_;
  AppCacheFrontend* frontend_;
  int process_id_;
  HostMap hosts_;

  DISALLOW_COPY_AND_ASSIGN(AppCacheBackendImpl);
};

}  // namespace

#endif  // CONTENT_BROWSER_APPCACHE_APPCACHE_BACKEND_IMPL_H_
