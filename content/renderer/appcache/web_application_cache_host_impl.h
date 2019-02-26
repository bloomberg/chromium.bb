// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_APPCACHE_WEB_APPLICATION_CACHE_HOST_IMPL_H_
#define CONTENT_RENDERER_APPCACHE_WEB_APPLICATION_CACHE_HOST_IMPL_H_

#include <string>

#include "mojo/public/cpp/bindings/binding.h"
#include "third_party/blink/public/mojom/appcache/appcache.mojom.h"
#include "third_party/blink/public/mojom/appcache/appcache_info.mojom.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom.h"
#include "third_party/blink/public/platform/web_application_cache_host.h"
#include "third_party/blink/public/platform/web_application_cache_host_client.h"
#include "third_party/blink/public/platform/web_url_response.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "url/gurl.h"

namespace content {

class WebApplicationCacheHostImpl : public blink::WebApplicationCacheHost,
                                    public blink::mojom::AppCacheFrontend {
 public:
  // Returns the host having given id or NULL if there is no such host.
  static WebApplicationCacheHostImpl* FromId(int id);

  WebApplicationCacheHostImpl(
      blink::WebApplicationCacheHostClient* client,
      int appcache_host_id,
      int render_frame_id,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);
  ~WebApplicationCacheHostImpl() override;

  int host_id() const { return host_id_; }
  blink::mojom::AppCacheBackend* backend() const { return backend_; }
  blink::WebApplicationCacheHostClient* client() const { return client_; }

  // blink::mojom::AppCacheFrontend
  void CacheSelected(blink::mojom::AppCacheInfoPtr info) override;
  void EventRaised(blink::mojom::AppCacheEventID event_id) override;
  void ProgressEventRaised(const GURL& url,
                           int32_t num_total,
                           int32_t num_complete) override;
  void ErrorEventRaised(blink::mojom::AppCacheErrorDetailsPtr details) override;

  // blink::WebApplicationCacheHost:
  void WillStartMainResourceRequest(
      const blink::WebURL& url,
      const blink::WebString& method,
      const WebApplicationCacheHost* spawning_host) override;
  void SelectCacheWithoutManifest() override;
  bool SelectCacheWithManifest(const blink::WebURL& manifestURL) override;
  void DidReceiveResponseForMainResource(const blink::WebURLResponse&) override;
  blink::mojom::AppCacheStatus GetStatus() override;
  bool StartUpdate() override;
  bool SwapCache() override;
  void GetResourceList(blink::WebVector<ResourceInfo>* resources) override;
  void GetAssociatedCacheInfo(CacheInfo* info) override;
  int GetHostID() const override;

  void SelectCacheForSharedWorker(long long app_cache_id);

 private:
  enum IsNewMasterEntry { MAYBE_NEW_ENTRY, NEW_ENTRY, OLD_ENTRY };

  mojo::Binding<blink::mojom::AppCacheFrontend> binding_;
  blink::WebApplicationCacheHostClient* client_;
  blink::mojom::AppCacheBackend* backend_;
  blink::mojom::AppCacheHostPtr backend_host_;
  int host_id_;
  blink::mojom::AppCacheStatus status_;
  blink::WebURLResponse document_response_;
  GURL document_url_;
  bool is_scheme_supported_;
  bool is_get_method_;
  IsNewMasterEntry is_new_master_entry_;
  blink::mojom::AppCacheInfo cache_info_;
  GURL original_main_resource_url_;  // Used to detect redirection.
  bool was_select_cache_called_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_APPCACHE_WEB_APPLICATION_CACHE_HOST_IMPL_H_
