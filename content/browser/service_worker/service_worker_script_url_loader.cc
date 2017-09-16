// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_script_url_loader.h"

#include <memory>
#include "content/browser/appcache/appcache_response.h"
#include "content/browser/service_worker/service_worker_cache_writer.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_disk_cache.h"
#include "content/browser/service_worker/service_worker_storage.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/browser/service_worker/service_worker_write_to_cache_job.h"
#include "content/browser/url_loader_factory_getter.h"
#include "content/public/common/resource_response.h"

namespace content {

namespace {

// Buffer size for reading script data from network. We chose this size because
// the AppCache uses this.
const size_t kReadBufferSize = 32768;

}  // namespace

// TODO(nhiroki): We're doing multiple things in the ctor. Consider factors out
// some of them into a separate function.
ServiceWorkerScriptURLLoader::ServiceWorkerScriptURLLoader(
    int32_t routing_id,
    int32_t request_id,
    uint32_t options,
    const ResourceRequest& resource_request,
    mojom::URLLoaderClientPtr client,
    scoped_refptr<ServiceWorkerVersion> version,
    scoped_refptr<URLLoaderFactoryGetter> loader_factory_getter,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation)
    : request_url_(resource_request.url),
      resource_type_(resource_request.resource_type),
      version_(version),
      network_client_binding_(this),
      network_watcher_(FROM_HERE, mojo::SimpleWatcher::ArmingPolicy::MANUAL),
      client_(std::move(client)),
      weak_factory_(this) {
  DCHECK_EQ(ServiceWorkerVersion::NEW, version->status());
  DCHECK(resource_type_ == RESOURCE_TYPE_SERVICE_WORKER ||
         resource_type_ == RESOURCE_TYPE_SCRIPT);
  // TODO(nhiroki): Handle the case where |cache_resource_id| is invalid.
  int64_t cache_resource_id = version->context()->storage()->NewResourceId();

  // |incumbent_cache_resource_id| is valid if the incumbent service worker
  // exists and it's required to do the byte-for-byte check.
  int64_t incumbent_cache_resource_id = kInvalidServiceWorkerResourceId;
  if (resource_type_ == RESOURCE_TYPE_SERVICE_WORKER) {
    scoped_refptr<ServiceWorkerRegistration> registration =
        version_->context()->GetLiveRegistration(version_->registration_id());
    DCHECK(registration);
    ServiceWorkerVersion* stored_version = registration->waiting_version()
                                               ? registration->waiting_version()
                                               : registration->active_version();
    // |pause_after_download()| indicates the version is required to do the
    // byte-for-byte check.
    if (stored_version && stored_version->script_url() == request_url_ &&
        version_->pause_after_download()) {
      incumbent_cache_resource_id =
          stored_version->script_cache_map()->LookupResourceId(request_url_);
    }
  }

  // Create response readers only when we have to do the byte-for-byte check.
  std::unique_ptr<ServiceWorkerResponseReader> compare_reader;
  std::unique_ptr<ServiceWorkerResponseReader> copy_reader;
  ServiceWorkerStorage* storage = version_->context()->storage();
  if (incumbent_cache_resource_id != kInvalidServiceWorkerResourceId) {
    compare_reader = storage->CreateResponseReader(incumbent_cache_resource_id);
    copy_reader = storage->CreateResponseReader(incumbent_cache_resource_id);
  }
  cache_writer_ = std::make_unique<ServiceWorkerCacheWriter>(
      std::move(compare_reader), std::move(copy_reader),
      storage->CreateResponseWriter(cache_resource_id));

  version_->script_cache_map()->NotifyStartedCaching(request_url_,
                                                     cache_resource_id);
  AdvanceState(State::kStarted);

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

void ServiceWorkerScriptURLLoader::PauseCachingResponseBody() {
  network_loader_->PauseCachingResponseBody();
}

void ServiceWorkerScriptURLLoader::ResumeCachingResponseBody() {
  network_loader_->ResumeCachingResponseBody();
}

// URLLoaderClient for network loader ------------------------------------------

void ServiceWorkerScriptURLLoader::OnReceiveResponse(
    const ResourceResponseHead& response_head,
    const base::Optional<net::SSLInfo>& ssl_info,
    mojom::DownloadedTempFilePtr downloaded_file) {
  if (!version_->context() || version_->is_redundant()) {
    CommitCompleted(ResourceRequestCompletionStatus(net::ERR_FAILED));
    return;
  }

  // We don't have complete info here, but fill in what we have now.
  // At least we need headers and SSL info.
  auto response_info = std::make_unique<net::HttpResponseInfo>();
  response_info->headers = response_head.headers;
  if (ssl_info.has_value())
    response_info->ssl_info = *ssl_info;
  response_info->was_fetched_via_spdy = response_head.was_fetched_via_spdy;
  response_info->was_alpn_negotiated = response_head.was_alpn_negotiated;
  response_info->alpn_negotiated_protocol =
      response_head.alpn_negotiated_protocol;
  response_info->connection_info = response_head.connection_info;
  response_info->socket_address = response_head.socket_address;

  // TODO(nhiroki): Check the response code.

  // TODO(nhiroki): Check the path restriction.
  // (See ServiceWorkerWriteToCacheJob::CheckPathRestriction())

  // TODO(nhiroki): Check the SSL certificate.

  // TODO(nhiroki): Check the MIME type.

  if (resource_type_ == RESOURCE_TYPE_SERVICE_WORKER)
    version_->SetMainScriptHttpResponseInfo(*response_info);

  WriteHeaders(
      base::MakeRefCounted<HttpResponseInfoIOBuffer>(response_info.release()));

  client_->OnReceiveResponse(response_head, ssl_info,
                             std::move(downloaded_file));
}

void ServiceWorkerScriptURLLoader::OnReceiveRedirect(
    const net::RedirectInfo& redirect_info,
    const ResourceResponseHead& response_head) {
  client_->OnReceiveRedirect(redirect_info, response_head);
}

void ServiceWorkerScriptURLLoader::OnDataDownloaded(int64_t data_len,
                                                    int64_t encoded_data_len) {
  client_->OnDataDownloaded(data_len, encoded_data_len);
}

void ServiceWorkerScriptURLLoader::OnUploadProgress(
    int64_t current_position,
    int64_t total_size,
    OnUploadProgressCallback ack_callback) {
  client_->OnUploadProgress(current_position, total_size,
                            std::move(ack_callback));
}

void ServiceWorkerScriptURLLoader::OnReceiveCachedMetadata(
    const std::vector<uint8_t>& data) {
  client_->OnReceiveCachedMetadata(data);
}

void ServiceWorkerScriptURLLoader::OnTransferSizeUpdated(
    int32_t transfer_size_diff) {
  client_->OnTransferSizeUpdated(transfer_size_diff);
}

void ServiceWorkerScriptURLLoader::OnStartLoadingResponseBody(
    mojo::ScopedDataPipeConsumerHandle consumer) {
  // Create a pair of the consumer and producer for responding to the client.
  mojo::ScopedDataPipeConsumerHandle client_consumer;
  if (mojo::CreateDataPipe(nullptr, &client_producer_, &client_consumer) !=
      MOJO_RESULT_OK) {
    CommitCompleted(ResourceRequestCompletionStatus(net::ERR_FAILED));
    return;
  }

  // Pass the consumer handle for responding with the response to the client.
  client_->OnStartLoadingResponseBody(std::move(client_consumer));

  network_consumer_ = std::move(consumer);
  MaybeStartNetworkConsumerHandleWatcher();
}

void ServiceWorkerScriptURLLoader::OnComplete(
    const ResourceRequestCompletionStatus& status) {
  if (status.error_code != net::OK) {
    CommitCompleted(status);
    return;
  }

  network_load_completed_ = true;
  switch (state_) {
    case State::kNotStarted:
    case State::kCompleted:
      break;
    case State::kStarted:
    case State::kWroteHeaders:
      // CommitCompleted() will be called after the data is written in the
      // storage.
      return;
    case State::kWroteData:
      CommitCompleted(ResourceRequestCompletionStatus(net::OK));
      return;
  }
  NOTREACHED() << static_cast<int>(state_);
}

// End of URLLoaderClient ------------------------------------------------------

void ServiceWorkerScriptURLLoader::AdvanceState(State new_state) {
  switch (state_) {
    case State::kNotStarted:
      DCHECK_EQ(State::kStarted, new_state);
      break;
    case State::kStarted:
      DCHECK(new_state == State::kWroteHeaders ||
             new_state == State::kCompleted);
      break;
    case State::kWroteHeaders:
      DCHECK(new_state == State::kWroteData || new_state == State::kCompleted);
      break;
    case State::kWroteData:
      DCHECK_EQ(State::kCompleted, new_state);
      break;
    case State::kCompleted:
      // This is the end state.
      NOTREACHED();
      break;
  }
  state_ = new_state;
}

void ServiceWorkerScriptURLLoader::WriteHeaders(
    scoped_refptr<HttpResponseInfoIOBuffer> info_buffer) {
  net::Error error = cache_writer_->MaybeWriteHeaders(
      info_buffer.get(),
      base::Bind(&ServiceWorkerScriptURLLoader::OnWriteHeadersComplete,
                 weak_factory_.GetWeakPtr()));
  if (error == net::ERR_IO_PENDING) {
    // OnWriteHeadersComplete() will be called asynchronously.
    return;
  }
  // MaybeWriteHeaders() doesn't run the callback if it finishes synchronously,
  // so explicitly call it here.
  OnWriteHeadersComplete(error);
}

void ServiceWorkerScriptURLLoader::OnWriteHeadersComplete(net::Error error) {
  DCHECK_NE(net::ERR_IO_PENDING, error);
  if (error != net::OK) {
    CommitCompleted(ResourceRequestCompletionStatus(error));
    return;
  }
  AdvanceState(State::kWroteHeaders);
  MaybeStartNetworkConsumerHandleWatcher();
}

void ServiceWorkerScriptURLLoader::MaybeStartNetworkConsumerHandleWatcher() {
  if (!network_consumer_.is_valid()) {
    // Wait until the network consumer handle is ready to read.
    // OnStartLoadingResponseBody() will continue the sequence.
    return;
  }
  if (state_ != State::kWroteHeaders) {
    // Wait until the headers are written in the script storage because the
    // cache writer cannot write the headers and data in parallel.
    // OnWriteHeadersComplete() will continue the sequence.
    return;
  }
  network_watcher_.Watch(
      network_consumer_.get(),
      MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
      base::Bind(&ServiceWorkerScriptURLLoader::OnNetworkDataAvailable,
                 weak_factory_.GetWeakPtr()));
  network_watcher_.ArmOrNotify();
}

void ServiceWorkerScriptURLLoader::OnNetworkDataAvailable(MojoResult) {
  DCHECK(network_consumer_.is_valid());
  scoped_refptr<network::MojoToNetPendingBuffer> pending_buffer;
  uint32_t available = 0;
  MojoResult result = network::MojoToNetPendingBuffer::BeginRead(
      &network_consumer_, &pending_buffer, &available);
  switch (result) {
    case MOJO_RESULT_FAILED_PRECONDITION:
      // Closed by peer. This indicates all the data from the network service
      // are read or there is an error. In the error case, the reason is
      // notified via OnComplete().
      AdvanceState(State::kWroteData);
      if (network_load_completed_)
        CommitCompleted(ResourceRequestCompletionStatus(net::OK));
      return;
    case MOJO_RESULT_SHOULD_WAIT:
      network_watcher_.ArmOrNotify();
      return;
    case MOJO_RESULT_OK:
      break;
    default:
      // TODO(nhiroki): Currently we handle a few limited cases. Audit other
      // cases.
      NOTREACHED() << static_cast<int>(result);
      return;
  }

  // Read the received data to |buffer|.
  size_t bytes_to_be_read = std::min<size_t>(kReadBufferSize, available);
  auto buffer = base::MakeRefCounted<network::MojoToNetIOBuffer>(
      pending_buffer.get(), bytes_to_be_read);

  WriteData(std::move(buffer), bytes_to_be_read, std::move(pending_buffer));
}

void ServiceWorkerScriptURLLoader::WriteData(
    scoped_refptr<net::IOBuffer> buffer,
    size_t available_bytes,
    scoped_refptr<network::MojoToNetPendingBuffer> pending_buffer) {
  uint32_t bytes_written = available_bytes;
  MojoResult result = client_producer_->WriteData(
      buffer->data(), &bytes_written, MOJO_WRITE_DATA_FLAG_ALL_OR_NONE);
  switch (result) {
    case MOJO_RESULT_FAILED_PRECONDITION:
      CommitCompleted(ResourceRequestCompletionStatus(net::ERR_FAILED));
      return;
    case MOJO_RESULT_OK:
      break;
    default:
      // TODO(nhiroki): Currently we handle a few limited cases. Audit other
      // cases.
      NOTREACHED() << static_cast<int>(result);
      return;
  }

  net::Error error = cache_writer_->MaybeWriteData(
      buffer.get(), bytes_written,
      base::Bind(&ServiceWorkerScriptURLLoader::OnWriteDataComplete,
                 weak_factory_.GetWeakPtr(), bytes_written,
                 make_scoped_refptr(pending_buffer.get())));
  if (error == net::ERR_IO_PENDING) {
    // OnWriteDataComplete() will be called asynchronously.
    return;
  }
  // MaybeWriteData() doesn't run the callback if it finishes synchronously, so
  // explicitly call it here.
  OnWriteDataComplete(available_bytes, std::move(pending_buffer), error);
}

void ServiceWorkerScriptURLLoader::OnWriteDataComplete(
    size_t bytes_written,
    scoped_refptr<network::MojoToNetPendingBuffer> pending_buffer,
    net::Error error) {
  DCHECK_NE(net::ERR_IO_PENDING, error);
  if (error != net::OK) {
    CommitCompleted(ResourceRequestCompletionStatus(error));
    return;
  }
  DCHECK(pending_buffer);
  pending_buffer->CompleteRead(bytes_written);
  // Get the consumer handle from a previous read operation if we have one.
  network_consumer_ = pending_buffer->ReleaseHandle();
  pending_buffer = nullptr;
  network_watcher_.ArmOrNotify();
}

void ServiceWorkerScriptURLLoader::CommitCompleted(
    const ResourceRequestCompletionStatus& status) {
  AdvanceState(State::kCompleted);
  net::Error error_code = static_cast<net::Error>(status.error_code);
  int bytes_written = -1;
  std::string status_message;
  if (error_code == net::OK) {
    // If all the calls to WriteHeaders/WriteData succeeded, but the incumbent
    // entry wasn't actually replaced because the new entry was equivalent, the
    // new version didn't actually install because it already exists.
    if (!cache_writer_->did_replace()) {
      version_->SetStartWorkerStatusCode(SERVICE_WORKER_ERROR_EXISTS);
      status_message = ServiceWorkerWriteToCacheJob::kIdenticalScriptError;
    }
    bytes_written = cache_writer_->bytes_written();
  } else {
    // AddMessageConsole must be called before notifying that an error occurred
    // because the worker stops soon after receiving the error response.
    // TODO(nhiroki): Provide more accurate error message instead of
    // |kFetchScriptError|.
    // TODO(nhiroki): Consider replacing this hacky way with the new error code
    // handling mechanism in URLLoader.
    version_->embedded_worker()->AddMessageToConsole(
        blink::WebConsoleMessage::kLevelError, kFetchScriptError);
    // TODO(nhiroki): Set |status_message| to notify the error reason.
  }
  version_->script_cache_map()->NotifyFinishedCaching(
      request_url_, bytes_written, error_code, status_message);

  // TODO(nhiroki): Record ServiceWorkerMetrics::CountWriteResponseResult().
  // (https://crbug.com/762357)
  client_->OnComplete(status);
  client_producer_.reset();

  network_loader_.reset();
  network_client_binding_.Close();
  network_consumer_.reset();
  network_watcher_.Cancel();
  cache_writer_.reset();
}

}  // namespace content
