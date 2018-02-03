// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/web_package_loader.h"

#include <memory>

#include "base/feature_list.h"
#include "base/strings/stringprintf.h"
#include "content/browser/loader/data_pipe_to_source_stream.h"
#include "content/browser/loader/signed_exchange_handler.h"
#include "content/browser/loader/source_stream_to_data_pipe.h"
#include "content/public/common/content_features.h"
#include "net/http/http_util.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/url_loader_completion_status.h"

namespace content {

namespace {

net::RedirectInfo CreateRedirectInfo(const GURL& new_url) {
  net::RedirectInfo redirect_info;
  redirect_info.new_url = new_url;
  redirect_info.new_method = "GET";
  redirect_info.status_code = 302;
  redirect_info.new_site_for_cookies = redirect_info.new_url;
  return redirect_info;
}

constexpr static int kDefaultBufferSize = 64 * 1024;

}  // namespace

class WebPackageLoader::ResponseTimingInfo {
 public:
  explicit ResponseTimingInfo(const network::ResourceResponseHead& response)
      : request_start_(response.request_start),
        response_start_(response.response_start),
        request_time_(response.request_time),
        response_time_(response.response_time),
        load_timing_(response.load_timing) {}

  network::ResourceResponseHead CreateRedirectResponseHead() const {
    network::ResourceResponseHead response_head;
    response_head.encoded_data_length = 0;
    std::string buf(base::StringPrintf("HTTP/1.1 %d %s\r\n", 302, "Found"));
    response_head.headers = new net::HttpResponseHeaders(
        net::HttpUtil::AssembleRawHeaders(buf.c_str(), buf.size()));
    response_head.encoded_data_length = 0;
    response_head.request_start = request_start_;
    response_head.response_start = response_start_;
    response_head.request_time = request_time_;
    response_head.response_time = response_time_;
    response_head.load_timing = load_timing_;
    return response_head;
  }

 private:
  const base::TimeTicks request_start_;
  const base::TimeTicks response_start_;
  const base::Time request_time_;
  const base::Time response_time_;
  const net::LoadTimingInfo load_timing_;

  DISALLOW_COPY_AND_ASSIGN(ResponseTimingInfo);
};

WebPackageLoader::WebPackageLoader(
    const network::ResourceResponseHead& original_response,
    network::mojom::URLLoaderClientPtr forwarding_client,
    network::mojom::URLLoaderClientEndpointsPtr endpoints)
    : original_response_timing_info_(
          std::make_unique<ResponseTimingInfo>(original_response)),
      forwarding_client_(std::move(forwarding_client)),
      url_loader_client_binding_(this),
      weak_factory_(this) {
  DCHECK(base::FeatureList::IsEnabled(features::kSignedHTTPExchange));
  url_loader_.Bind(std::move(endpoints->url_loader));

  if (!base::FeatureList::IsEnabled(network::features::kNetworkService)) {
    // We don't propagate the response to the navigation request and its
    // throttles, therefore we need to call this here internally in order to
    // move it forward.
    // TODO(https://crbug.com/791049): Remove this when NetworkService is
    // enabled by default.
    url_loader_->ProceedWithResponse();
  }

  // Bind the endpoint with |this| to get the body DataPipe.
  url_loader_client_binding_.Bind(std::move(endpoints->url_loader_client));

  // |client_| will be bound with a forwarding client by ConnectToClient().
  pending_client_request_ = mojo::MakeRequest(&client_);
}

WebPackageLoader::~WebPackageLoader() = default;

void WebPackageLoader::OnReceiveResponse(
    const network::ResourceResponseHead& response_head,
    const base::Optional<net::SSLInfo>& ssl_info,
    network::mojom::DownloadedTempFilePtr downloaded_file) {
  // Must not be called because this WebPackageLoader and the client endpoints
  // were bound after OnReceiveResponse() is called.
  NOTREACHED();
}

void WebPackageLoader::OnReceiveRedirect(
    const net::RedirectInfo& redirect_info,
    const network::ResourceResponseHead& response_head) {
  // Must not be called because this WebPackageLoader and the client endpoints
  // were bound after OnReceiveResponse() is called.
  NOTREACHED();
}

void WebPackageLoader::OnDataDownloaded(int64_t data_len,
                                        int64_t encoded_data_len) {
  // Must not be called because this WebPackageLoader and the client endpoints
  // were bound after OnReceiveResponse() is called.
  NOTREACHED();
}

void WebPackageLoader::OnUploadProgress(int64_t current_position,
                                        int64_t total_size,
                                        OnUploadProgressCallback ack_callback) {
  // Must not be called because this WebPackageLoader and the client endpoints
  // were bound after OnReceiveResponse() is called.
  NOTREACHED();
}

void WebPackageLoader::OnReceiveCachedMetadata(
    const std::vector<uint8_t>& data) {
  // Curerntly CachedMetadata for WebPackage is not supported.
  NOTREACHED();
}

void WebPackageLoader::OnTransferSizeUpdated(int32_t transfer_size_diff) {
  // TODO(https://crbug.com/803774): Implement this to progressively update the
  // encoded data length in DevTools.
}

void WebPackageLoader::OnStartLoadingResponseBody(
    mojo::ScopedDataPipeConsumerHandle body) {
  signed_exchange_handler_ = std::make_unique<SignedExchangeHandler>(
      std::make_unique<DataPipeToSourceStream>(std::move(body)),
      base::BindOnce(&WebPackageLoader::OnHTTPExchangeFound,
                     weak_factory_.GetWeakPtr()));
}

void WebPackageLoader::OnComplete(
    const network::URLLoaderCompletionStatus& status) {
  // TODO(https://crbug.com/803774): Copy the data length information and pass
  // to |client_| when OnHTTPExchangeFinished() is called.
}

void WebPackageLoader::FollowRedirect() {
  NOTREACHED();
}

void WebPackageLoader::ProceedWithResponse() {
  // TODO(https://crbug.com/791049): Remove this when NetworkService is
  // enabled by default.
  DCHECK(!base::FeatureList::IsEnabled(network::features::kNetworkService));
  DCHECK(body_data_pipe_adapter_);
  DCHECK(pending_body_consumer_.is_valid());

  body_data_pipe_adapter_->Start();
  client_->OnStartLoadingResponseBody(std::move(pending_body_consumer_));
}

void WebPackageLoader::SetPriority(net::RequestPriority priority,
                                   int intra_priority_value) {
  // TODO(https://crbug.com/803774): Implement this.
}

void WebPackageLoader::PauseReadingBodyFromNet() {
  // TODO(https://crbug.com/803774): Implement this.
}

void WebPackageLoader::ResumeReadingBodyFromNet() {
  // TODO(https://crbug.com/803774): Implement this.
}

void WebPackageLoader::ConnectToClient(
    network::mojom::URLLoaderClientPtr client) {
  DCHECK(pending_client_request_.is_pending());
  mojo::FuseInterface(std::move(pending_client_request_),
                      client.PassInterface());
}

void WebPackageLoader::OnHTTPExchangeFound(
    const GURL& request_url,
    const std::string& request_method,
    const network::ResourceResponseHead& resource_response,
    base::Optional<net::SSLInfo> ssl_info) {
  // TODO(https://crbug.com/803774): Handle no-GET request_method as a error.
  DCHECK(original_response_timing_info_);
  forwarding_client_->OnReceiveRedirect(
      CreateRedirectInfo(request_url),
      std::move(original_response_timing_info_)->CreateRedirectResponseHead());
  forwarding_client_->OnComplete(network::URLLoaderCompletionStatus(net::OK));
  forwarding_client_.reset();

  client_->OnReceiveResponse(resource_response, std::move(ssl_info),
                             nullptr /* downloaded_file */);

  // Currently we always assume that we have body.
  // TODO(https://crbug.com/80374): Add error handling and bail out
  // earlier if there's an error.

  mojo::DataPipe data_pipe(kDefaultBufferSize);
  pending_body_consumer_ = std::move(data_pipe.consumer_handle);

  body_data_pipe_adapter_ = std::make_unique<SourceStreamToDataPipe>(
      signed_exchange_handler_.get(), std::move(data_pipe.producer_handle),
      base::BindOnce(&WebPackageLoader::FinishReadingBody,
                     base::Unretained(this)));

  if (!base::FeatureList::IsEnabled(network::features::kNetworkService)) {
    // Need to wait until ProceedWithResponse() is called.
    return;
  }

  // Start reading.
  body_data_pipe_adapter_->Start();
  client_->OnStartLoadingResponseBody(std::move(pending_body_consumer_));
}

void WebPackageLoader::FinishReadingBody(int result) {
  // This will eventually delete |this|.
  client_->OnComplete(network::URLLoaderCompletionStatus(result));
}

}  // namespace content
