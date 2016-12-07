// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_resource_throttle.h"

#include <utility>

#include "base/bind.h"
#include "build/build_config.h"
#include "chrome/browser/download/download_stats.h"
#include "content/public/browser/browser_thread.h"

#if defined(OS_ANDROID)
#include "chrome/browser/android/download/download_controller_base.h"
#include "content/public/browser/render_view_host.h"
#endif

using content::BrowserThread;

namespace {

void OnCanDownloadDecided(base::WeakPtr<DownloadResourceThrottle> throttle,
                          bool allow) {
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&DownloadResourceThrottle::ContinueDownload, throttle, allow));
}

void CanDownload(
    std::unique_ptr<DownloadResourceThrottle::DownloadRequestInfo> info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  info->limiter->CanDownload(info->web_contents_getter, info->url,
                             info->request_method, info->continue_callback);
}

#if defined(OS_ANDROID)
void OnAcquireFileAccessPermissionDone(
    std::unique_ptr<DownloadResourceThrottle::DownloadRequestInfo> info,
    bool granted) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (granted)
    CanDownload(std::move(info));
  else
    info->continue_callback.Run(false);
}
#endif

void CanDownloadOnUIThread(
    std::unique_ptr<DownloadResourceThrottle::DownloadRequestInfo> info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
#if defined(OS_ANDROID)
  content::WebContents* contents = info->web_contents_getter.Run();
  if (contents) {
    DownloadControllerBase::Get()->AcquireFileAccessPermission(
        contents, base::Bind(&OnAcquireFileAccessPermissionDone,
                             base::Passed(std::move(info))));
  } else {
    OnAcquireFileAccessPermissionDone(std::move(info), false);
  }
#else
  CanDownload(std::move(info));
#endif
}

}  // namespace

DownloadResourceThrottle::DownloadRequestInfo::DownloadRequestInfo(
    scoped_refptr<DownloadRequestLimiter> limiter,
    const content::ResourceRequestInfo::WebContentsGetter& web_contents_getter,
    const GURL& url,
    const std::string& request_method,
    const DownloadRequestLimiter::Callback& continue_callback)
    : limiter(limiter),
      web_contents_getter(web_contents_getter),
      url(url),
      request_method(request_method),
      continue_callback(continue_callback) {}

DownloadResourceThrottle::DownloadRequestInfo::~DownloadRequestInfo() {}

DownloadResourceThrottle::DownloadResourceThrottle(
    scoped_refptr<DownloadRequestLimiter> limiter,
    const content::ResourceRequestInfo::WebContentsGetter& web_contents_getter,
    const GURL& url,
    const std::string& request_method)
    : querying_limiter_(true),
      request_allowed_(false),
      request_deferred_(false) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&CanDownloadOnUIThread,
                 base::Passed(std::unique_ptr<DownloadRequestInfo>(
                     new DownloadRequestInfo(
                         limiter, web_contents_getter, url, request_method,
                         base::Bind(&OnCanDownloadDecided, AsWeakPtr()))))));
}

DownloadResourceThrottle::~DownloadResourceThrottle() {
}

void DownloadResourceThrottle::WillStartRequest(bool* defer) {
  WillDownload(defer);
}

void DownloadResourceThrottle::WillRedirectRequest(
    const net::RedirectInfo& redirect_info,
    bool* defer) {
  WillDownload(defer);
}

void DownloadResourceThrottle::WillProcessResponse(bool* defer) {
  WillDownload(defer);
}

const char* DownloadResourceThrottle::GetNameForLogging() const {
  return "DownloadResourceThrottle";
}

void DownloadResourceThrottle::WillDownload(bool* defer) {
  DCHECK(!request_deferred_);

  // Defer the download until we have the DownloadRequestLimiter result.
  if (querying_limiter_) {
    request_deferred_ = true;
    *defer = true;
    return;
  }

  if (!request_allowed_)
    Cancel();
}

void DownloadResourceThrottle::ContinueDownload(bool allow) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  querying_limiter_ = false;
  request_allowed_ = allow;

  if (allow) {
    // Presumes all downloads initiated by navigation use this throttle and
    // nothing else does.
    RecordDownloadSource(DOWNLOAD_INITIATED_BY_NAVIGATION);
  } else {
    RecordDownloadCount(CHROME_DOWNLOAD_COUNT_BLOCKED_BY_THROTTLING);
  }

  if (request_deferred_) {
    request_deferred_ = false;
    if (allow) {
      Resume();
    } else {
      Cancel();
    }
  }
}
