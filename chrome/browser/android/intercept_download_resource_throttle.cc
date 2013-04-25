// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/intercept_download_resource_throttle.h"

#include "content/public/browser/android/download_controller_android.h"
#include "content/public/browser/resource_controller.h"
#include "net/http/http_request_headers.h"
#include "net/url_request/url_request.h"

namespace chrome {

InterceptDownloadResourceThrottle::InterceptDownloadResourceThrottle(
    net::URLRequest* request,
    int render_process_id,
    int render_view_id,
    int request_id)
    : request_(request),
      render_process_id_(render_process_id),
      render_view_id_(render_view_id),
      request_id_(request_id) {
}

InterceptDownloadResourceThrottle::~InterceptDownloadResourceThrottle() {
}

void InterceptDownloadResourceThrottle::WillStartRequest(bool* defer) {
  ProcessDownloadRequest();
}

void InterceptDownloadResourceThrottle::WillProcessResponse(bool* defer) {
  ProcessDownloadRequest();
}

void InterceptDownloadResourceThrottle::ProcessDownloadRequest() {
  if (request_->method() != net::HttpRequestHeaders::kGetMethod ||
      request_->response_info().did_use_http_auth)
    return;

  if (request_->url_chain().empty())
    return;

  GURL url = request_->url_chain().back();
  if (!url.SchemeIs("http") && !url.SchemeIs("https"))
    return;

  content::DownloadControllerAndroid::Get()->CreateGETDownload(
      render_process_id_, render_view_id_, request_id_);
  controller()->Cancel();
}

}  // namespace
