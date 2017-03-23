// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/discovery/dial/device_description_fetcher.h"

#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/media/router/discovery/dial/dial_device_data.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/load_flags.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/http/http_util.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_context_getter.h"

using content::BrowserThread;

constexpr char kApplicationUrlHeaderName[] = "Application-URL";
constexpr int kMaxRetries = 3;
// DIAL devices are unlikely to expose uPnP functions other than DIAL, so 256kb
// should be more than sufficient.
constexpr int kMaxDescriptionSizeBytes = 262144;

namespace media_router {

DeviceDescriptionFetcher::DeviceDescriptionFetcher(
    const GURL& device_description_url,
    net::URLRequestContextGetter* request_context,
    base::OnceCallback<void(const DialDeviceDescriptionData&)> success_cb,
    base::OnceCallback<void(const std::string&)> error_cb)
    : device_description_url_(device_description_url),
      request_context_(request_context),
      success_cb_(std::move(success_cb)),
      error_cb_(std::move(error_cb)) {
  DCHECK(request_context_);
  DCHECK(device_description_url_.is_valid());
}

DeviceDescriptionFetcher::~DeviceDescriptionFetcher() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void DeviceDescriptionFetcher::Start() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!fetcher_);

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("dial_get_device_description", R"(
        semantics {
          sender: "DIAL"
          description:
            "Chromium sends a request to a device (such as a smart TV) "
            "discovered via the DIAL (Disovery and Launch) protocol to obtain "
            "its device description. Chromium then uses the device description "
            "to determine the capabilities of the device to be used as a "
            "target for casting media content."
          trigger:
            "A new or updated device has been discovered via DIAL in the local "
            "network."
          data: "An HTTP GET request."
          destination: OTHER
          destination_other:
            "A device in the local network."
        }
        policy {
          cookies_allowed: false
          setting:
            "This feature cannot be disabled by settings and can only be "
            "disabled by media-router flag."
          chrome_policy {
            EnableMediaRouter {
              policy_options {mode: MANDATORY}
              EnableMediaRouter: false
            }
          }
        })");
  // DIAL returns device descriptions via GET request.
  fetcher_ =
      net::URLFetcher::Create(kURLFetcherIDForTest, device_description_url_,
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

void DeviceDescriptionFetcher::OnURLFetchComplete(
    const net::URLFetcher* source) {
  DCHECK_EQ(fetcher_.get(), source);

  if (source->GetResponseCode() != net::HTTP_OK) {
    ReportError(
        base::StringPrintf("HTTP %d: Unable to fetch device description",
                           source->GetResponseCode()));
    return;
  }

  const net::HttpResponseHeaders* headers = source->GetResponseHeaders();

  // NOTE: The uPnP spec requires devices to set a Content-Type: header of
  // text/xml; charset="utf-8" (sec 2.11).  However Chromecast (and possibly
  // other devices) do not comply, so specifically not checking this header.
  std::string app_url_header;
  if (!headers->GetNormalizedHeader(kApplicationUrlHeaderName,
                                    &app_url_header) ||
      app_url_header.empty()) {
    ReportError("Missing or empty Application-URL:");
    return;
  }

  // Section 5.4 of the DIAL spec implies that the Application URL should not
  // have path, query or fragment...unsure if that can be enforced.
  GURL app_url(app_url_header);
  if (!app_url.is_valid() || !app_url.SchemeIs("http") ||
      !app_url.HostIsIPAddress() ||
      app_url.host() != device_description_url_.host()) {
    ReportError(base::StringPrintf("Invalid Application-URL: %s",
                                   app_url_header.c_str()));
    return;
  }

  if (source->GetReceivedResponseContentLength() > kMaxDescriptionSizeBytes) {
    ReportError("Response too large");
    return;
  }

  std::string device_description;
  if (!source->GetResponseAsString(&device_description) ||
      device_description.empty()) {
    ReportError("Missing or empty response");
    return;
  }

  if (!base::IsStringUTF8(device_description)) {
    ReportError("Invalid response encoding");
    return;
  }

  std::move(success_cb_)
      .Run(DialDeviceDescriptionData(std::move(device_description), app_url));
}

void DeviceDescriptionFetcher::OnURLFetchDownloadProgress(
    const net::URLFetcher* source,
    int64_t current,
    int64_t total,
    int64_t current_network_bytes) {}

void DeviceDescriptionFetcher::OnURLFetchUploadProgress(
    const net::URLFetcher* source,
    int64_t current,
    int64_t total) {}

void DeviceDescriptionFetcher::ReportError(const std::string& message) {
  std::move(error_cb_).Run(message);
}

}  // namespace media_router
