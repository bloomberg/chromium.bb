// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/discovery/dial/dial_url_fetcher.h"

#include "chrome/browser/browser_process.h"
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

constexpr int kMaxRetries = 3;
// DIAL devices are unlikely to expose uPnP functions other than DIAL, so 256kb
// should be more than sufficient.
constexpr int kMaxResponseSizeBytes = 262144;

namespace media_router {

namespace {

constexpr net::NetworkTrafficAnnotationTag kDialUrlFetcherTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("dial_url_fetcher", R"(
        semantics {
          sender: "DIAL"
          description:
            "Chromium sends a request to a device (such as a smart TV) "
            "discovered via the DIAL (Discovery and Launch) protocol to obtain "
            "its device description or app info data. Chromium then uses the "
            "data to determine the capabilities of the device to be used as a "
            "targetfor casting media content."
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

void BindURLLoaderFactoryRequestOnUIThread(
    network::mojom::URLLoaderFactoryRequest request) {
  network::mojom::URLLoaderFactory* factory =
      g_browser_process->system_network_context_manager()
          ->GetURLLoaderFactory();
  factory->Clone(std::move(request));
}

}  // namespace

DialURLFetcher::DialURLFetcher(
    const GURL& url,
    base::OnceCallback<void(const std::string&)> success_cb,
    base::OnceCallback<void(int, const std::string&)> error_cb)
    : url_(url),
      success_cb_(std::move(success_cb)),
      error_cb_(std::move(error_cb)) {
  DCHECK(url_.is_valid());
}

DialURLFetcher::~DialURLFetcher() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

const network::ResourceResponseHead* DialURLFetcher::GetResponseHead() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return loader_ ? loader_->ResponseInfo() : nullptr;
}

void DialURLFetcher::Start() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!loader_);

  auto request = std::make_unique<network::ResourceRequest>();
  request->url = url_;

  // net::LOAD_BYPASS_PROXY: Proxies almost certainly hurt more cases than they
  //     help.
  // net::LOAD_DISABLE_CACHE: The request should not touch the cache.
  // net::LOAD_DO_NOT_{SAVE,SEND}_COOKIES: The request should not touch cookies.
  // net::LOAD_DO_NOT_SEND_AUTH_DATA: The request should not send auth data.
  request->load_flags = net::LOAD_BYPASS_PROXY | net::LOAD_DISABLE_CACHE |
                        net::LOAD_DO_NOT_SAVE_COOKIES |
                        net::LOAD_DO_NOT_SEND_COOKIES |
                        net::LOAD_DO_NOT_SEND_AUTH_DATA;

  loader_ = network::SimpleURLLoader::Create(std::move(request),
                                             kDialUrlFetcherTrafficAnnotation);

  // Allow the fetcher to retry on 5XX responses and ERR_NETWORK_CHANGED.
  loader_->SetRetryOptions(
      kMaxRetries,
      network::SimpleURLLoader::RetryMode::RETRY_ON_5XX |
          network::SimpleURLLoader::RetryMode::RETRY_ON_NETWORK_CHANGE);

  // Section 5.4 of the DIAL spec prohibits redirects.
  // In practice, the callback will only get called once, since |loader_| will
  // be deleted.
  loader_->SetOnRedirectCallback(base::BindRepeating(
      &DialURLFetcher::ReportRedirectError, base::Unretained(this)));

  StartDownload();
}

void DialURLFetcher::ReportError(int response_code,
                                 const std::string& message) {
  std::move(error_cb_).Run(response_code, message);
}

void DialURLFetcher::ReportRedirectError(
    const net::RedirectInfo& redirect_info,
    const network::ResourceResponseHead& response_head) {
  // Cancel the request.
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  loader_.reset();

  // Returning |OK| on error will be treated as unavailable.
  ReportError(net::Error::OK, "Redirect not allowed");
}

void DialURLFetcher::StartDownload() {
  // Bind the request to the system URLLoaderFactory obtained on UI thread.
  // Currently this is the only way to guarantee a live URLLoaderFactory.
  // TOOD(mmenke): Figure out a way to do this transparently on IO thread.
  network::mojom::URLLoaderFactoryPtr loader_factory;

  // TODO(https://crbug.com/823869): Fix DeviceDescriptionServiceTest and remove
  // this conditional.
  auto mojo_request = mojo::MakeRequest(&loader_factory);
  if (content::BrowserThread::IsThreadInitialized(content::BrowserThread::UI)) {
    content::BrowserThread::PostTask(
        content::BrowserThread::UI, FROM_HERE,
        base::BindOnce(&BindURLLoaderFactoryRequestOnUIThread,
                       std::move(mojo_request)));
  }

  loader_->DownloadToString(
      loader_factory.get(),
      base::BindOnce(&DialURLFetcher::ProcessResponse, base::Unretained(this)),
      kMaxResponseSizeBytes);
}

void DialURLFetcher::ProcessResponse(std::unique_ptr<std::string> response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  int response_code = loader_->NetError();
  if (response_code != net::Error::OK) {
    ReportError(response_code,
                base::StringPrintf("HTTP %d: Unable to fetch DIAL app info",
                                   response_code));
    return;
  }

  if (!response || response->empty()) {
    ReportError(response_code, "Missing or empty response");
    return;
  }

  if (!base::IsStringUTF8(*response)) {
    ReportError(response_code, "Invalid response encoding");
    return;
  }

  std::move(success_cb_).Run(*response);
}

}  // namespace media_router
