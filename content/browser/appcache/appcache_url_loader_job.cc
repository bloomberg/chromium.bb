// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/appcache/appcache_url_loader_job.h"

#include "base/strings/string_number_conversions.h"
#include "content/browser/appcache/appcache_histograms.h"
#include "content/browser/appcache/appcache_subresource_url_factory.h"
#include "content/browser/appcache/appcache_url_loader_request.h"
#include "content/browser/url_loader_factory_getter.h"
#include "content/common/net_adapters.h"
#include "content/public/common/resource_type.h"
#include "net/http/http_status_code.h"

namespace content {

SubresourceLoadInfo::SubresourceLoadInfo()
    : routing_id(-1), request_id(-1), options(0) {}

SubresourceLoadInfo::~SubresourceLoadInfo() {}

AppCacheURLLoaderJob::~AppCacheURLLoaderJob() {
  if (storage_.get())
    storage_->CancelDelegateCallbacks(this);
}

void AppCacheURLLoaderJob::Kill() {}

bool AppCacheURLLoaderJob::IsStarted() const {
  return delivery_type_ != AWAITING_DELIVERY_ORDERS;
}

void AppCacheURLLoaderJob::DeliverAppCachedResponse(const GURL& manifest_url,
                                                    int64_t cache_id,
                                                    const AppCacheEntry& entry,
                                                    bool is_fallback) {
  if (!storage_.get()) {
    DeliverErrorResponse();
    return;
  }

  delivery_type_ = APPCACHED_DELIVERY;

  load_timing_info_.request_start_time = base::Time::Now();
  load_timing_info_.request_start = base::TimeTicks::Now();

  AppCacheHistograms::AddAppCacheJobStartDelaySample(base::TimeTicks::Now() -
                                                     start_time_tick_);

  manifest_url_ = manifest_url;
  cache_id_ = cache_id;
  entry_ = entry;
  is_fallback_ = is_fallback;

  // Handle range requests.
  net::HttpRequestHeaders headers;
  headers.AddHeadersFromString(request_.headers);
  InitializeRangeRequestInfo(headers);

  // TODO(ananta)
  // Implement the AppCacheServiceImpl::Observer interface or add weak pointer
  // support to it.
  storage_->LoadResponseInfo(manifest_url_, entry_.response_id(), this);
}

void AppCacheURLLoaderJob::DeliverNetworkResponse() {
  delivery_type_ = NETWORK_DELIVERY;

  AppCacheHistograms::AddNetworkJobStartDelaySample(base::TimeTicks::Now() -
                                                    start_time_tick_);

  if (IsResourceTypeFrame(request_.resource_type)) {
    DCHECK(!main_resource_loader_callback_.is_null());
    // In network service land, if we are processing a navigation request, we
    // need to inform the loader callback that we are not going to handle this
    // request. The loader callback is valid only for navigation requests.
    std::move(main_resource_loader_callback_).Run(StartLoaderCallback());
  } else {
    mojom::URLLoaderClientPtr client_ptr;
    network_loader_client_binding_.Bind(mojo::MakeRequest(&client_ptr));

    default_url_loader_factory_getter_->GetNetworkFactory()
        ->get()
        ->CreateLoaderAndStart(
            mojo::MakeRequest(&network_loader_),
            subresource_load_info_->routing_id,
            subresource_load_info_->request_id, subresource_load_info_->options,
            subresource_load_info_->request, std::move(client_ptr),
            subresource_load_info_->traffic_annotation);
  }
}

void AppCacheURLLoaderJob::DeliverErrorResponse() {
  delivery_type_ = ERROR_DELIVERY;

  // We expect the URLLoaderClient pointer to be valid at this point.
  DCHECK(client_);

  // AppCacheURLRequestJob uses ERR_FAILED as the error code here. That seems
  // to map to HTTP_INTERNAL_SERVER_ERROR.
  std::string status("HTTP/1.1 ");
  status.append(base::IntToString(net::HTTP_INTERNAL_SERVER_ERROR));
  status.append(" ");
  status.append(net::GetHttpReasonPhrase(net::HTTP_INTERNAL_SERVER_ERROR));
  status.append("\0\0", 2);

  ResourceResponseHead response;
  response.headers = new net::HttpResponseHeaders(status);
  client_->OnReceiveResponse(response, base::nullopt, nullptr);

  NotifyCompleted(net::ERR_FAILED);

  AppCacheHistograms::AddErrorJobStartDelaySample(base::TimeTicks::Now() -
                                                  start_time_tick_);
}

const GURL& AppCacheURLLoaderJob::GetURL() const {
  return request_.url;
}

AppCacheURLLoaderJob* AppCacheURLLoaderJob::AsURLLoaderJob() {
  return this;
}

void AppCacheURLLoaderJob::FollowRedirect() {
  if (network_loader_)
    network_loader_->FollowRedirect();
}

void AppCacheURLLoaderJob::SetPriority(net::RequestPriority priority,
                                       int32_t intra_priority_value) {
  if (network_loader_)
    network_loader_->SetPriority(priority, intra_priority_value);
}

void AppCacheURLLoaderJob::OnReceiveResponse(
    const ResourceResponseHead& response_head,
    const base::Optional<net::SSLInfo>& ssl_info,
    mojom::DownloadedTempFilePtr downloaded_file) {
  appcache_request_->set_response(response_head);
  // The MaybeLoadFallbackForResponse() call below can pass a fallback
  // response to us. Reset the delivery_type_ to ensure that we can
  // receive it
  delivery_type_ = AWAITING_DELIVERY_ORDERS;
  if (!sub_resource_handler_->MaybeLoadFallbackForResponse(nullptr)) {
    client_->OnReceiveResponse(response_head, ssl_info,
                               std::move(downloaded_file));
  } else {
    // Disconnect from the network loader as we are delivering a fallback
    // response to the client.
    DisconnectFromNetworkLoader();
  }
}

void AppCacheURLLoaderJob::OnReceiveRedirect(
    const net::RedirectInfo& redirect_info,
    const ResourceResponseHead& response_head) {
  appcache_request_->set_response(response_head);
  // The MaybeLoadFallbackForRedirect() call below can pass a fallback
  // response to us. Reset the delivery_type_ to ensure that we can
  // receive it
  delivery_type_ = AWAITING_DELIVERY_ORDERS;
  if (!sub_resource_handler_->MaybeLoadFallbackForRedirect(
          nullptr, redirect_info.new_url)) {
    client_->OnReceiveRedirect(redirect_info, response_head);
  } else {
    // Disconnect from the network loader as we are delivering a fallback
    // response to the client.
    DisconnectFromNetworkLoader();
  }
}

void AppCacheURLLoaderJob::OnDataDownloaded(int64_t data_len,
                                            int64_t encoded_data_len) {
  client_->OnDataDownloaded(data_len, encoded_data_len);
}

void AppCacheURLLoaderJob::OnUploadProgress(
    int64_t current_position,
    int64_t total_size,
    OnUploadProgressCallback ack_callback) {
  client_->OnUploadProgress(current_position, total_size,
                            std::move(ack_callback));
}

void AppCacheURLLoaderJob::OnReceiveCachedMetadata(
    const std::vector<uint8_t>& data) {
  client_->OnReceiveCachedMetadata(data);
}

void AppCacheURLLoaderJob::OnTransferSizeUpdated(int32_t transfer_size_diff) {
  client_->OnTransferSizeUpdated(transfer_size_diff);
}

void AppCacheURLLoaderJob::OnStartLoadingResponseBody(
    mojo::ScopedDataPipeConsumerHandle body) {
  client_->OnStartLoadingResponseBody(std::move(body));
}

void AppCacheURLLoaderJob::OnComplete(
    const ResourceRequestCompletionStatus& status) {
  delivery_type_ = AWAITING_DELIVERY_ORDERS;
  if (!sub_resource_handler_->MaybeLoadFallbackForResponse(nullptr)) {
    client_->OnComplete(status);
  } else {
    // Disconnect from the network loader as we are delivering a fallback
    // response to the client.
    DisconnectFromNetworkLoader();
  }
}

void AppCacheURLLoaderJob::SetSubresourceLoadInfo(
    std::unique_ptr<SubresourceLoadInfo> subresource_load_info,
    URLLoaderFactoryGetter* default_url_loader) {
  subresource_load_info_ = std::move(subresource_load_info);

  binding_.Bind(std::move(subresource_load_info_->url_loader_request));
  binding_.set_connection_error_handler(base::Bind(
      &AppCacheURLLoaderJob::OnConnectionError, StaticAsWeakPtr(this)));

  client_ = std::move(subresource_load_info_->client);
  default_url_loader_factory_getter_ = default_url_loader;
}

void AppCacheURLLoaderJob::BindRequest(mojom::URLLoaderClientPtr client,
                                       mojom::URLLoaderRequest request) {
  DCHECK(!binding_.is_bound());
  binding_.Bind(std::move(request));

  client_ = std::move(client);

  binding_.set_connection_error_handler(base::Bind(
      &AppCacheURLLoaderJob::OnConnectionError, StaticAsWeakPtr(this)));
}

void AppCacheURLLoaderJob::Start(mojom::URLLoaderRequest request,
                                 mojom::URLLoaderClientPtr client) {
  BindRequest(std::move(client), std::move(request));
  // Send the cached AppCacheResponse if any.
  if (info_.get())
    SendResponseInfo();
}

AppCacheURLLoaderJob::AppCacheURLLoaderJob(
    const ResourceRequest& request,
    AppCacheURLLoaderRequest* appcache_request,
    AppCacheStorage* storage)
    : request_(request),
      storage_(storage->GetWeakPtr()),
      start_time_tick_(base::TimeTicks::Now()),
      cache_id_(kAppCacheNoCacheId),
      is_fallback_(false),
      binding_(this),
      writable_handle_watcher_(FROM_HERE,
                               mojo::SimpleWatcher::ArmingPolicy::MANUAL),
      network_loader_client_binding_(this),
      appcache_request_(appcache_request) {}

void AppCacheURLLoaderJob::OnResponseInfoLoaded(
    AppCacheResponseInfo* response_info,
    int64_t response_id) {
  DCHECK(IsDeliveringAppCacheResponse());

  if (!storage_.get()) {
    DeliverErrorResponse();
    return;
  }

  if (response_info) {
    info_ = response_info;
    reader_.reset(
        storage_->CreateResponseReader(manifest_url_, entry_.response_id()));

    if (is_range_request())
      SetupRangeResponse();

    if (IsResourceTypeFrame(request_.resource_type) &&
        main_resource_loader_callback_) {
      std::move(main_resource_loader_callback_)
          .Run(base::Bind(&AppCacheURLLoaderJob::Start, StaticAsWeakPtr(this)));
    }

    response_body_stream_ = std::move(data_pipe_.producer_handle);

    // TODO(ananta)
    // Move the asynchronous reading and mojo pipe handling code to a helper
    // class. That would also need a change to BlobURLLoader.

    // Wait for the data pipe to be ready to accept data.
    writable_handle_watcher_.Watch(
        response_body_stream_.get(), MOJO_HANDLE_SIGNAL_WRITABLE,
        base::Bind(&AppCacheURLLoaderJob::OnResponseBodyStreamReady,
                   StaticAsWeakPtr(this)));

    if (client_)
      SendResponseInfo();

    ReadMore();
  } else {
    // Error case here. We fallback to the network.
    DeliverNetworkResponse();
    AppCacheHistograms::CountResponseRetrieval(
        false, IsResourceTypeFrame(request_.resource_type),
        manifest_url_.GetOrigin());

    cache_entry_not_found_ = true;
  }
}

void AppCacheURLLoaderJob::OnReadComplete(int result) {
  DLOG(WARNING) << "AppCache read completed with result: " << result;

  if (result <= 0) {
    writable_handle_watcher_.Cancel();
    pending_write_->Complete(0);
    pending_write_ = nullptr;
  }

  bool is_main_resource = IsResourceTypeFrame(request_.resource_type);

  if (result == 0) {
    NotifyCompleted(result);
    AppCacheHistograms::CountResponseRetrieval(true, is_main_resource,
                                               manifest_url_.GetOrigin());
    return;
  } else if (result < 0) {
    // TODO(ananta)
    // Populate the relevant fields of the ResourceRequestCompletionStatus
    // structure.
    NotifyCompleted(result);
    AppCacheHistograms::CountResponseRetrieval(false, is_main_resource,
                                               manifest_url_.GetOrigin());
    return;
  }

  uint32_t bytes_written = static_cast<uint32_t>(result);
  response_body_stream_ = pending_write_->Complete(bytes_written);
  pending_write_ = nullptr;
  ReadMore();
}

void AppCacheURLLoaderJob::OnConnectionError() {
  if (storage_.get())
    storage_->CancelDelegateCallbacks(this);
  base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, this);
}

void AppCacheURLLoaderJob::SendResponseInfo() {
  DCHECK(client_);
  // If this is null it means the response information was sent to the client.
  if (!data_pipe_.consumer_handle.is_valid())
    return;

  const net::HttpResponseInfo* http_info = is_range_request()
                                               ? range_response_info_.get()
                                               : info_->http_response_info();

  ResourceResponseHead response_head;
  response_head.headers = http_info->headers;
  response_head.appcache_id = cache_id_;
  response_head.appcache_manifest_url = manifest_url_;

  http_info->headers->GetMimeType(&response_head.mime_type);
  http_info->headers->GetCharset(&response_head.charset);

  // TODO(ananta)
  // Verify if the times sent here are correct.
  response_head.request_time = http_info->request_time;
  response_head.response_time = http_info->response_time;
  response_head.content_length =
      is_range_request() ? range_response_info_->headers->GetContentLength()
                         : info_->response_data_size();

  response_head.connection_info = http_info->connection_info;
  response_head.socket_address = http_info->socket_address;
  response_head.was_fetched_via_spdy = http_info->was_fetched_via_spdy;
  response_head.was_alpn_negotiated = http_info->was_alpn_negotiated;
  response_head.alpn_negotiated_protocol = http_info->alpn_negotiated_protocol;

  response_head.load_timing = load_timing_info_;

  appcache_request_->set_response(response_head);

  client_->OnReceiveResponse(response_head, http_info->ssl_info,
                             mojom::DownloadedTempFilePtr());

  client_->OnStartLoadingResponseBody(std::move(data_pipe_.consumer_handle));
}

void AppCacheURLLoaderJob::ReadMore() {
  DCHECK(!pending_write_.get());

  uint32_t num_bytes;
  // TODO: we should use the abstractions in MojoAsyncResourceHandler.
  MojoResult result = NetToMojoPendingBuffer::BeginWrite(
      &response_body_stream_, &pending_write_, &num_bytes);
  if (result == MOJO_RESULT_SHOULD_WAIT) {
    // The pipe is full. We need to wait for it to have more space.
    writable_handle_watcher_.ArmOrNotify();
    return;
  } else if (result != MOJO_RESULT_OK) {
    // The response body stream is in a bad state. Bail.
    // TODO(ananta)
    // Add proper error handling here.
    NotifyCompleted(net::ERR_FAILED);
    writable_handle_watcher_.Cancel();
    response_body_stream_.reset();
    return;
  }

  CHECK_GT(static_cast<uint32_t>(std::numeric_limits<int>::max()), num_bytes);
  scoped_refptr<NetToMojoIOBuffer> buffer =
      new NetToMojoIOBuffer(pending_write_.get());

  reader_->ReadData(
      buffer.get(), info_->response_data_size(),
      base::Bind(&AppCacheURLLoaderJob::OnReadComplete, StaticAsWeakPtr(this)));
}

void AppCacheURLLoaderJob::OnResponseBodyStreamReady(MojoResult result) {
  // TODO(ananta)
  // Add proper error handling here.
  if (result != MOJO_RESULT_OK) {
    DCHECK(false);
    NotifyCompleted(net::ERR_FAILED);
  }
  ReadMore();
}

void AppCacheURLLoaderJob::NotifyCompleted(int error_code) {
  if (storage_.get())
    storage_->CancelDelegateCallbacks(this);

  const net::HttpResponseInfo* http_info =
      is_range_request() ? range_response_info_.get()
                         : (info_ ? info_->http_response_info() : nullptr);

  ResourceRequestCompletionStatus request_complete_data;
  request_complete_data.error_code = error_code;

  // TODO(ananta)
  // Fill other details in the ResourceRequestCompletionStatus structure in
  // case of an error.
  if (!request_complete_data.error_code) {
    request_complete_data.exists_in_cache = http_info->was_cached;
    request_complete_data.completion_time = base::TimeTicks::Now();
    request_complete_data.encoded_body_length =
        is_range_request() ? range_response_info_->headers->GetContentLength()
                           : (info_ ? info_->response_data_size() : 0);
    request_complete_data.decoded_body_length =
        request_complete_data.encoded_body_length;
  }
  client_->OnComplete(request_complete_data);
}

void AppCacheURLLoaderJob::DisconnectFromNetworkLoader() {
  // Close the pipe to the network loader as we are delivering a fallback
  // response to the client.
  network_loader_client_binding_.Close();
  network_loader_ = nullptr;
}

}  // namespace content
