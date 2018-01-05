// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/discovery/dial/dial_app_info_fetcher.h"

#include "chrome/browser/profiles/profile.h"
#include "net/base/load_flags.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/http/http_util.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_context_getter.h"

using content::BrowserThread;

constexpr int kMaxRetries = 3;
// DIAL devices are unlikely to expose uPnP functions other than DIAL, so 256kb
// should be more than sufficient.
constexpr int kMaxAppInfoSizeBytes = 262144;

namespace media_router {

DialAppInfoFetcher::DialAppInfoFetcher(
    const GURL& app_url,
    net::URLRequestContextGetter* request_context,
    base::OnceCallback<void(const std::string&)> success_cb,
    base::OnceCallback<void(int, const std::string&)> error_cb)
    : app_url_(app_url),
      request_context_(request_context),
      success_cb_(std::move(success_cb)),
      error_cb_(std::move(error_cb)) {
  DCHECK(request_context_);
  DCHECK(app_url_.is_valid());
}

DialAppInfoFetcher::~DialAppInfoFetcher() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

void DialAppInfoFetcher::Start() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!fetcher_);

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("dial_get_app_info", R"(
        semantics {
          sender: "DIAL"
          description:
            "Chromium sends a request to a device (such as a smart TV) "
            "discovered via the DIAL (Discovery and Launch) protocol to obtain "
            "its app info data. Chromium then uses the app info data to"
            "determine the capabilities of the device to be used as a target"
            "for casting media content."
          trigger:
            "A new or updated device has been discovered via DIAL in the local "
            "network."
          data: "An HTTP GET request."
          destination: OTHER
          destination_other:
            "A device in the local network."
        }
        policy {
          cookies_allowed: NO
          setting:
            "This feature cannot be disabled by settings."
          chrome_policy {
            EnableMediaRouter {
              policy_options {mode: MANDATORY}
              EnableMediaRouter: false
            }
          }
        })");
  // DIAL returns app info via GET request.
  fetcher_ =
      net::URLFetcher::Create(kURLFetcherIDForTest, app_url_,
                              net::URLFetcher::GET, this, traffic_annotation);

  // net::LOAD_BYPASS_PROXY: Proxies almost certainly hurt more cases than they
  //     help.
  // net::LOAD_DISABLE_CACHE: The request should not touch the cache.
  // net::LOAD_DO_NOT_{SAVE,SEND}_COOKIES: The request should not touch cookies.
  // net::LOAD_DO_NOT_SEND_AUTH_DATA: The request should not send auth data.
  fetcher_->SetLoadFlags(net::LOAD_BYPASS_PROXY | net::LOAD_DISABLE_CACHE |
                         net::LOAD_DO_NOT_SAVE_COOKIES |
                         net::LOAD_DO_NOT_SEND_COOKIES |
                         net::LOAD_DO_NOT_SEND_AUTH_DATA);

  // Section 5.4 of the DIAL spec prohibits redirects.
  fetcher_->SetStopOnRedirect(true);

  // Allow the fetcher to retry on 5XX responses and ERR_NETWORK_CHANGED.
  fetcher_->SetMaxRetriesOn5xx(kMaxRetries);
  fetcher_->SetAutomaticallyRetryOnNetworkChanges(kMaxRetries);

  fetcher_->SetRequestContext(request_context_.get());
  fetcher_->Start();
}

void DialAppInfoFetcher::ReportError(int response_code,
                                     const std::string& message) {
  std::move(error_cb_).Run(response_code, message);
}

void DialAppInfoFetcher::OnURLFetchComplete(const net::URLFetcher* source) {
  DCHECK_EQ(fetcher_.get(), source);

  int response_code = source->GetResponseCode();
  if (response_code != net::HTTP_OK) {
    ReportError(response_code,
                base::StringPrintf("HTTP %d: Unable to fetch DIAL app info",
                                   response_code));
    return;
  }

  if (source->GetReceivedResponseContentLength() > kMaxAppInfoSizeBytes) {
    ReportError(response_code, "Response too large");
    return;
  }

  std::string app_info_xml;
  if (!source->GetResponseAsString(&app_info_xml) || app_info_xml.empty()) {
    ReportError(response_code, "Missing or empty response");
    return;
  }

  if (!base::IsStringUTF8(app_info_xml)) {
    ReportError(response_code, "Invalid response encoding");
    return;
  }

  std::move(success_cb_).Run(std::string(app_info_xml));
}

}  // namespace media_router
