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

#define STATIC_ASSERT_ENUM(a, b, desc) \
    static_assert((int)a == (int)b, desc)

// Confirm that WebApplicationCacheHost::<a> == APPCACHE_<b>.
#define STATIC_ASSERT_WAC_ENUM(a, b) \
    STATIC_ASSERT_ENUM(WebApplicationCacheHost:: a, APPCACHE_##b, #a)

STATIC_ASSERT_WAC_ENUM(Uncached,    STATUS_UNCACHED);
STATIC_ASSERT_WAC_ENUM(Idle,        STATUS_IDLE);
STATIC_ASSERT_WAC_ENUM(Checking,    STATUS_CHECKING);
STATIC_ASSERT_WAC_ENUM(Downloading, STATUS_DOWNLOADING);
STATIC_ASSERT_WAC_ENUM(UpdateReady, STATUS_UPDATE_READY);
STATIC_ASSERT_WAC_ENUM(Obsolete,    STATUS_OBSOLETE);

STATIC_ASSERT_WAC_ENUM(CheckingEvent,    CHECKING_EVENT);
STATIC_ASSERT_WAC_ENUM(ErrorEvent,       ERROR_EVENT);
STATIC_ASSERT_WAC_ENUM(NoUpdateEvent,    NO_UPDATE_EVENT);
STATIC_ASSERT_WAC_ENUM(DownloadingEvent, DOWNLOADING_EVENT);
STATIC_ASSERT_WAC_ENUM(ProgressEvent,    PROGRESS_EVENT);
STATIC_ASSERT_WAC_ENUM(UpdateReadyEvent, UPDATE_READY_EVENT);
STATIC_ASSERT_WAC_ENUM(CachedEvent,      CACHED_EVENT);
STATIC_ASSERT_WAC_ENUM(ObsoleteEvent,    OBSOLETE_EVENT);

STATIC_ASSERT_WAC_ENUM(ManifestError,  MANIFEST_ERROR);
STATIC_ASSERT_WAC_ENUM(SignatureError, SIGNATURE_ERROR);
STATIC_ASSERT_WAC_ENUM(ResourceError,  RESOURCE_ERROR);
STATIC_ASSERT_WAC_ENUM(ChangedError,   CHANGED_ERROR);
STATIC_ASSERT_WAC_ENUM(AbortError,     ABORT_ERROR);
STATIC_ASSERT_WAC_ENUM(QuotaError,     QUOTA_ERROR);
STATIC_ASSERT_WAC_ENUM(PolicyError,    POLICY_ERROR);
STATIC_ASSERT_WAC_ENUM(UnknownError,   UNKNOWN_ERROR);

// Confirm that WebConsoleMessage::<a> == APPCACHE_<b>.
#define STATIC_ASSERT_WCM_ENUM(a, b) \
    STATIC_ASSERT_ENUM(WebConsoleMessage:: a, APPCACHE_##b, #a)

STATIC_ASSERT_WCM_ENUM(LevelDebug,   LOG_DEBUG);
STATIC_ASSERT_WCM_ENUM(LevelLog,     LOG_INFO);
STATIC_ASSERT_WCM_ENUM(LevelWarning, LOG_WARNING);
STATIC_ASSERT_WCM_ENUM(LevelError,   LOG_ERROR);

}  // namespace content
