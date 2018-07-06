// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/appcache/appcache_navigation_handle_core.h"

#include <map>
#include <utility>

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/strings/stringprintf.h"
#include "content/browser/appcache/appcache_host.h"
#include "content/browser/appcache/appcache_navigation_handle.h"
#include "content/browser/appcache/appcache_service_impl.h"
#include "content/browser/appcache/chrome_appcache_service.h"
#include "content/common/service_worker/service_worker_utils.h"
#include "content/public/browser/browser_thread.h"

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
    base::WeakPtr<AppCacheNavigationHandle> ui_handle,
    ChromeAppCacheService* appcache_service,
    int appcache_host_id)
    : appcache_service_(appcache_service),
      appcache_host_id_(appcache_host_id),
      ui_handle_(ui_handle) {
  if (ServiceWorkerUtils::IsServicificationEnabled())
    debug_log_ = base::make_optional<std::vector<std::string>>();

  // The AppCacheNavigationHandleCore is created on the UI thread but
  // should only be accessed from the IO thread afterwards.
  // Temporary CHECK for https://crbug.com/857005.
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

AppCacheNavigationHandleCore::~AppCacheNavigationHandleCore() {
  // Temporary CHECK for https://crbug.com/857005.
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  precreated_host_.reset(nullptr);
  g_appcache_handle_map.Get().erase(appcache_host_id_);
}

void AppCacheNavigationHandleCore::Initialize() {
  // Temporary CHECK for https://crbug.com/857005.
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  CHECK(precreated_host_.get() == nullptr);
  precreated_host_.reset(
      new AppCacheHost(appcache_host_id_, this, GetAppCacheService()));

  CHECK(g_appcache_handle_map.Get().find(appcache_host_id_) ==
        g_appcache_handle_map.Get().end());
  g_appcache_handle_map.Get()[appcache_host_id_] = this;

  if (debug_log_)
    debug_log_->emplace_back("Init:" + std::to_string(appcache_host_id_));
}

// static
std::unique_ptr<AppCacheHost> AppCacheNavigationHandleCore::GetPrecreatedHost(
    int host_id) {
  // Temporary CHECK for https://crbug.com/857005.
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  auto index = g_appcache_handle_map.Get().find(host_id);
  if (index != g_appcache_handle_map.Get().end()) {
    AppCacheNavigationHandleCore* instance = index->second;
    DCHECK(instance);
    auto host = std::move(instance->precreated_host_);
    if (instance->debug_log_) {
      instance->debug_log_->emplace_back(
          host ? base::StringPrintf("Get:id=%d,host=%d", host_id,
                                    host->host_id())
               : base::StringPrintf("Get:id=%d,host=null", host_id));
    }
    return host;
  }
  return std::unique_ptr<AppCacheHost>();
}

AppCacheServiceImpl* AppCacheNavigationHandleCore::GetAppCacheService() {
  return static_cast<AppCacheServiceImpl*>(appcache_service_.get());
}

void AppCacheNavigationHandleCore::AddRequestToDebugLog(const GURL& url) {
  if (debug_log_) {
    CHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    debug_log_->emplace_back("Req:host=" + HostToString() +
                             ",url=" + url.spec().substr(0, 64));
  }
}

void AppCacheNavigationHandleCore::AddDefaultFactoryRunToDebugLog(
    bool was_request_intercepted) {
  if (debug_log_) {
    CHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    debug_log_->emplace_back(
        base::StringPrintf("Fac:host=%s,int=%s", HostToString().c_str(),
                           was_request_intercepted ? "T" : "F"));
  }
}

void AppCacheNavigationHandleCore::AddCreateURLLoaderToDebugLog() {
  if (debug_log_) {
    CHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    debug_log_->emplace_back("Load:host=" + HostToString());
  }
}

void AppCacheNavigationHandleCore::AddNavigationStartToDebugLog() {
  if (debug_log_) {
    CHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    debug_log_->emplace_back("Start:host=" + HostToString());
  }
}

std::string AppCacheNavigationHandleCore::GetDebugLog() {
  if (!debug_log_)
    return "debug log is not enabled";
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  std::string log;
  for (const auto& event : *debug_log_)
    log += event + " ";
  return log;
}

void AppCacheNavigationHandleCore::OnCacheSelected(int host_id,
                                                   const AppCacheInfo& info) {
  DCHECK(false);
}

void AppCacheNavigationHandleCore::OnStatusChanged(
    const std::vector<int>& host_ids,
    AppCacheStatus status) {
  // Should never be called.
  DCHECK(false);
}

void AppCacheNavigationHandleCore::OnEventRaised(
    const std::vector<int>& host_ids,
    AppCacheEventID event_id) {
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
    const AppCacheErrorDetails& details) {
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

std::string AppCacheNavigationHandleCore::HostToString() {
  return precreated_host_ ? std::to_string(precreated_host_->host_id())
                          : "null";
}

}  // namespace content
