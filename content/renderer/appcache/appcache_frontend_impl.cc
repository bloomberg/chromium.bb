// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/appcache/appcache_frontend_impl.h"

#include "content/public/common/service_names.mojom.h"
#include "content/public/renderer/render_thread.h"
#include "content/renderer/appcache/web_application_cache_host_impl.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "services/service_manager/public/cpp/connector.h"
#include "third_party/blink/public/mojom/appcache/appcache.mojom.h"
#include "third_party/blink/public/mojom/appcache/appcache_info.mojom.h"
#include "third_party/blink/public/web/web_console_message.h"

using blink::WebApplicationCacheHost;
using blink::WebConsoleMessage;

namespace content {

// Inline helper to keep the lines shorter and unwrapped.
inline WebApplicationCacheHostImpl* GetHost(int id) {
  return WebApplicationCacheHostImpl::FromId(id);
}

AppCacheFrontendImpl::AppCacheFrontendImpl() : binding_(this) {}
AppCacheFrontendImpl::~AppCacheFrontendImpl() = default;

void AppCacheFrontendImpl::Bind(blink::mojom::AppCacheFrontendRequest request) {
  binding_.Bind(std::move(request));
}

blink::mojom::AppCacheBackend* AppCacheFrontendImpl::backend_proxy() {
  if (!backend_ptr_) {
    RenderThread::Get()->GetConnector()->BindInterface(
        mojom::kBrowserServiceName, mojo::MakeRequest(&backend_ptr_));
  }
  return backend_ptr_.get();
}

void AppCacheFrontendImpl::CacheSelected(int32_t host_id,
                                         blink::mojom::AppCacheInfoPtr info) {
  WebApplicationCacheHostImpl* host = GetHost(host_id);
  if (host)
    host->OnCacheSelected(*info);
}

void AppCacheFrontendImpl::StatusChanged(const std::vector<int32_t>& host_ids,
                                         blink::mojom::AppCacheStatus status) {
  for (auto i = host_ids.begin(); i != host_ids.end(); ++i) {
    WebApplicationCacheHostImpl* host = GetHost(*i);
    if (host)
      host->OnStatusChanged(status);
  }
}

void AppCacheFrontendImpl::EventRaised(const std::vector<int32_t>& host_ids,
                                       blink::mojom::AppCacheEventID event_id) {
  DCHECK_NE(event_id,
            blink::mojom::AppCacheEventID::
                APPCACHE_PROGRESS_EVENT);  // See OnProgressEventRaised.
  DCHECK_NE(event_id,
            blink::mojom::AppCacheEventID::
                APPCACHE_ERROR_EVENT);  // See OnErrorEventRaised.
  for (auto i = host_ids.begin(); i != host_ids.end(); ++i) {
    WebApplicationCacheHostImpl* host = GetHost(*i);
    if (host)
      host->OnEventRaised(event_id);
  }
}

void AppCacheFrontendImpl::ProgressEventRaised(
    const std::vector<int32_t>& host_ids,
    const GURL& url,
    int32_t num_total,
    int32_t num_complete) {
  for (auto i = host_ids.begin(); i != host_ids.end(); ++i) {
    WebApplicationCacheHostImpl* host = GetHost(*i);
    if (host)
      host->OnProgressEventRaised(url, num_total, num_complete);
  }
}

void AppCacheFrontendImpl::ErrorEventRaised(
    const std::vector<int32_t>& host_ids,
    blink::mojom::AppCacheErrorDetailsPtr details) {
  for (auto i = host_ids.begin(); i != host_ids.end(); ++i) {
    WebApplicationCacheHostImpl* host = GetHost(*i);
    if (host)
      host->OnErrorEventRaised(*details);
  }
}

void AppCacheFrontendImpl::LogMessage(int32_t host_id,
                                      int32_t log_level,
                                      const std::string& message) {
  WebApplicationCacheHostImpl* host = GetHost(host_id);
  if (host)
    host->OnLogMessage(static_cast<AppCacheLogLevel>(log_level), message);
}

void AppCacheFrontendImpl::ContentBlocked(int32_t host_id,
                                          const GURL& manifest_url) {
  WebApplicationCacheHostImpl* host = GetHost(host_id);
  if (host)
    host->OnContentBlocked(manifest_url);
}

void AppCacheFrontendImpl::SetSubresourceFactory(
    int32_t host_id,
    network::mojom::URLLoaderFactoryPtr url_loader_factory) {
  WebApplicationCacheHostImpl* host = GetHost(host_id);
  if (host)
    host->SetSubresourceFactory(std::move(url_loader_factory));
}

// Ensure that enum values never get out of sync with the
// ones declared for use within the WebKit api

#define STATIC_ASSERT_ENUM(a, b)                            \
  static_assert(static_cast<int>(a) == static_cast<int>(b), \
                "mismatched enum: " #a)

STATIC_ASSERT_ENUM(WebApplicationCacheHost::kUncached,
                   blink::mojom::AppCacheStatus::APPCACHE_STATUS_UNCACHED);
STATIC_ASSERT_ENUM(WebApplicationCacheHost::kIdle,
                   blink::mojom::AppCacheStatus::APPCACHE_STATUS_IDLE);
STATIC_ASSERT_ENUM(WebApplicationCacheHost::kChecking,
                   blink::mojom::AppCacheStatus::APPCACHE_STATUS_CHECKING);
STATIC_ASSERT_ENUM(WebApplicationCacheHost::kDownloading,
                   blink::mojom::AppCacheStatus::APPCACHE_STATUS_DOWNLOADING);
STATIC_ASSERT_ENUM(WebApplicationCacheHost::kUpdateReady,
                   blink::mojom::AppCacheStatus::APPCACHE_STATUS_UPDATE_READY);
STATIC_ASSERT_ENUM(WebApplicationCacheHost::kObsolete,
                   blink::mojom::AppCacheStatus::APPCACHE_STATUS_OBSOLETE);

STATIC_ASSERT_ENUM(WebApplicationCacheHost::kCheckingEvent,
                   blink::mojom::AppCacheEventID::APPCACHE_CHECKING_EVENT);
STATIC_ASSERT_ENUM(WebApplicationCacheHost::kErrorEvent,
                   blink::mojom::AppCacheEventID::APPCACHE_ERROR_EVENT);
STATIC_ASSERT_ENUM(WebApplicationCacheHost::kNoUpdateEvent,
                   blink::mojom::AppCacheEventID::APPCACHE_NO_UPDATE_EVENT);
STATIC_ASSERT_ENUM(WebApplicationCacheHost::kDownloadingEvent,
                   blink::mojom::AppCacheEventID::APPCACHE_DOWNLOADING_EVENT);
STATIC_ASSERT_ENUM(WebApplicationCacheHost::kProgressEvent,
                   blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);
STATIC_ASSERT_ENUM(WebApplicationCacheHost::kUpdateReadyEvent,
                   blink::mojom::AppCacheEventID::APPCACHE_UPDATE_READY_EVENT);
STATIC_ASSERT_ENUM(WebApplicationCacheHost::kCachedEvent,
                   blink::mojom::AppCacheEventID::APPCACHE_CACHED_EVENT);
STATIC_ASSERT_ENUM(WebApplicationCacheHost::kObsoleteEvent,
                   blink::mojom::AppCacheEventID::APPCACHE_OBSOLETE_EVENT);

STATIC_ASSERT_ENUM(WebApplicationCacheHost::kManifestError,
                   blink::mojom::AppCacheErrorReason::APPCACHE_MANIFEST_ERROR);
STATIC_ASSERT_ENUM(WebApplicationCacheHost::kSignatureError,
                   blink::mojom::AppCacheErrorReason::APPCACHE_SIGNATURE_ERROR);
STATIC_ASSERT_ENUM(WebApplicationCacheHost::kResourceError,
                   blink::mojom::AppCacheErrorReason::APPCACHE_RESOURCE_ERROR);
STATIC_ASSERT_ENUM(WebApplicationCacheHost::kChangedError,
                   blink::mojom::AppCacheErrorReason::APPCACHE_CHANGED_ERROR);
STATIC_ASSERT_ENUM(WebApplicationCacheHost::kAbortError,
                   blink::mojom::AppCacheErrorReason::APPCACHE_ABORT_ERROR);
STATIC_ASSERT_ENUM(WebApplicationCacheHost::kQuotaError,
                   blink::mojom::AppCacheErrorReason::APPCACHE_QUOTA_ERROR);
STATIC_ASSERT_ENUM(WebApplicationCacheHost::kPolicyError,
                   blink::mojom::AppCacheErrorReason::APPCACHE_POLICY_ERROR);
STATIC_ASSERT_ENUM(WebApplicationCacheHost::kUnknownError,
                   blink::mojom::AppCacheErrorReason::APPCACHE_UNKNOWN_ERROR);

STATIC_ASSERT_ENUM(WebConsoleMessage::kLevelVerbose, APPCACHE_LOG_VERBOSE);
STATIC_ASSERT_ENUM(WebConsoleMessage::kLevelInfo, APPCACHE_LOG_INFO);
STATIC_ASSERT_ENUM(WebConsoleMessage::kLevelWarning, APPCACHE_LOG_WARNING);
STATIC_ASSERT_ENUM(WebConsoleMessage::kLevelError, APPCACHE_LOG_ERROR);

#undef STATIC_ASSERT_ENUM

}  // namespace content
