// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/diagnosticsd/diagnosticsd_web_request_service.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "chrome/browser/chromeos/diagnosticsd/mojo_utils.h"
#include "mojo/public/cpp/system/buffer.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/base/url_util.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/url_constants.h"

namespace chromeos {

// Maximum size of the |request_queue_|.
const int kDiagnosticsdWebRequestQueueMaxSize = 10;

// Maximum size of the web response body.
const int kDiagnosticsdWebResponseMaxSizeInBytes = 1000000;

namespace {

// Converts mojo HTTP method into string.
std::string GetHttpMethod(
    diagnosticsd::mojom::DiagnosticsdWebRequestHttpMethod http_method) {
  switch (http_method) {
    case diagnosticsd::mojom::DiagnosticsdWebRequestHttpMethod::kGet:
      return "GET";
    case diagnosticsd::mojom::DiagnosticsdWebRequestHttpMethod::kHead:
      return "HEAD";
    case diagnosticsd::mojom::DiagnosticsdWebRequestHttpMethod::kPost:
      return "POST";
    case diagnosticsd::mojom::DiagnosticsdWebRequestHttpMethod::kPut:
      return "PUT";
  }
}

// Returns true in case of non-error 2xx HTTP status code.
bool IsHttpOkCode(int code) {
  return 200 <= code && code < 300;
}

}  //  namespace

DiagnosticsdWebRequestService::DiagnosticsdWebRequestService(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : url_loader_factory_(std::move(url_loader_factory)) {
  DCHECK(url_loader_factory_);
}

DiagnosticsdWebRequestService::~DiagnosticsdWebRequestService() {
  if (active_request_) {
    std::move(active_request_->callback)
        .Run(diagnosticsd::mojom::DiagnosticsdWebRequestStatus::kNetworkError,
             0 /* http_status */, mojo::ScopedHandle() /* response_body */);
    active_request_.reset();
  }
  while (!request_queue_.empty()) {
    auto request = std::move(request_queue_.front());
    request_queue_.pop();
    std::move(request->callback)
        .Run(diagnosticsd::mojom::DiagnosticsdWebRequestStatus::kNetworkError,
             0 /* http_status */, mojo::ScopedHandle() /* response_body */);
  }
  DCHECK(!active_request_);
}

void DiagnosticsdWebRequestService::PerformRequest(
    diagnosticsd::mojom::DiagnosticsdWebRequestHttpMethod http_method,
    GURL url,
    std::vector<base::StringPiece> headers,
    std::string request_body,
    PerformWebRequestCallback callback) {
  // Fail with the kNetworkError if the queue overflows.
  if (request_queue_.size() == kDiagnosticsdWebRequestQueueMaxSize) {
    LOG(ERROR) << "Too many incomplete requests in the diagnosticsd web request"
               << " queue.";
    std::move(callback).Run(
        diagnosticsd::mojom::DiagnosticsdWebRequestStatus::kNetworkError,
        0 /* http_status */, mojo::ScopedHandle() /* response_body */);
    return;
  }

  // Fail with kNetworkError if the |url| is invalid.
  if (!url.is_valid()) {
    LOG(ERROR) << "Diagnosticsd web request URL is invalid.";
    std::move(callback).Run(
        diagnosticsd::mojom::DiagnosticsdWebRequestStatus::kNetworkError,
        0 /* http_status */, mojo::ScopedHandle() /* response_body */);
    return;
  }

  // Fail with kNetworkError for non-HTTPs URL.
  if (!url.SchemeIs(url::kHttpsScheme)) {
    LOG(ERROR) << "Diagnosticsd web request URL must have a HTTPS scheme.";
    std::move(callback).Run(
        diagnosticsd::mojom::DiagnosticsdWebRequestStatus::kNetworkError,
        0 /* http_status */, mojo::ScopedHandle() /* response_body */);
    return;
  }

  // request_body must be empty for GET and HEAD HTTP methods.
  if (!request_body.empty() &&
      (http_method ==
           diagnosticsd::mojom::DiagnosticsdWebRequestHttpMethod::kGet ||
       http_method ==
           diagnosticsd::mojom::DiagnosticsdWebRequestHttpMethod::kHead)) {
    LOG(ERROR) << "Incorrect diagnosticsd web request format: require an empty "
               << "request body for GET and HEAD HTTP methods.";
    std::move(callback).Run(
        diagnosticsd::mojom::DiagnosticsdWebRequestStatus::kNetworkError,
        0 /* http_status */, mojo::ScopedHandle() /*response_body */);
    return;
  }

  // Do not allow local requests.
  if (net::IsLocalhost(url)) {
    LOG(ERROR) << "Local requests are not allowed.";
    std::move(callback).Run(
        diagnosticsd::mojom::DiagnosticsdWebRequestStatus::kNetworkError,
        0 /* http_status */, mojo::ScopedHandle() /*response_body */);
    return;
  }

  // Create a web request.
  auto request = std::make_unique<WebRequest>();
  request->request = std::make_unique<network::ResourceRequest>();
  request->request->method = GetHttpMethod(http_method);
  request->request->url = std::move(url);
  request->request->allow_credentials = false;
  request->request->load_flags = net::LOAD_DISABLE_CACHE;
  for (auto header : headers) {
    request->request->headers.AddHeaderFromString(header);
  }

  request->request_body = std::move(request_body);
  request->callback = std::move(callback);

  request_queue_.push(std::move(request));
  MaybeStartNextRequest();
}

DiagnosticsdWebRequestService::WebRequest::WebRequest() = default;

DiagnosticsdWebRequestService::WebRequest::~WebRequest() = default;

void DiagnosticsdWebRequestService::MaybeStartNextRequest() {
  // Starts the next web requests only if there is nothing pending.
  if (active_request_ || request_queue_.empty())
    return;

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("diagnosticsd", R"(
          semantics {
            sender: "Diagnosticsd"
            description: "Perform a web request."
            trigger:
                "diagnostics_processor performs a web request to their server."
            data:
                "diagnostics_processor's proprietary data."
            destination: OTHER
          }
          policy {
            cookies_allowed: NO
          }
      )");

  // Start new web request.
  active_request_ = std::move(request_queue_.front());
  request_queue_.pop();

  url_loader_ = network::SimpleURLLoader::Create(
      std::move(active_request_->request), traffic_annotation);
  // Allows non-empty response body in case of HTTP errors.
  url_loader_->SetAllowHttpErrorResults(true /* allow */);

  if (!active_request_->request_body.empty()) {
    url_loader_->AttachStringForUpload(active_request_->request_body,
                                       "text/plain");
  }
  // Do not retry.
  url_loader_->SetRetryOptions(0, network::SimpleURLLoader::RETRY_NEVER);
  url_loader_->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&DiagnosticsdWebRequestService::OnRequestComplete,
                     base::Unretained(this)),
      kDiagnosticsdWebResponseMaxSizeInBytes);
}

void DiagnosticsdWebRequestService::OnRequestComplete(
    std::unique_ptr<std::string> response_body) {
  DCHECK(active_request_);

  int response_code = -1;
  if (url_loader_->ResponseInfo() && url_loader_->ResponseInfo()->headers) {
    response_code = url_loader_->ResponseInfo()->headers->response_code();
  }

  int net_error = url_loader_->NetError();
  // Got a network error.
  if (net_error != net::OK &&
      (response_code == -1 || IsHttpOkCode(response_code))) {
    DVLOG(0) << "Web request failed with error: " << net_error
             << net::ErrorToString(net_error);
    std::move(active_request_->callback)
        .Run(diagnosticsd::mojom::DiagnosticsdWebRequestStatus::kNetworkError,
             0 /* http_status */, mojo::ScopedHandle() /* response_body */);
    active_request_.reset();
    MaybeStartNextRequest();
    return;
  }

  // The response_code cannot be parsed from the web response.
  if (response_code == -1) {
    LOG(ERROR) << "Web request response cannot be parsed.";
    response_code = net::HTTP_INTERNAL_SERVER_ERROR;
  }

  DCHECK(!response_body ||
         response_body->size() <= kDiagnosticsdWebResponseMaxSizeInBytes);

  mojo::ScopedHandle response_body_handle;
  if (response_body)
    response_body_handle = CreateReadOnlySharedMemoryMojoHandle(*response_body);

  // Got an HTTP error.
  if (!IsHttpOkCode(response_code)) {
    std::move(active_request_->callback)
        .Run(diagnosticsd::mojom::DiagnosticsdWebRequestStatus::kHttpError,
             response_code, std::move(response_body_handle));
    active_request_.reset();
    MaybeStartNextRequest();
    return;
  }

  // The web request is completed successfully.
  std::move(active_request_->callback)
      .Run(diagnosticsd::mojom::DiagnosticsdWebRequestStatus::kOk,
           response_code, std::move(response_body_handle));
  active_request_.reset();
  MaybeStartNextRequest();
}

}  // namespace chromeos
