// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_APPCACHE_APPCACHE_FRONTEND_IMPL_H_
#define CONTENT_RENDERER_APPCACHE_APPCACHE_FRONTEND_IMPL_H_

#include <cstdint>
#include <string>
#include <vector>

#include "mojo/public/cpp/bindings/binding.h"
#include "third_party/blink/public/mojom/appcache/appcache.mojom.h"
#include "third_party/blink/public/mojom/appcache/appcache_info.mojom.h"

namespace content {

// Dispatches appcache related messages sent to a child process from the main
// browser process. There is one instance per child process.
class AppCacheFrontendImpl : public blink::mojom::AppCacheFrontend {
 public:
  AppCacheFrontendImpl();
  ~AppCacheFrontendImpl() override;

  void Bind(blink::mojom::AppCacheFrontendRequest request);

  blink::mojom::AppCacheBackend* backend_proxy();

 private:
  // blink::mojom::AppCacheFrontend
  void CacheSelected(int32_t host_id,
                     blink::mojom::AppCacheInfoPtr info) override;
  void StatusChanged(const std::vector<int32_t>& host_ids,
                     blink::mojom::AppCacheStatus status) override;
  void EventRaised(const std::vector<int32_t>& host_ids,
                   blink::mojom::AppCacheEventID event_id) override;
  void ProgressEventRaised(const std::vector<int32_t>& host_ids,
                           const GURL& url,
                           int32_t num_total,
                           int32_t num_complete) override;
  void ErrorEventRaised(const std::vector<int32_t>& host_ids,
                        blink::mojom::AppCacheErrorDetailsPtr details) override;
  void LogMessage(int32_t host_id,
                  int32_t log_level,
                  const std::string& message) override;
  void ContentBlocked(int32_t host_id, const GURL& manifest_url) override;
  void SetSubresourceFactory(
      int32_t host_id,
      network::mojom::URLLoaderFactoryPtr url_loader_factory) override;

  blink::mojom::AppCacheBackendPtr backend_ptr_;
  mojo::Binding<blink::mojom::AppCacheFrontend> binding_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_APPCACHE_APPCACHE_FRONTEND_IMPL_H_
