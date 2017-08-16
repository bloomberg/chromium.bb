// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_script_url_loader.h"

#include <memory>
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_provider_host.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/browser/url_loader_factory_getter.h"
#include "content/public/common/resource_response.h"
#include "storage/browser/blob/blob_storage_context.h"

namespace content {

ServiceWorkerScriptURLLoader::ServiceWorkerScriptURLLoader(
    int32_t routing_id,
    int32_t request_id,
    uint32_t options,
    const ResourceRequest& resource_request,
    mojom::URLLoaderClientPtr client,
    base::WeakPtr<ServiceWorkerContextCore> context,
    base::WeakPtr<ServiceWorkerProviderHost> provider_host,
    base::WeakPtr<storage::BlobStorageContext> blob_storage_context,
    scoped_refptr<URLLoaderFactoryGetter> loader_factory_getter,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation)
    : network_client_binding_(this),
      forwarding_client_(std::move(client)),
      provider_host_(provider_host) {
  mojom::URLLoaderClientPtr network_client;
  network_client_binding_.Bind(mojo::MakeRequest(&network_client));
  loader_factory_getter->GetNetworkFactory()->get()->CreateLoaderAndStart(
      mojo::MakeRequest(&network_loader_), routing_id, request_id, options,
      resource_request, std::move(network_client), traffic_annotation);
}

ServiceWorkerScriptURLLoader::~ServiceWorkerScriptURLLoader() = default;

void ServiceWorkerScriptURLLoader::FollowRedirect() {
  network_loader_->FollowRedirect();
}

void ServiceWorkerScriptURLLoader::SetPriority(net::RequestPriority priority,
                                               int32_t intra_priority_value) {
  network_loader_->SetPriority(priority, intra_priority_value);
}

void ServiceWorkerScriptURLLoader::OnReceiveResponse(
    const ResourceResponseHead& response_head,
    const base::Optional<net::SSLInfo>& ssl_info,
    mojom::DownloadedTempFilePtr downloaded_file) {
  if (provider_host_) {
    // We don't have complete info here, but fill in what we have now.
    // At least we need headers and SSL info.
    net::HttpResponseInfo response_info;
    response_info.headers = response_head.headers;
    if (ssl_info.has_value())
      response_info.ssl_info = *ssl_info;
    response_info.was_fetched_via_spdy = response_head.was_fetched_via_spdy;
    response_info.was_alpn_negotiated = response_head.was_alpn_negotiated;
    response_info.alpn_negotiated_protocol =
        response_head.alpn_negotiated_protocol;
    response_info.connection_info = response_head.connection_info;
    response_info.socket_address = response_head.socket_address;

    DCHECK(provider_host_->IsHostToRunningServiceWorker());
    provider_host_->running_hosted_version()->SetMainScriptHttpResponseInfo(
        response_info);
  }
  forwarding_client_->OnReceiveResponse(response_head, ssl_info,
                                        std::move(downloaded_file));
}

void ServiceWorkerScriptURLLoader::OnReceiveRedirect(
    const net::RedirectInfo& redirect_info,
    const ResourceResponseHead& response_head) {
  forwarding_client_->OnReceiveRedirect(redirect_info, response_head);
}

void ServiceWorkerScriptURLLoader::OnDataDownloaded(int64_t data_len,
                                                    int64_t encoded_data_len) {
  forwarding_client_->OnDataDownloaded(data_len, encoded_data_len);
}

void ServiceWorkerScriptURLLoader::OnUploadProgress(
    int64_t current_position,
    int64_t total_size,
    OnUploadProgressCallback ack_callback) {
  forwarding_client_->OnUploadProgress(current_position, total_size,
                                       std::move(ack_callback));
}

void ServiceWorkerScriptURLLoader::OnReceiveCachedMetadata(
    const std::vector<uint8_t>& data) {
  forwarding_client_->OnReceiveCachedMetadata(data);
}

void ServiceWorkerScriptURLLoader::OnTransferSizeUpdated(
    int32_t transfer_size_diff) {
  forwarding_client_->OnTransferSizeUpdated(transfer_size_diff);
}

void ServiceWorkerScriptURLLoader::OnStartLoadingResponseBody(
    mojo::ScopedDataPipeConsumerHandle body) {
  forwarding_client_->OnStartLoadingResponseBody(std::move(body));
}

void ServiceWorkerScriptURLLoader::OnComplete(
    const ResourceRequestCompletionStatus& status) {
  forwarding_client_->OnComplete(status);
}

}  // namespace content
