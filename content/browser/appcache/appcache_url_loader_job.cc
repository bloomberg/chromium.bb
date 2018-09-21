// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/appcache/appcache_url_loader_job.h"

#include "base/strings/string_number_conversions.h"
#include "content/browser/appcache/appcache_histograms.h"
#include "content/browser/appcache/appcache_request_handler.h"
#include "content/browser/appcache/appcache_subresource_url_factory.h"
#include "content/browser/appcache/appcache_url_loader_request.h"
#include "content/browser/url_loader_factory_getter.h"
#include "content/public/common/resource_type.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/net_adapters.h"

namespace content {

AppCacheURLLoaderJob::~AppCacheURLLoaderJob() {
  if (storage_.get())
    storage_->CancelDelegateCallbacks(this);
}

bool AppCacheURLLoaderJob::IsStarted() const {
  return delivery_type_ != AWAITING_DELIVERY_ORDERS &&
         delivery_type_ != NETWORK_DELIVERY;
}

void AppCacheURLLoaderJob::DeliverAppCachedResponse(const GURL& manifest_url,
                                                    int64_t cache_id,
                                                    const AppCacheEntry& entry,
                                                    bool is_fallback) {
  if (!storage_.get() || !appcache_request_) {
    DeliverErrorResponse();
    return;
  }

  delivery_type_ = APPCACHED_DELIVERY;

  // In tests we only care about the delivery_type_ state.
  if (AppCacheRequestHandler::IsRunningInTests())
    return;

  load_timing_info_.request_start_time = base::Time::Now();
  load_timing_info_.request_start = base::TimeTicks::Now();

  manifest_url_ = manifest_url;
  cache_id_ = cache_id;
  entry_ = entry;
  is_fallback_ = is_fallback;

  if (is_fallback_ && loader_callback_)
    CallLoaderCallback();

  InitializeRangeRequestInfo(appcache_request_->GetResourceRequest()->headers);
  storage_->LoadResponseInfo(manifest_url_, entry_.response_id(), this);
}

void AppCacheURLLoaderJob::DeliverNetworkResponse() {
  delivery_type_ = NETWORK_DELIVERY;

  // In tests we only care about the delivery_type_ state.
  if (AppCacheRequestHandler::IsRunningInTests())
    return;

  // We signal our caller with an empy callback that it needs to perform
  // the network load.
  DCHECK(loader_callback_ && !binding_.is_bound());
  std::move(loader_callback_).Run({});
  DeleteSoon();
}

void AppCacheURLLoaderJob::DeliverErrorResponse() {
  delivery_type_ = ERROR_DELIVERY;

  // In tests we only care about the delivery_type_ state.
  if (AppCacheRequestHandler::IsRunningInTests())
    return;

  if (loader_callback_)
    CallLoaderCallback();
  NotifyCompleted(net::ERR_FAILED);
}

AppCacheURLLoaderJob* AppCacheURLLoaderJob::AsURLLoaderJob() {
  return this;
}

base::WeakPtr<AppCacheJob> AppCacheURLLoaderJob::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

base::WeakPtr<AppCacheURLLoaderJob> AppCacheURLLoaderJob::GetDerivedWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void AppCacheURLLoaderJob::FollowRedirect(
    const base::Optional<std::vector<std::string>>&
        to_be_removed_request_headers,
    const base::Optional<net::HttpRequestHeaders>& modified_request_headers) {
  NOTREACHED() << "appcache never produces redirects";
}

void AppCacheURLLoaderJob::ProceedWithResponse() {
  // TODO(arthursonzogni):  Implement this if AppCache starts using the
  // AppCacheURLLoader before the Network Service has shipped.
  NOTREACHED();
}

void AppCacheURLLoaderJob::SetPriority(net::RequestPriority priority,
                                       int32_t intra_priority_value) {}
void AppCacheURLLoaderJob::PauseReadingBodyFromNet() {}
void AppCacheURLLoaderJob::ResumeReadingBodyFromNet() {}

void AppCacheURLLoaderJob::DeleteIfNeeded() {
  if (binding_.is_bound() || is_deleting_soon_)
    return;
  delete this;
}

void AppCacheURLLoaderJob::Start(
    const network::ResourceRequest& /* resource_request */,
    network::mojom::URLLoaderRequest request,
    network::mojom::URLLoaderClientPtr client) {
  // TODO(crbug.com/876531): Figure out how AppCache interception should
  // interact with URLLoaderThrottles. It might be incorrect to ignore
  // |resource_request| here, since it's the current request after throttles.
  DCHECK(!binding_.is_bound());
  binding_.Bind(std::move(request));
  client_ = std::move(client);
  binding_.set_connection_error_handler(
      base::BindOnce(&AppCacheURLLoaderJob::DeleteSoon, GetDerivedWeakPtr()));
}

AppCacheURLLoaderJob::AppCacheURLLoaderJob(
    AppCacheURLLoaderRequest* appcache_request,
    AppCacheStorage* storage,
    NavigationLoaderInterceptor::LoaderCallback loader_callback)
    : storage_(storage->GetWeakPtr()),
      start_time_tick_(base::TimeTicks::Now()),
      cache_id_(kAppCacheNoCacheId),
      is_fallback_(false),
      binding_(this),
      writable_handle_watcher_(FROM_HERE,
                               mojo::SimpleWatcher::ArmingPolicy::MANUAL,
                               base::SequencedTaskRunnerHandle::Get()),
      loader_callback_(std::move(loader_callback)),
      appcache_request_(appcache_request->GetWeakPtr()),
      is_main_resource_load_(IsResourceTypeFrame(static_cast<ResourceType>(
          appcache_request->GetResourceRequest()->resource_type))),
      weak_factory_(this) {}

void AppCacheURLLoaderJob::CallLoaderCallback() {
  DCHECK(loader_callback_);
  DCHECK(!binding_.is_bound());
  std::move(loader_callback_)
      .Run(base::BindOnce(&AppCacheURLLoaderJob::Start, GetDerivedWeakPtr()));
  DCHECK(binding_.is_bound());
}

void AppCacheURLLoaderJob::OnResponseInfoLoaded(
    AppCacheResponseInfo* response_info,
    int64_t response_id) {
  DCHECK(IsDeliveringAppCacheResponse());

  if (!storage_.get()) {
    DeliverErrorResponse();
    return;
  }

  if (response_info) {
    if (loader_callback_)
      CallLoaderCallback();

    info_ = response_info;
    reader_.reset(
        storage_->CreateResponseReader(manifest_url_, entry_.response_id()));

    if (is_range_request())
      SetupRangeResponse();

    response_body_stream_ = std::move(data_pipe_.producer_handle);

    // TODO(ananta)
    // Move the asynchronous reading and mojo pipe handling code to a helper
    // class. That would also need a change to BlobURLLoader.

    // Wait for the data pipe to be ready to accept data.
    writable_handle_watcher_.Watch(
        response_body_stream_.get(), MOJO_HANDLE_SIGNAL_WRITABLE,
        base::Bind(&AppCacheURLLoaderJob::OnResponseBodyStreamReady,
                   GetDerivedWeakPtr()));

    SendResponseInfo();
    ReadMore();
    return;
  }

  // We failed to load the response headers from the disk cache.
  if (storage_->service()->storage() == storage_.get()) {
    // A resource that is expected to be in the appcache is missing.
    // If the 'check' fails, the corrupt appcache will be deleted.
    // See http://code.google.com/p/chromium/issues/detail?id=50657
    storage_->service()->CheckAppCacheResponse(manifest_url_, cache_id_,
                                               entry_.response_id());
    AppCacheHistograms::CountResponseRetrieval(
        false, is_main_resource_load_, url::Origin::Create(manifest_url_));
  }
  cache_entry_not_found_ = true;

  // We fallback to the network unless this job was falling back to the
  // appcache from the network which had already failed in some way.
  if (!is_fallback_)
    DeliverNetworkResponse();
  else
    DeliverErrorResponse();
}

void AppCacheURLLoaderJob::OnReadComplete(int result) {
  if (result > 0) {
    uint32_t bytes_written = static_cast<uint32_t>(result);
    response_body_stream_ = pending_write_->Complete(bytes_written);
    pending_write_ = nullptr;
    ReadMore();
    return;
  }

  writable_handle_watcher_.Cancel();
  pending_write_->Complete(0);
  pending_write_ = nullptr;
  NotifyCompleted(result);
}

void AppCacheURLLoaderJob::OnResponseBodyStreamReady(MojoResult result) {
  // TODO(ananta)
  // Add proper error handling here.
  if (result != MOJO_RESULT_OK) {
    DCHECK(false);
    NotifyCompleted(net::ERR_FAILED);
    return;
  }
  ReadMore();
}

void AppCacheURLLoaderJob::DeleteSoon() {
  if (storage_.get())
    storage_->CancelDelegateCallbacks(this);
  weak_factory_.InvalidateWeakPtrs();
  is_deleting_soon_ = true;
  base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, this);
}

void AppCacheURLLoaderJob::SendResponseInfo() {
  DCHECK(client_);
  // If this is null it means the response information was sent to the client.
  if (!data_pipe_.consumer_handle.is_valid())
    return;

  const net::HttpResponseInfo& http_info =
      is_range_request() ? *range_response_info_ : info_->http_response_info();
  network::ResourceResponseHead response_head;
  response_head.headers = http_info.headers;
  response_head.appcache_id = cache_id_;
  response_head.appcache_manifest_url = manifest_url_;

  http_info.headers->GetMimeType(&response_head.mime_type);
  http_info.headers->GetCharset(&response_head.charset);

  // TODO(ananta)
  // Verify if the times sent here are correct.
  response_head.request_time = http_info.request_time;
  response_head.response_time = http_info.response_time;
  response_head.content_length =
      is_range_request() ? range_response_info_->headers->GetContentLength()
                         : info_->response_data_size();
  response_head.connection_info = http_info.connection_info;
  response_head.socket_address = http_info.socket_address;
  response_head.was_fetched_via_spdy = http_info.was_fetched_via_spdy;
  response_head.was_alpn_negotiated = http_info.was_alpn_negotiated;
  response_head.alpn_negotiated_protocol = http_info.alpn_negotiated_protocol;
  if (http_info.ssl_info.cert)
    response_head.ssl_info = http_info.ssl_info;
  response_head.load_timing = load_timing_info_;

  client_->OnReceiveResponse(response_head);
  client_->OnStartLoadingResponseBody(std::move(data_pipe_.consumer_handle));
}

void AppCacheURLLoaderJob::ReadMore() {
  DCHECK(!pending_write_.get());

  uint32_t num_bytes;
  // TODO: we should use the abstractions in MojoAsyncResourceHandler.
  MojoResult result = network::NetToMojoPendingBuffer::BeginWrite(
      &response_body_stream_, &pending_write_, &num_bytes);
  if (result == MOJO_RESULT_SHOULD_WAIT) {
    // The pipe is full. We need to wait for it to have more space.
    writable_handle_watcher_.ArmOrNotify();
    return;
  } else if (result != MOJO_RESULT_OK) {
    NotifyCompleted(net::ERR_FAILED);
    writable_handle_watcher_.Cancel();
    response_body_stream_.reset();
    return;
  }

  CHECK_GT(static_cast<uint32_t>(std::numeric_limits<int>::max()), num_bytes);
  auto buffer =
      base::MakeRefCounted<network::NetToMojoIOBuffer>(pending_write_.get());

  uint32_t bytes_to_read =
      std::min<uint32_t>(num_bytes, info_->response_data_size());

  reader_->ReadData(buffer.get(), bytes_to_read,
                    base::BindOnce(&AppCacheURLLoaderJob::OnReadComplete,
                                   GetDerivedWeakPtr()));
}

void AppCacheURLLoaderJob::NotifyCompleted(int error_code) {
  if (storage_.get())
    storage_->CancelDelegateCallbacks(this);

  if (AppCacheRequestHandler::IsRunningInTests())
    return;

  network::URLLoaderCompletionStatus status(error_code);
  if (!error_code) {
    const net::HttpResponseInfo* http_info =
        is_range_request() ? range_response_info_.get()
                           : (info_ ? &info_->http_response_info() : nullptr);
    status.exists_in_cache = http_info->was_cached;
    status.completion_time = base::TimeTicks::Now();
    status.encoded_body_length =
        is_range_request() ? range_response_info_->headers->GetContentLength()
                           : (info_ ? info_->response_data_size() : 0);
    status.decoded_body_length = status.encoded_body_length;
  }
  client_->OnComplete(status);

  if (delivery_type_ == APPCACHED_DELIVERY) {
    AppCacheHistograms::CountResponseRetrieval(
        error_code == 0, is_main_resource_load_,
        url::Origin::Create(manifest_url_));
  }
}

}  // namespace content
