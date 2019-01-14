// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/appcache/appcache_frontend_proxy.h"

#include "base/task/post_task.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/bind_interface_helpers.h"
#include "third_party/blink/public/mojom/appcache/appcache.mojom.h"
#include "third_party/blink/public/mojom/appcache/appcache_info.mojom.h"

namespace content {

AppCacheFrontendProxy::AppCacheFrontendProxy(int process_id)
    : process_id_(process_id) {}

AppCacheFrontendProxy::~AppCacheFrontendProxy() {}

namespace {
void BindOnUIThread(int process_id,
                    blink::mojom::AppCacheFrontendRequest request) {
  if (auto* render_process_host = RenderProcessHost::FromID(process_id)) {
    BindInterface(render_process_host, std::move(request));
  }
}
}  // namespace

blink::mojom::AppCacheFrontend* AppCacheFrontendProxy::GetAppCacheFrontend() {
  if (!app_cache_renderer_ptr_) {
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(&BindOnUIThread, process_id_,
                       mojo::MakeRequest(&app_cache_renderer_ptr_)));
  }
  return app_cache_renderer_ptr_.get();
}

void AppCacheFrontendProxy::OnCacheSelected(
    int host_id,
    const blink::mojom::AppCacheInfo& info) {
  // TODO(crbug:611938) Get rid of the need to Clone().
  GetAppCacheFrontend()->CacheSelected(host_id, info.Clone());
}

void AppCacheFrontendProxy::OnStatusChanged(
    const std::vector<int>& host_ids,
    blink::mojom::AppCacheStatus status) {
  GetAppCacheFrontend()->StatusChanged(host_ids, status);
}

void AppCacheFrontendProxy::OnEventRaised(
    const std::vector<int>& host_ids,
    blink::mojom::AppCacheEventID event_id) {
  DCHECK_NE(blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT,
            event_id);  // See OnProgressEventRaised.
  GetAppCacheFrontend()->EventRaised(host_ids, event_id);
}

void AppCacheFrontendProxy::OnProgressEventRaised(
    const std::vector<int>& host_ids,
    const GURL& url, int num_total, int num_complete) {
  GetAppCacheFrontend()->ProgressEventRaised(host_ids, url, num_total,
                                             num_complete);
}

void AppCacheFrontendProxy::OnErrorEventRaised(
    const std::vector<int>& host_ids,
    const blink::mojom::AppCacheErrorDetails& details) {
  GetAppCacheFrontend()->ErrorEventRaised(host_ids, details.Clone());
}

void AppCacheFrontendProxy::OnLogMessage(int host_id,
                                         AppCacheLogLevel log_level,
                                         const std::string& message) {
  GetAppCacheFrontend()->LogMessage(host_id, log_level, message);
}

void AppCacheFrontendProxy::OnContentBlocked(int host_id,
                                             const GURL& manifest_url) {
  GetAppCacheFrontend()->ContentBlocked(host_id, manifest_url);
}

void AppCacheFrontendProxy::OnSetSubresourceFactory(
    int host_id,
    network::mojom::URLLoaderFactoryPtr url_loader_factory) {
  GetAppCacheFrontend()->SetSubresourceFactory(host_id,
                                               std::move(url_loader_factory));
}

}  // namespace content
