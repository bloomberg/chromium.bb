// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/discovery/dial/device_description_fetcher.h"

#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/media/router/discovery/dial/dial_device_data.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/load_flags.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/http/http_util.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/redirect_info.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"

constexpr char kApplicationUrlHeaderName[] = "Application-URL";
constexpr int kMaxRetries = 3;
// DIAL devices are unlikely to expose uPnP functions other than DIAL, so 256kb
// should be more than sufficient.
constexpr int kMaxDescriptionSizeBytes = 262144;

namespace media_router {

namespace {

constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
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
          cookies_allowed: NO
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

void BindURLLoaderFactoryRequestOnUIThread(
    network::mojom::URLLoaderFactoryRequest request) {
  network::mojom::URLLoaderFactory* factory =
      g_browser_process->system_network_context_manager()
          ->GetURLLoaderFactory();
  factory->Clone(std::move(request));
}

}  // namespace

DeviceDescriptionFetcher::DeviceDescriptionFetcher(
    const GURL& device_description_url,
    base::OnceCallback<void(const DialDeviceDescriptionData&)> success_cb,
    base::OnceCallback<void(const std::string&)> error_cb)
    : device_description_url_(device_description_url),
      success_cb_(std::move(success_cb)),
      error_cb_(std::move(error_cb)) {
  DCHECK(device_description_url_.is_valid());
}

DeviceDescriptionFetcher::~DeviceDescriptionFetcher() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void DeviceDescriptionFetcher::Start() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!loader_);

  auto request = std::make_unique<network::ResourceRequest>();
  request->url = device_description_url_;

  // net::LOAD_BYPASS_PROXY: Proxies almost certainly hurt more cases than they
  //     help.
  // net::LOAD_DISABLE_CACHE: The request should not touch the cache.
  // net::LOAD_DO_NOT_{SAVE,SEND}_COOKIES: The request should not touch cookies.
  // net::LOAD_DO_NOT_SEND_AUTH_DATA: The request should not send auth data.
  request->load_flags = net::LOAD_BYPASS_PROXY | net::LOAD_DISABLE_CACHE |
                        net::LOAD_DO_NOT_SAVE_COOKIES |
                        net::LOAD_DO_NOT_SEND_COOKIES |
                        net::LOAD_DO_NOT_SEND_AUTH_DATA;

  loader_ =
      network::SimpleURLLoader::Create(std::move(request), kTrafficAnnotation);

  // Allow the fetcher to retry on 5XX responses and ERR_NETWORK_CHANGED.
  loader_->SetRetryOptions(
      kMaxRetries,
      network::SimpleURLLoader::RetryMode::RETRY_ON_5XX |
          network::SimpleURLLoader::RetryMode::RETRY_ON_NETWORK_CHANGE);

  // Section 5.4 of the DIAL spec prohibits redirects.
  // In practice, the callback will only get called once, since |loader_| will
  // be deleted on redirect.
  loader_->SetOnRedirectCallback(base::BindRepeating(
      &DeviceDescriptionFetcher::ReportRedirectError, base::Unretained(this)));

  StartDownload();
}

void DeviceDescriptionFetcher::StartDownload() {
  // Bind the request to the system URLLoaderFactory obtained on UI thread.
  // Currently this is the only way to guarantee a live URLLoaderFactory.
  // TOOD(mmenke): Figure out a way to do this transparently on IO thread.
  network::mojom::URLLoaderFactoryPtr loader_factory;
  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE,
      base::BindOnce(&BindURLLoaderFactoryRequestOnUIThread,
                     mojo::MakeRequest(&loader_factory)));
  loader_->DownloadToString(
      loader_factory.get(),
      base::BindOnce(&DeviceDescriptionFetcher::ProcessResponse,
                     base::Unretained(this)),
      kMaxDescriptionSizeBytes);
}

void DeviceDescriptionFetcher::ProcessResponse(
    std::unique_ptr<std::string> response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (loader_->NetError() != net::Error::OK) {
    ReportError(base::StringPrintf(
        "HTTP %d: Unable to fetch device description", loader_->NetError()));
    return;
  }

  if (!response || response->empty()) {
    ReportError("Missing or empty response");
    return;
  }

  if (!base::IsStringUTF8(*response)) {
    ReportError("Invalid response encoding");
    return;
  }

  const network::ResourceResponseHead* response_info = loader_->ResponseInfo();
  DCHECK(response_info);

  // NOTE: The uPnP spec requires devices to set a Content-Type: header of
  // text/xml; charset="utf-8" (sec 2.11).  However Chromecast (and possibly
  // other devices) do not comply, so specifically not checking this header.
  std::string app_url_header;
  if (!response_info->headers ||
      !response_info->headers->GetNormalizedHeader(kApplicationUrlHeaderName,
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

  // Remove trailing slash if there is any.
  if (app_url.ExtractFileName().empty()) {
    DVLOG(2) << "App url has trailing slash: " << app_url_header;
    app_url = GURL(app_url_header.substr(0, app_url_header.length() - 1));
  }

  std::move(success_cb_).Run(DialDeviceDescriptionData(*response, app_url));
}

void DeviceDescriptionFetcher::ReportRedirectError(
    const net::RedirectInfo& redirect_info,
    const network::ResourceResponseHead& response_head) {
  // Cancel the request.
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  loader_.reset();
  ReportError("Redirect not allowed");
}

void DeviceDescriptionFetcher::ReportError(const std::string& message) {
  std::move(error_cb_).Run(message);
}

}  // namespace media_router
