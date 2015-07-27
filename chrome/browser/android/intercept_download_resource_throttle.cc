// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/intercept_download_resource_throttle.h"

#include "base/metrics/histogram_macros.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_headers.h"
#include "content/public/browser/android/download_controller_android.h"
#include "content/public/browser/resource_controller.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request.h"

namespace {

// UMA histogram for tracking reasons that chrome fails to intercept the
// download. Keep this in sync with MobileDownloadInterceptFailureReasons in
// histograms.xml.
enum MobileDownloadInterceptFailureReason {
  NO_FAILURE = 0,
  EMPTY_URL,
  NON_HTTP_OR_HTTPS,
  NON_GET_METHODS,
  NO_REQUEST_HEADERS,
  USE_HTTP_AUTH,
  USE_CHANNEL_BOUND_COOKIES,
  // FAILURE_REASON_SIZE should always be last - this is a count of the number
  // of items in this enum.
  FAILURE_REASON_SIZE,
};

void RecordInterceptFailureReasons(
    MobileDownloadInterceptFailureReason reason) {
  UMA_HISTOGRAM_ENUMERATION("MobileDownload.InterceptFailureReason",
                            reason,
                            FAILURE_REASON_SIZE);
}

}  // namespace

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

void InterceptDownloadResourceThrottle::WillProcessResponse(bool* defer) {
  ProcessDownloadRequest();
}

const char* InterceptDownloadResourceThrottle::GetNameForLogging() const {
  return "InterceptDownloadResourceThrottle";
}

void InterceptDownloadResourceThrottle::ProcessDownloadRequest() {
  if (request_->url_chain().empty()) {
    RecordInterceptFailureReasons(EMPTY_URL);
    return;
  }

  GURL url = request_->url_chain().back();
  if (!url.SchemeIsHTTPOrHTTPS()) {
    RecordInterceptFailureReasons(NON_HTTP_OR_HTTPS);
    return;
  }

  if (request_->method() != net::HttpRequestHeaders::kGetMethod) {
    RecordInterceptFailureReasons(NON_GET_METHODS);
    return;
  }

  net::HttpRequestHeaders headers;
  if (!request_->GetFullRequestHeaders(&headers)) {
    RecordInterceptFailureReasons(NO_REQUEST_HEADERS);
    return;
  }

  // In general, if the request uses HTTP authorization, either with the origin
  // or a proxy, then the network stack should handle the download. The one
  // exception is a request that is fetched via the Chrome Proxy and does not
  // authenticate with the origin.
  if (request_->response_info().did_use_http_auth) {
    if (headers.HasHeader(net::HttpRequestHeaders::kAuthorization) ||
        !(request_->response_info().headers.get() &&
          data_reduction_proxy::HasDataReductionProxyViaHeader(
              request_->response_info().headers.get(), NULL))) {
      RecordInterceptFailureReasons(USE_HTTP_AUTH);
      return;
    }
  }

  // If the cookie is possibly channel-bound, don't pass it to android download
  // manager.
  // TODO(qinmin): add a test for this. http://crbug.com/430541.
  if (request_->ssl_info().channel_id_sent) {
    RecordInterceptFailureReasons(USE_CHANNEL_BOUND_COOKIES);
    return;
  }

  content::DownloadControllerAndroid::Get()->CreateGETDownload(
      render_process_id_, render_view_id_, request_id_);
  controller()->Cancel();
  RecordInterceptFailureReasons(NO_FAILURE);
}

}  // namespace chrome
