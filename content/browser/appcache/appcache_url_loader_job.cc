// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/appcache/appcache_url_loader_job.h"

#include "base/strings/string_number_conversions.h"
#include "content/browser/appcache/appcache_histograms.h"
#include "content/common/net_adapters.h"
#include "content/public/common/resource_type.h"
#include "net/http/http_status_code.h"

namespace content {

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

  DCHECK(!loader_callback_.is_null());
  // In network service land, if we are processing a navigation request, we
  // need to inform the loader callback that we are not going to handle this
  // request. The loader callback is valid only for navigation requests.
  std::move(loader_callback_).Run(StartLoaderCallback());
}

void AppCacheURLLoaderJob::DeliverErrorResponse() {
  delivery_type_ = ERROR_DELIVERY;

  // We expect the URLLoaderClient pointer to be valid at this point.
  DCHECK(client_info_);

  // AppCacheURLRequestJob uses ERR_FAILED as the error code here. That seems
  // to map to HTTP_INTERNAL_SERVER_ERROR.
  std::string status("HTTP/1.1 ");
  status.append(base::IntToString(net::HTTP_INTERNAL_SERVER_ERROR));
  status.append(" ");
  status.append(net::GetHttpReasonPhrase(net::HTTP_INTERNAL_SERVER_ERROR));
  status.append("\0\0", 2);

  ResourceResponseHead response;
  response.headers = new net::HttpResponseHeaders(status);
  client_info_->OnReceiveResponse(response, base::nullopt, nullptr);

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
  DCHECK(false);
}

void AppCacheURLLoaderJob::SetPriority(net::RequestPriority priority,
                                       int32_t intra_priority_value) {
  NOTREACHED() << "We don't support SetPriority()";
}

void AppCacheURLLoaderJob::Start(mojom::URLLoaderRequest request,
                                 mojom::URLLoaderClientPtr client) {
  DCHECK(!binding_.is_bound());
  binding_.Bind(std::move(request));

  binding_.set_connection_error_handler(base::Bind(
      &AppCacheURLLoaderJob::OnConnectionError, StaticAsWeakPtr(this)));

  client_info_ = std::move(client);

  // Send the cached AppCacheResponse if any.
  if (info_.get())
    SendResponseInfo();
}

AppCacheURLLoaderJob::AppCacheURLLoaderJob(const ResourceRequest& request,
                                           AppCacheStorage* storage)
    : request_(request),
      storage_(storage->GetWeakPtr()),
      start_time_tick_(base::TimeTicks::Now()),
      cache_id_(kAppCacheNoCacheId),
      is_fallback_(false),
      binding_(this),
      writable_handle_watcher_(FROM_HERE,
                               mojo::SimpleWatcher::ArmingPolicy::MANUAL) {}

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

    DCHECK(!loader_callback_.is_null());
    std::move(loader_callback_)
        .Run(base::Bind(&AppCacheURLLoaderJob::Start, StaticAsWeakPtr(this)));

    response_body_stream_ = std::move(data_pipe_.producer_handle);

    // TODO(ananta)
    // Move the asynchronous reading and mojo pipe handling code to a helper
    // class. That would also need a change to BlobURLLoader.

    // Wait for the data pipe to be ready to accept data.
    writable_handle_watcher_.Watch(
        response_body_stream_.get(), MOJO_HANDLE_SIGNAL_WRITABLE,
        base::Bind(&AppCacheURLLoaderJob::OnResponseBodyStreamReady,
                   StaticAsWeakPtr(this)));

    if (client_info_)
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
  DCHECK(client_info_);

  // If this is null it means the response information was sent to the client.
  if (!data_pipe_.consumer_handle.is_valid())
    return;

  const net::HttpResponseInfo* http_info = is_range_request()
                                               ? range_response_info_.get()
                                               : info_->http_response_info();

  ResourceResponseHead response_head;
  response_head.headers = http_info->headers;

  // TODO(ananta)
  // Copy more fields.
  http_info->headers->GetMimeType(&response_head.mime_type);
  http_info->headers->GetCharset(&response_head.charset);

  response_head.request_time = http_info->request_time;
  response_head.response_time = http_info->response_time;
  response_head.content_length =
      is_range_request() ? range_response_info_->headers->GetContentLength()
                         : info_->response_data_size();

  client_info_->OnReceiveResponse(response_head, http_info->ssl_info,
                                  mojom::DownloadedTempFilePtr());

  client_info_->OnStartLoadingResponseBody(
      std::move(data_pipe_.consumer_handle));
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
  // TODO(ananta)
  // Fill other details in the ResourceRequestCompletionStatus structure.
  ResourceRequestCompletionStatus request_complete_data;
  request_complete_data.error_code = error_code;
  client_info_->OnComplete(request_complete_data);
}

}  // namespace content
