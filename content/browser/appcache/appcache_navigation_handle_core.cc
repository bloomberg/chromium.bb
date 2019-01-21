// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/appcache/appcache_navigation_handle_core.h"

#include <map>
#include <utility>

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "content/browser/appcache/appcache_host.h"
#include "content/browser/appcache/appcache_navigation_handle.h"
#include "content/browser/appcache/appcache_service_impl.h"
#include "content/browser/appcache/chrome_appcache_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/child_process_host.h"
#include "third_party/blink/public/mojom/appcache/appcache.mojom.h"
#include "third_party/blink/public/mojom/appcache/appcache_info.mojom.h"

namespace {

// Map of AppCache host id to the AppCacheNavigationHandleCore instance.
// Accessed on the IO thread only.
using AppCacheHandleMap =
    std::map <int, content::AppCacheNavigationHandleCore*>;
base::LazyInstance<AppCacheHandleMap>::DestructorAtExit g_appcache_handle_map =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

namespace content {

AppCacheNavigationHandleCore::AppCacheNavigationHandleCore(
    ChromeAppCacheService* appcache_service,
    int appcache_host_id,
    int process_id)
    : appcache_service_(appcache_service),
      appcache_host_id_(appcache_host_id),
      process_id_(process_id) {
  // The AppCacheNavigationHandleCore is created on the UI thread but
  // should only be accessed from the IO thread afterwards.
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

AppCacheNavigationHandleCore::~AppCacheNavigationHandleCore() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  precreated_host_.reset(nullptr);
  g_appcache_handle_map.Get().erase(appcache_host_id_);
}

void AppCacheNavigationHandleCore::Initialize() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(precreated_host_.get() == nullptr);
  precreated_host_.reset(new AppCacheHost(appcache_host_id_, process_id_, this,
                                          GetAppCacheService()));

  DCHECK(g_appcache_handle_map.Get().find(appcache_host_id_) ==
         g_appcache_handle_map.Get().end());
  g_appcache_handle_map.Get()[appcache_host_id_] = this;
}

// static
std::unique_ptr<AppCacheHost> AppCacheNavigationHandleCore::GetPrecreatedHost(
    int host_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  auto index = g_appcache_handle_map.Get().find(host_id);
  if (index != g_appcache_handle_map.Get().end()) {
    AppCacheNavigationHandleCore* instance = index->second;
    DCHECK(instance);
    return std::move(instance->precreated_host_);
  }
  return std::unique_ptr<AppCacheHost>();
}

AppCacheServiceImpl* AppCacheNavigationHandleCore::GetAppCacheService() {
  return static_cast<AppCacheServiceImpl*>(appcache_service_.get());
}

void AppCacheNavigationHandleCore::SetProcessId(int process_id) {
  DCHECK_EQ(process_id_, ChildProcessHost::kInvalidUniqueID);
  DCHECK_NE(process_id, ChildProcessHost::kInvalidUniqueID);
  DCHECK(precreated_host_);
  process_id_ = process_id;
  precreated_host_->SetProcessId(process_id);
}

void AppCacheNavigationHandleCore::OnCacheSelected(
    int host_id,
    const blink::mojom::AppCacheInfo& info) {
  DCHECK(false);
}

void AppCacheNavigationHandleCore::OnStatusChanged(
    const std::vector<int>& host_ids,
    blink::mojom::AppCacheStatus status) {
  // Should never be called.
  DCHECK(false);
}

void AppCacheNavigationHandleCore::OnEventRaised(
    const std::vector<int>& host_ids,
    blink::mojom::AppCacheEventID event_id) {
  // Should never be called.
  DCHECK(false);
}

void AppCacheNavigationHandleCore::OnProgressEventRaised(
    const std::vector<int>& host_ids,
    const GURL& url,
    int num_total,
    int num_complete) {
  // Should never be called.
  DCHECK(false);
}

void AppCacheNavigationHandleCore::OnErrorEventRaised(
    const std::vector<int>& host_ids,
    const blink::mojom::AppCacheErrorDetails& details) {
  // Should never be called.
  DCHECK(false);
}

void AppCacheNavigationHandleCore::OnLogMessage(int host_id,
                                                AppCacheLogLevel log_level,
                                                const std::string& message) {
  // Should never be called.
  DCHECK(false);
}

void AppCacheNavigationHandleCore::OnContentBlocked(int host_id,
                                                    const GURL& manifest_url) {
  // Should never be called.
  DCHECK(false);
}

void AppCacheNavigationHandleCore::OnSetSubresourceFactory(
    int host_id,
    network::mojom::URLLoaderFactoryPtr url_loader_factory) {
  // Should never be called.
  DCHECK(false);
}

}  // namespace content
