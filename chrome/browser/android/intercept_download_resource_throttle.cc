// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/intercept_download_resource_throttle.h"

#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/android/chrome_feature_list.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_headers.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/resource_controller.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"

using content::BrowserThread;

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

// static
bool InterceptDownloadResourceThrottle::IsDownloadInterceptionEnabled() {
  return base::FeatureList::IsEnabled(chrome::android::kSystemDownloadManager);
}

InterceptDownloadResourceThrottle::InterceptDownloadResourceThrottle(
    net::URLRequest* request,
    int render_process_id,
    int render_view_id,
    bool must_download)
    : request_(request),
      render_process_id_(render_process_id),
      render_view_id_(render_view_id),
      must_download_(must_download),
      weak_factory_(this) {
}

InterceptDownloadResourceThrottle::~InterceptDownloadResourceThrottle() {
}

void InterceptDownloadResourceThrottle::WillProcessResponse(bool* defer) {
  ProcessDownloadRequest(defer);
}

const char* InterceptDownloadResourceThrottle::GetNameForLogging() const {
  return "InterceptDownloadResourceThrottle";
}

void InterceptDownloadResourceThrottle::ProcessDownloadRequest(bool* defer) {
  if (!IsDownloadInterceptionEnabled())
    return;

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

  net::CookieStore* cookie_store = request_->context()->cookie_store();
  if (cookie_store) {
    // Cookie is obtained via asynchonous call. Setting |*defer| to true
    // keeps the throttle alive in the meantime.
    *defer = true;
    net::CookieOptions options;
    options.set_include_httponly();
    cookie_store->GetCookieListWithOptionsAsync(
        request_->url(),
        options,
        base::Bind(&InterceptDownloadResourceThrottle::CheckCookiePolicy,
                   weak_factory_.GetWeakPtr()));
  } else {
    // Can't get any cookies, start android download.
    StartDownload(DownloadInfo(request_));
  }
}

void InterceptDownloadResourceThrottle::CheckCookiePolicy(
    const net::CookieList& cookie_list) {
  DownloadInfo info(request_);
  if (request_->context()->network_delegate()->CanGetCookies(*request_,
                                                             cookie_list)) {
    std::string cookie = net::CookieStore::BuildCookieLine(cookie_list);
    if (!cookie.empty()) {
      info.cookie = cookie;
    }
  }
  StartDownload(info);
}

void InterceptDownloadResourceThrottle::StartDownload(
    const DownloadInfo& info) {
  DownloadControllerBase::Get()->CreateGETDownload(
      render_process_id_, render_view_id_, must_download_, info);
  controller()->Cancel();
  RecordInterceptFailureReasons(NO_FAILURE);
}

}  // namespace chrome
