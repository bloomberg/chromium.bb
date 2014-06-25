// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/appcache/appcache_frontend_impl.h"

#include "base/logging.h"
#include "content/child/appcache/web_application_cache_host_impl.h"
#include "third_party/WebKit/public/web/WebConsoleMessage.h"

using blink::WebApplicationCacheHost;
using blink::WebConsoleMessage;

namespace content {

// Inline helper to keep the lines shorter and unwrapped.
inline WebApplicationCacheHostImpl* GetHost(int id) {
  return WebApplicationCacheHostImpl::FromId(id);
}

void AppCacheFrontendImpl::OnCacheSelected(int host_id,
                                           const AppCacheInfo& info) {
  WebApplicationCacheHostImpl* host = GetHost(host_id);
  if (host)
    host->OnCacheSelected(info);
}

void AppCacheFrontendImpl::OnStatusChanged(const std::vector<int>& host_ids,
                                           AppCacheStatus status) {
  for (std::vector<int>::const_iterator i = host_ids.begin();
       i != host_ids.end(); ++i) {
    WebApplicationCacheHostImpl* host = GetHost(*i);
    if (host)
      host->OnStatusChanged(status);
  }
}

void AppCacheFrontendImpl::OnEventRaised(const std::vector<int>& host_ids,
                                         AppCacheEventID event_id) {
  DCHECK(event_id !=
         APPCACHE_PROGRESS_EVENT);  // See OnProgressEventRaised.
  DCHECK(event_id != APPCACHE_ERROR_EVENT); // See OnErrorEventRaised.
  for (std::vector<int>::const_iterator i = host_ids.begin();
       i != host_ids.end(); ++i) {
    WebApplicationCacheHostImpl* host = GetHost(*i);
    if (host)
      host->OnEventRaised(event_id);
  }
}

void AppCacheFrontendImpl::OnProgressEventRaised(
    const std::vector<int>& host_ids,
    const GURL& url,
    int num_total,
    int num_complete) {
  for (std::vector<int>::const_iterator i = host_ids.begin();
       i != host_ids.end(); ++i) {
    WebApplicationCacheHostImpl* host = GetHost(*i);
    if (host)
      host->OnProgressEventRaised(url, num_total, num_complete);
  }
}

void AppCacheFrontendImpl::OnErrorEventRaised(
    const std::vector<int>& host_ids,
    const AppCacheErrorDetails& details) {
  for (std::vector<int>::const_iterator i = host_ids.begin();
       i != host_ids.end(); ++i) {
    WebApplicationCacheHostImpl* host = GetHost(*i);
    if (host)
      host->OnErrorEventRaised(details);
  }
}

void AppCacheFrontendImpl::OnLogMessage(int host_id,
                                        AppCacheLogLevel log_level,
                                        const std::string& message) {
  WebApplicationCacheHostImpl* host = GetHost(host_id);
  if (host)
    host->OnLogMessage(log_level, message);
}

void AppCacheFrontendImpl::OnContentBlocked(int host_id,
                                            const GURL& manifest_url) {
  WebApplicationCacheHostImpl* host = GetHost(host_id);
  if (host)
    host->OnContentBlocked(manifest_url);
}

// Ensure that enum values never get out of sync with the
// ones declared for use within the WebKit api
COMPILE_ASSERT((int)WebApplicationCacheHost::Uncached ==
               (int)APPCACHE_STATUS_UNCACHED, Uncached);
COMPILE_ASSERT((int)WebApplicationCacheHost::Idle ==
               (int)APPCACHE_STATUS_IDLE, Idle);
COMPILE_ASSERT((int)WebApplicationCacheHost::Checking ==
               (int)APPCACHE_STATUS_CHECKING, Checking);
COMPILE_ASSERT((int)WebApplicationCacheHost::Downloading ==
               (int)APPCACHE_STATUS_DOWNLOADING, Downloading);
COMPILE_ASSERT((int)WebApplicationCacheHost::UpdateReady ==
               (int)APPCACHE_STATUS_UPDATE_READY, UpdateReady);
COMPILE_ASSERT((int)WebApplicationCacheHost::Obsolete ==
               (int)APPCACHE_STATUS_OBSOLETE, Obsolete);

COMPILE_ASSERT((int)WebApplicationCacheHost::CheckingEvent ==
               (int)APPCACHE_CHECKING_EVENT, CheckingEvent);
COMPILE_ASSERT((int)WebApplicationCacheHost::ErrorEvent ==
               (int)APPCACHE_ERROR_EVENT, ErrorEvent);
COMPILE_ASSERT((int)WebApplicationCacheHost::NoUpdateEvent ==
               (int)APPCACHE_NO_UPDATE_EVENT, NoUpdateEvent);
COMPILE_ASSERT((int)WebApplicationCacheHost::DownloadingEvent ==
               (int)APPCACHE_DOWNLOADING_EVENT, DownloadingEvent);
COMPILE_ASSERT((int)WebApplicationCacheHost::ProgressEvent ==
               (int)APPCACHE_PROGRESS_EVENT, ProgressEvent);
COMPILE_ASSERT((int)WebApplicationCacheHost::UpdateReadyEvent ==
               (int)APPCACHE_UPDATE_READY_EVENT, UpdateReadyEvent);
COMPILE_ASSERT((int)WebApplicationCacheHost::CachedEvent ==
               (int)APPCACHE_CACHED_EVENT, CachedEvent);
COMPILE_ASSERT((int)WebApplicationCacheHost::ObsoleteEvent ==
               (int)APPCACHE_OBSOLETE_EVENT, ObsoleteEvent);

COMPILE_ASSERT((int)WebConsoleMessage::LevelDebug ==
               (int)APPCACHE_LOG_DEBUG, LevelDebug);
COMPILE_ASSERT((int)WebConsoleMessage::LevelLog ==
               (int)APPCACHE_LOG_INFO, LevelLog);
COMPILE_ASSERT((int)WebConsoleMessage::LevelWarning ==
               (int)APPCACHE_LOG_WARNING, LevelWarning);
COMPILE_ASSERT((int)WebConsoleMessage::LevelError ==
               (int)APPCACHE_LOG_ERROR, LevelError);

COMPILE_ASSERT((int)WebApplicationCacheHost::ManifestError ==
                   (int)APPCACHE_MANIFEST_ERROR,
               ManifestError);
COMPILE_ASSERT((int)WebApplicationCacheHost::SignatureError ==
                   (int)APPCACHE_SIGNATURE_ERROR,
               SignatureError);
COMPILE_ASSERT((int)WebApplicationCacheHost::ResourceError ==
                   (int)APPCACHE_RESOURCE_ERROR,
               ResourceError);
COMPILE_ASSERT((int)WebApplicationCacheHost::ChangedError ==
                   (int)APPCACHE_CHANGED_ERROR,
               ChangedError);
COMPILE_ASSERT((int)WebApplicationCacheHost::AbortError ==
                   (int)APPCACHE_ABORT_ERROR,
               AbortError);
COMPILE_ASSERT((int)WebApplicationCacheHost::QuotaError ==
                   (int)APPCACHE_QUOTA_ERROR,
               QuotaError);
COMPILE_ASSERT((int)WebApplicationCacheHost::PolicyError ==
                   (int)APPCACHE_POLICY_ERROR,
               PolicyError);
COMPILE_ASSERT((int)WebApplicationCacheHost::UnknownError ==
                   (int)APPCACHE_UNKNOWN_ERROR,
               UnknownError);

}  // namespace content
