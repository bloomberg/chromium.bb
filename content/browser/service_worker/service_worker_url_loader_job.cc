// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_url_loader_job.h"

#include "base/strings/stringprintf.h"
#include "content/browser/blob_storage/blob_url_loader_factory.h"
#include "content/browser/service_worker/service_worker_fetch_dispatcher.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/common/service_worker/service_worker_utils.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/common/content_switches.h"
#include "net/base/io_buffer.h"
#include "net/http/http_util.h"
#include "storage/browser/blob/blob_reader.h"
#include "storage/browser/blob/blob_storage_context.h"

namespace content {

ServiceWorkerURLLoaderJob::ServiceWorkerURLLoaderJob(
    LoaderCallback callback,
    Delegate* delegate,
    const ResourceRequest& resource_request,
    base::WeakPtr<storage::BlobStorageContext> blob_storage_context)
    : loader_callback_(std::move(callback)),
      delegate_(delegate),
      resource_request_(resource_request),
      blob_storage_context_(blob_storage_context),
      blob_client_binding_(this),
      binding_(this),
      weak_factory_(this) {
  DCHECK(ServiceWorkerUtils::IsServicificationEnabled());
}

ServiceWorkerURLLoaderJob::~ServiceWorkerURLLoaderJob() {}

void ServiceWorkerURLLoaderJob::FallbackToNetwork() {
  response_type_ = FALLBACK_TO_NETWORK;
  // This could be called multiple times in some cases because we simply
  // call this synchronously here and don't wait for a separate async
  // StartRequest cue like what URLRequestJob case does.
  // TODO(kinuko): Make sure this is ok or we need to make this async.
  if (!loader_callback_.is_null())
    std::move(loader_callback_).Run(StartLoaderCallback());
}

void ServiceWorkerURLLoaderJob::FallbackToNetworkOrRenderer() {
  // TODO(kinuko): Implement this. Now we always fallback to network.
  FallbackToNetwork();
}

void ServiceWorkerURLLoaderJob::ForwardToServiceWorker() {
  response_type_ = FORWARD_TO_SERVICE_WORKER;
  StartRequest();
}

bool ServiceWorkerURLLoaderJob::ShouldFallbackToNetwork() {
  return response_type_ == FALLBACK_TO_NETWORK;
}

ui::PageTransition ServiceWorkerURLLoaderJob::GetPageTransition() {
  NOTIMPLEMENTED();
  return ui::PAGE_TRANSITION_LINK;
}

size_t ServiceWorkerURLLoaderJob::GetURLChainSize() const {
  NOTIMPLEMENTED();
  return 0;
}

void ServiceWorkerURLLoaderJob::FailDueToLostController() {
  NOTIMPLEMENTED();
}

void ServiceWorkerURLLoaderJob::Cancel() {
  url_loader_client_.reset();
  status_ = Status::kCancelled;
  weak_factory_.InvalidateWeakPtrs();
  blob_storage_context_.reset();
  fetch_dispatcher_.reset();
}

bool ServiceWorkerURLLoaderJob::WasCanceled() const {
  return status_ == Status::kCancelled;
}

void ServiceWorkerURLLoaderJob::StartRequest() {
  DCHECK_EQ(FORWARD_TO_SERVICE_WORKER, response_type_);
  DCHECK_EQ(Status::kNotStarted, status_);
  status_ = Status::kStarted;

  ServiceWorkerMetrics::URLRequestJobResult result =
      ServiceWorkerMetrics::REQUEST_JOB_ERROR_BAD_DELEGATE;
  ServiceWorkerVersion* active_worker =
      delegate_->GetServiceWorkerVersion(&result);
  if (!active_worker) {
    DeliverErrorResponse();
    return;
  }

  fetch_dispatcher_.reset(new ServiceWorkerFetchDispatcher(
      CreateFetchRequest(resource_request_), active_worker,
      resource_request_.resource_type, base::nullopt,
      net::NetLogWithSource() /* TODO(scottmg): net log? */,
      base::Bind(&ServiceWorkerURLLoaderJob::DidPrepareFetchEvent,
                 weak_factory_.GetWeakPtr(), active_worker),
      base::Bind(&ServiceWorkerURLLoaderJob::DidDispatchFetchEvent,
                 weak_factory_.GetWeakPtr())));
  // TODO(kinuko): Handle navigation preload.
  fetch_dispatcher_->Run();
}

std::unique_ptr<ServiceWorkerFetchRequest>
ServiceWorkerURLLoaderJob::CreateFetchRequest(const ResourceRequest& request) {
  std::string blob_uuid;
  uint64_t blob_size = 0;
  // TODO(scottmg): Implement passing body as blob to handler.
  DCHECK(!request.request_body);
  auto new_request = base::MakeUnique<ServiceWorkerFetchRequest>();
  new_request->mode = request.fetch_request_mode;
  new_request->is_main_resource_load =
      ServiceWorkerUtils::IsMainResourceType(request.resource_type);
  new_request->request_context_type = request.fetch_request_context_type;
  new_request->frame_type = request.fetch_frame_type;
  new_request->url = request.url;
  new_request->method = request.method;
  new_request->blob_uuid = blob_uuid;
  new_request->blob_size = blob_size;
  new_request->credentials_mode = request.fetch_credentials_mode;
  new_request->redirect_mode = request.fetch_redirect_mode;
  new_request->is_reload = ui::PageTransitionCoreTypeIs(
      request.transition_type, ui::PAGE_TRANSITION_RELOAD);
  new_request->referrer =
      Referrer(GURL(request.referrer), request.referrer_policy);
  new_request->fetch_type = ServiceWorkerFetchType::FETCH;
  return new_request;
}

void ServiceWorkerURLLoaderJob::SaveResponseInfo(
    const ServiceWorkerResponse& response) {
  response_head_.was_fetched_via_service_worker = true;
  response_head_.was_fetched_via_foreign_fetch = false;
  response_head_.was_fallback_required_by_service_worker = false;
  response_head_.url_list_via_service_worker = response.url_list;
  response_head_.response_type_via_service_worker = response.response_type;
  response_head_.is_in_cache_storage = response.is_in_cache_storage;
  response_head_.cache_storage_cache_name = response.cache_storage_cache_name;
  response_head_.cors_exposed_header_names = response.cors_exposed_header_names;
  response_head_.did_service_worker_navigation_preload =
      did_navigation_preload_;
}

void ServiceWorkerURLLoaderJob::SaveResponseHeaders(
    int status_code,
    const std::string& status_text,
    const ServiceWorkerHeaderMap& headers) {
  // Build a string instead of using HttpResponseHeaders::AddHeader on
  // each header, since AddHeader has O(n^2) performance.
  std::string buf(base::StringPrintf("HTTP/1.1 %d %s\r\n", status_code,
                                     status_text.c_str()));
  for (const auto& item : headers) {
    buf.append(item.first);
    buf.append(": ");
    buf.append(item.second);
    buf.append("\r\n");
  }
  buf.append("\r\n");

  response_head_.headers = new net::HttpResponseHeaders(
      net::HttpUtil::AssembleRawHeaders(buf.c_str(), buf.size()));
  if (response_head_.mime_type.empty()) {
    std::string mime_type;
    response_head_.headers->GetMimeType(&mime_type);
    if (mime_type.empty())
      mime_type = "text/plain";
    response_head_.mime_type = mime_type;
  }
}

void ServiceWorkerURLLoaderJob::CommitResponseHeaders() {
  DCHECK_EQ(Status::kStarted, status_);
  status_ = Status::kSentHeader;
  url_loader_client_->OnReceiveResponse(response_head_, ssl_info_, nullptr);
}

void ServiceWorkerURLLoaderJob::CommitCompleted(int error_code) {
  DCHECK_LT(status_, Status::kCompleted);
  status_ = Status::kCompleted;
  ResourceRequestCompletionStatus completion_status;
  completion_status.error_code = error_code;
  completion_status.completion_time = base::TimeTicks::Now();
  url_loader_client_->OnComplete(completion_status);
}

void ServiceWorkerURLLoaderJob::DeliverErrorResponse() {
  DCHECK_GT(status_, Status::kNotStarted);
  DCHECK_LT(status_, Status::kCompleted);
  if (status_ < Status::kSentHeader) {
    SaveResponseHeaders(500, "Service Worker Response Error",
                        ServiceWorkerHeaderMap());
    CommitResponseHeaders();
  }
  CommitCompleted(net::ERR_FAILED);
}

void ServiceWorkerURLLoaderJob::DidPrepareFetchEvent(
    scoped_refptr<ServiceWorkerVersion> version) {}

void ServiceWorkerURLLoaderJob::DidDispatchFetchEvent(
    ServiceWorkerStatusCode status,
    ServiceWorkerFetchEventResult fetch_result,
    const ServiceWorkerResponse& response,
    blink::mojom::ServiceWorkerStreamHandlePtr body_as_stream,
    const scoped_refptr<ServiceWorkerVersion>& version) {
  if (!did_navigation_preload_)
    fetch_dispatcher_.reset();

  ServiceWorkerMetrics::URLRequestJobResult result =
      ServiceWorkerMetrics::REQUEST_JOB_ERROR_BAD_DELEGATE;
  if (!delegate_->RequestStillValid(&result)) {
    DeliverErrorResponse();
    return;
  }

  if (status != SERVICE_WORKER_OK) {
    delegate_->MainResourceLoadFailed();
    FallbackToNetwork();
    return;
  }

  if (fetch_result == SERVICE_WORKER_FETCH_EVENT_RESULT_FALLBACK) {
    // TODO(kinuko): Check if this needs to fallback to the renderer.
    FallbackToNetwork();
    return;
  }

  DCHECK_EQ(fetch_result, SERVICE_WORKER_FETCH_EVENT_RESULT_RESPONSE);

  // A response with status code 0 is Blink telling us to respond with
  // network error.
  if (response.status_code == 0) {
    DeliverErrorResponse();
    return;
  }

  // Creates a new HttpResponseInfo using the the ServiceWorker script's
  // HttpResponseInfo to show HTTPS padlock.
  // TODO(horo): When we support mixed-content (HTTP) no-cors requests from a
  // ServiceWorker, we have to check the security level of the responses.
  const net::HttpResponseInfo* main_script_http_info =
      version->GetMainScriptHttpResponseInfo();
  DCHECK(main_script_http_info);
  ssl_info_ = main_script_http_info->ssl_info;

  std::move(loader_callback_)
      .Run(base::Bind(&ServiceWorkerURLLoaderJob::StartResponse,
                      weak_factory_.GetWeakPtr(), response,
                      base::Passed(std::move(body_as_stream))));
}

void ServiceWorkerURLLoaderJob::StartResponse(
    const ServiceWorkerResponse& response,
    blink::mojom::ServiceWorkerStreamHandlePtr body_as_stream,
    mojom::URLLoaderRequest request,
    mojom::URLLoaderClientPtr client) {
  DCHECK(!binding_.is_bound());
  binding_.Bind(std::move(request));
  binding_.set_connection_error_handler(
      base::Bind(&ServiceWorkerURLLoaderJob::Cancel, base::Unretained(this)));
  url_loader_client_ = std::move(client);

  SaveResponseInfo(response);
  SaveResponseHeaders(response.status_code, response.status_text,
                      response.headers);

  // TODO(kinuko): Set the script's HTTP response info via
  // version->GetMainScriptHttpResponseInfo() for HTTPS padlock.

  // Ideally, we would always get a data pipe fom SWFetchDispatcher and use
  // this case. See:
  // https://docs.google.com/a/google.com/document/d/1_ROmusFvd8ATwIZa29-P6Ls5yyLjfld0KvKchVfA84Y/edit?usp=drive_web
  if (!body_as_stream.is_null() && body_as_stream->stream.is_valid()) {
    CommitResponseHeaders();
    url_loader_client_->OnStartLoadingResponseBody(
        std::move(body_as_stream->stream));
    CommitCompleted(net::OK);
    return;
  }

  if (!response.blob_uuid.empty() && blob_storage_context_) {
    std::unique_ptr<storage::BlobDataHandle> blob_data_handle =
        blob_storage_context_->GetBlobDataFromUUID(response.blob_uuid);
    mojom::URLLoaderAssociatedRequest request;
    mojom::URLLoaderClientPtr client;
    blob_client_binding_.Bind(mojo::MakeRequest(&client));
    BlobURLLoaderFactory::CreateLoaderAndStart(
        std::move(request), resource_request_, std::move(client),
        std::move(blob_data_handle), nullptr /* file_system_context */);
  }
}

// URLLoader implementation----------------------------------------

void ServiceWorkerURLLoaderJob::FollowRedirect() {
  NOTIMPLEMENTED();
}

void ServiceWorkerURLLoaderJob::SetPriority(net::RequestPriority priority,
                                            int32_t intra_priority_value) {
  NOTIMPLEMENTED();
}

// Blob URLLoaderClient implementation for blob reading ------------

void ServiceWorkerURLLoaderJob::OnReceiveResponse(
    const ResourceResponseHead& response_head,
    const base::Optional<net::SSLInfo>& ssl_info,
    mojom::DownloadedTempFilePtr downloaded_file) {
  DCHECK_EQ(Status::kStarted, status_);
  status_ = Status::kSentHeader;
  if (response_head.headers->response_code() >= 400)
    response_head_.headers = response_head.headers;
  url_loader_client_->OnReceiveResponse(response_head_, ssl_info,
                                        std::move(downloaded_file));
}

void ServiceWorkerURLLoaderJob::OnReceiveRedirect(
    const net::RedirectInfo& redirect_info,
    const ResourceResponseHead& response_head) {
  NOTREACHED();
}

void ServiceWorkerURLLoaderJob::OnDataDownloaded(int64_t data_len,
                                                 int64_t encoded_data_len) {
  NOTREACHED();
}

void ServiceWorkerURLLoaderJob::OnUploadProgress(
    int64_t current_position,
    int64_t total_size,
    OnUploadProgressCallback ack_callback) {
  NOTREACHED();
}

void ServiceWorkerURLLoaderJob::OnReceiveCachedMetadata(
    const std::vector<uint8_t>& data) {
  NOTREACHED();
}

void ServiceWorkerURLLoaderJob::OnTransferSizeUpdated(
    int32_t transfer_size_diff) {
  NOTREACHED();
}

void ServiceWorkerURLLoaderJob::OnStartLoadingResponseBody(
    mojo::ScopedDataPipeConsumerHandle body) {
  url_loader_client_->OnStartLoadingResponseBody(std::move(body));
}

void ServiceWorkerURLLoaderJob::OnComplete(
    const ResourceRequestCompletionStatus& status) {
  DCHECK_EQ(Status::kSentHeader, status_);
  status_ = Status::kCompleted;
  url_loader_client_->OnComplete(status);
}

}  // namespace content
