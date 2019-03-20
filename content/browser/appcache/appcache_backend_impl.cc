// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/appcache/appcache_backend_impl.h"

#include <memory>
#include <tuple>
#include <utility>
#include <vector>

#include "content/browser/appcache/appcache.h"
#include "content/browser/appcache/appcache_group.h"
#include "content/browser/appcache/appcache_navigation_handle_core.h"
#include "content/browser/appcache/appcache_service_impl.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/blink/public/mojom/appcache/appcache.mojom.h"

namespace content {

AppCacheBackendImpl::AppCacheBackendImpl(AppCacheServiceImpl* service,
                                         int process_id)
    : service_(service),
      process_id_(process_id) {
  DCHECK(service);
  service_->RegisterBackend(this);
}

AppCacheBackendImpl::~AppCacheBackendImpl() {
  hosts_.clear();
  service_->UnregisterBackend(this);
}

void AppCacheBackendImpl::RegisterHost(
    blink::mojom::AppCacheHostRequest host_request,
    blink::mojom::AppCacheFrontendPtr frontend,
    int32_t id,
    int32_t render_frame_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (GetHost(id)) {
    mojo::ReportBadMessage("ACDH_REGISTER");
    return;
  }

  // The AppCacheHost could have been precreated in which case we want to
  // register it with the backend here.
  std::unique_ptr<AppCacheHost> host =
      AppCacheNavigationHandleCore::GetPrecreatedHost(id);
  if (host) {
    // Switch the frontend proxy so that the host can make IPC calls from
    // here on.
    host->set_frontend(std::move(frontend), render_frame_id);
  } else {
    if (id < 0) {
      // Negative ids correspond to precreated hosts. We should be able to
      // retrieve one, but currently we have a race between this IPC and
      // browser removing the corresponding frame and precreated AppCacheHost.
      // Instead of crashing the renderer or returning wrong host, we do not
      // bind any host and let renderer do nothing until it is destroyed by
      // the request from browser.
      return;
    }
    host = std::make_unique<AppCacheHost>(id, process_id(), render_frame_id,
                                          std::move(frontend), service_);
  }

  host->BindRequest(std::move(host_request));
  hosts_.emplace(std::piecewise_construct, std::forward_as_tuple(id),
                 std::forward_as_tuple(std::move(host)));
}

void AppCacheBackendImpl::UnregisterHost(int32_t id) {
  if (!hosts_.erase(id))
    mojo::ReportBadMessage("ACDH_UNREGISTER");
}

}  // namespace content
