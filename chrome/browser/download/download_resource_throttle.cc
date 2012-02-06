// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_resource_throttle.h"

#include "base/bind.h"
#include "content/public/browser/resource_throttle_controller.h"

DownloadResourceThrottle::DownloadResourceThrottle(
    DownloadRequestLimiter* limiter,
    int render_process_id,
    int render_view_id,
    int request_id)
    : request_allowed_(false),
      request_deferred_(false) {
  limiter->CanDownloadOnIOThread(
      render_process_id,
      render_view_id,
      request_id,
      base::Bind(&DownloadResourceThrottle::ContinueDownload,
                 AsWeakPtr()));
}

DownloadResourceThrottle::~DownloadResourceThrottle() {
}

void DownloadResourceThrottle::WillStartRequest(bool* defer) {
  *defer = request_deferred_ = !request_allowed_;
}

void DownloadResourceThrottle::WillRedirectRequest(const GURL& new_url,
                                                   bool* defer) {
  *defer = request_deferred_ = !request_allowed_;
}

void DownloadResourceThrottle::WillReadRequest(bool* defer) {
  *defer = request_deferred_ = !request_allowed_;
}

void DownloadResourceThrottle::ContinueDownload(bool allow) {
  request_allowed_ = allow;

  if (request_deferred_) {
    if (allow) {
      controller()->Resume();
    } else {
      controller()->Cancel();
    }
  }
}
