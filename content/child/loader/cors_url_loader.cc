// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/loader/cors_url_loader.h"

namespace content {

// TODO(hintzed): At the moment this class simply forwards all calls to the
// underlying network loader. Part of the effort to move CORS handling out of
// Blink: http://crbug/736308.
CORSURLLoader::CORSURLLoader(
    int32_t routing_id,
    int32_t request_id,
    uint32_t options,
    const ResourceRequest& resource_request,
    mojom::URLLoaderClientPtr client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
    mojom::URLLoaderFactory* network_loader_factory)
    : network_loader_factory_(network_loader_factory),
      network_client_binding_(this),
      forwarding_client_(std::move(client)) {
  mojom::URLLoaderClientPtr network_client;
  network_client_binding_.Bind(mojo::MakeRequest(&network_client));
  network_loader_factory_->CreateLoaderAndStart(
      mojo::MakeRequest(&network_loader_), routing_id, request_id, options,
      resource_request, std::move(network_client), traffic_annotation);
}

CORSURLLoader::~CORSURLLoader() {}

void CORSURLLoader::FollowRedirect() {
  network_loader_->FollowRedirect();
}

void CORSURLLoader::SetPriority(net::RequestPriority priority,
                                int32_t intra_priority_value) {
  network_loader_->SetPriority(priority, intra_priority_value);
}

// mojom::URLLoaderClient for simply proxying network for now:
void CORSURLLoader::OnReceiveResponse(
    const ResourceResponseHead& response_head,
    const base::Optional<net::SSLInfo>& ssl_info,
    mojom::DownloadedTempFilePtr downloaded_file) {
  forwarding_client_->OnReceiveResponse(response_head, ssl_info,
                                        std::move(downloaded_file));
}

void CORSURLLoader::OnReceiveRedirect(
    const net::RedirectInfo& redirect_info,
    const ResourceResponseHead& response_head) {
  forwarding_client_->OnReceiveRedirect(redirect_info, response_head);
}

void CORSURLLoader::OnDataDownloaded(int64_t data_len,
                                     int64_t encoded_data_len) {
  forwarding_client_->OnDataDownloaded(data_len, encoded_data_len);
}

void CORSURLLoader::OnUploadProgress(int64_t current_position,
                                     int64_t total_size,
                                     OnUploadProgressCallback ack_callback) {
  forwarding_client_->OnUploadProgress(current_position, total_size,
                                       std::move(ack_callback));
}

void CORSURLLoader::OnReceiveCachedMetadata(const std::vector<uint8_t>& data) {
  forwarding_client_->OnReceiveCachedMetadata(data);
}

void CORSURLLoader::OnTransferSizeUpdated(int32_t transfer_size_diff) {
  forwarding_client_->OnTransferSizeUpdated(transfer_size_diff);
}

void CORSURLLoader::OnStartLoadingResponseBody(
    mojo::ScopedDataPipeConsumerHandle body) {
  forwarding_client_->OnStartLoadingResponseBody(std::move(body));
}

void CORSURLLoader::OnComplete(const ResourceRequestCompletionStatus& status) {
  forwarding_client_->OnComplete(status);
}

}  // namespace content
