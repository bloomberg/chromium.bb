// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_url_loader_job.h"

#include "base/guid.h"
#include "base/optional.h"
#include "content/browser/blob_storage/blob_url_loader_factory.h"
#include "content/browser/service_worker/service_worker_fetch_dispatcher.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/browser/url_loader_factory_getter.h"
#include "content/common/service_worker/service_worker_loader_helpers.h"
#include "content/common/service_worker/service_worker_utils.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "storage/browser/blob/blob_data_builder.h"
#include "storage/browser/blob/blob_data_handle.h"
#include "storage/browser/blob/blob_impl.h"
#include "storage/browser/blob/blob_reader.h"
#include "storage/browser/blob/blob_storage_context.h"

namespace content {

// This class waits for completion of a stream response from the service worker.
// It calls ServiceWorkerURLLoader::CommitComplete() upon completion of the
// response.
class ServiceWorkerURLLoaderJob::StreamWaiter
    : public blink::mojom::ServiceWorkerStreamCallback {
 public:
  StreamWaiter(
      ServiceWorkerURLLoaderJob* owner,
      scoped_refptr<ServiceWorkerVersion> streaming_version,
      blink::mojom::ServiceWorkerStreamCallbackRequest callback_request)
      : owner_(owner),
        streaming_version_(streaming_version),
        binding_(this, std::move(callback_request)) {
    streaming_version_->OnStreamResponseStarted();
    binding_.set_connection_error_handler(
        base::BindOnce(&StreamWaiter::OnAborted, base::Unretained(this)));
  }
  ~StreamWaiter() override { streaming_version_->OnStreamResponseFinished(); }

  // Implements mojom::ServiceWorkerStreamCallback.
  void OnCompleted() override {
    // Destroys |this|.
    owner_->CommitCompleted(net::OK);
  }
  void OnAborted() override {
    // Destroys |this|.
    owner_->CommitCompleted(net::ERR_ABORTED);
  }

 private:
  ServiceWorkerURLLoaderJob* owner_;
  scoped_refptr<ServiceWorkerVersion> streaming_version_;
  mojo::Binding<blink::mojom::ServiceWorkerStreamCallback> binding_;

  DISALLOW_COPY_AND_ASSIGN(StreamWaiter);
};

ServiceWorkerURLLoaderJob::ServiceWorkerURLLoaderJob(
    LoaderCallback callback,
    Delegate* delegate,
    const ResourceRequest& resource_request,
    scoped_refptr<URLLoaderFactoryGetter> url_loader_factory_getter,
    base::WeakPtr<storage::BlobStorageContext> blob_storage_context)
    : loader_callback_(std::move(callback)),
      delegate_(delegate),
      resource_request_(resource_request),
      url_loader_factory_getter_(std::move(url_loader_factory_getter)),
      blob_storage_context_(blob_storage_context),
      blob_client_binding_(this),
      binding_(this),
      weak_factory_(this) {
  DCHECK_EQ(FETCH_REQUEST_MODE_NAVIGATE, resource_request_.fetch_request_mode);
  DCHECK_EQ(FETCH_CREDENTIALS_MODE_INCLUDE,
            resource_request_.fetch_credentials_mode);
  DCHECK_EQ(FetchRedirectMode::MANUAL_MODE,
            resource_request_.fetch_redirect_mode);
  response_head_.load_timing.request_start = base::TimeTicks::Now();
  response_head_.load_timing.request_start_time = base::Time::Now();
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
  status_ = Status::kCancelled;
  weak_factory_.InvalidateWeakPtrs();
  blob_storage_context_.reset();
  fetch_dispatcher_.reset();
  stream_waiter_.reset();

  url_loader_client_->OnComplete(
      ResourceRequestCompletionStatus(net::ERR_ABORTED));
  url_loader_client_.reset();
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
    ReturnNetworkError();
    return;
  }

  // Create the URL request to pass to the fetch event.
  std::unique_ptr<ServiceWorkerFetchRequest> fetch_request =
      ServiceWorkerLoaderHelpers::CreateFetchRequest(resource_request_);
  // TODO(emim): We should create an error when no blob_storage_context_, but
  // for consistency with StartResponse(), just ignore for now.
  if (resource_request_.request_body.get() && blob_storage_context_) {
    DCHECK(features::IsMojoBlobsEnabled());
    fetch_request->blob = CreateRequestBodyBlob();
  }

  // Dispatch the fetch event.
  fetch_dispatcher_ = std::make_unique<ServiceWorkerFetchDispatcher>(
      std::move(fetch_request), active_worker, resource_request_.resource_type,
      base::nullopt, net::NetLogWithSource() /* TODO(scottmg): net log? */,
      base::Bind(&ServiceWorkerURLLoaderJob::DidPrepareFetchEvent,
                 weak_factory_.GetWeakPtr(),
                 base::WrapRefCounted(active_worker)),
      base::Bind(&ServiceWorkerURLLoaderJob::DidDispatchFetchEvent,
                 weak_factory_.GetWeakPtr()));
  did_navigation_preload_ =
      fetch_dispatcher_->MaybeStartNavigationPreloadWithURLLoader(
          resource_request_, url_loader_factory_getter_.get(),
          base::BindOnce(&base::DoNothing /* TODO(crbug/762357): metrics? */));
  response_head_.service_worker_start_time = base::TimeTicks::Now();
  response_head_.load_timing.send_start = base::TimeTicks::Now();
  response_head_.load_timing.send_end = base::TimeTicks::Now();
  fetch_dispatcher_->Run();
}

scoped_refptr<storage::BlobHandle>
ServiceWorkerURLLoaderJob::CreateRequestBodyBlob() {
  storage::BlobDataBuilder blob_builder(base::GenerateGUID());
  // TODO(emim): Resolve file sizes before calling AppendIPCDataElement().
  for (const auto& element : (*resource_request_.request_body->elements())) {
    blob_builder.AppendIPCDataElement(element);
  }

  std::unique_ptr<storage::BlobDataHandle> request_body_blob_data_handle =
      blob_storage_context_->AddFinishedBlob(&blob_builder);
  // TODO(emim): Return a network error when the blob is broken.
  CHECK(!request_body_blob_data_handle->IsBroken());
  storage::mojom::BlobPtr blob_ptr;
  storage::BlobImpl::Create(std::move(request_body_blob_data_handle),
                            MakeRequest(&blob_ptr));
  return base::MakeRefCounted<storage::BlobHandle>(std::move(blob_ptr));
}

void ServiceWorkerURLLoaderJob::CommitResponseHeaders() {
  DCHECK_EQ(Status::kStarted, status_);
  DCHECK(url_loader_client_.is_bound());
  status_ = Status::kSentHeader;
  url_loader_client_->OnReceiveResponse(response_head_, ssl_info_,
                                        nullptr /* downloaded_file */);
}

void ServiceWorkerURLLoaderJob::CommitCompleted(int error_code) {
  DCHECK_LT(status_, Status::kCompleted);
  DCHECK(url_loader_client_.is_bound());
  status_ = Status::kCompleted;

  // |stream_waiter_| calls this when done.
  stream_waiter_.reset();

  url_loader_client_->OnComplete(ResourceRequestCompletionStatus(error_code));
}

void ServiceWorkerURLLoaderJob::ReturnNetworkError() {
  DCHECK(!url_loader_client_.is_bound());
  DCHECK(loader_callback_);
  std::move(loader_callback_)
      .Run(base::BindOnce(&ServiceWorkerURLLoaderJob::StartErrorResponse,
                          weak_factory_.GetWeakPtr()));
}

void ServiceWorkerURLLoaderJob::DidPrepareFetchEvent(
    scoped_refptr<ServiceWorkerVersion> version) {
  response_head_.service_worker_ready_time = base::TimeTicks::Now();
}

void ServiceWorkerURLLoaderJob::DidDispatchFetchEvent(
    ServiceWorkerStatusCode status,
    ServiceWorkerFetchEventResult fetch_result,
    const ServiceWorkerResponse& response,
    blink::mojom::ServiceWorkerStreamHandlePtr body_as_stream,
    storage::mojom::BlobPtr body_as_blob,
    const scoped_refptr<ServiceWorkerVersion>& version) {
  ServiceWorkerMetrics::URLRequestJobResult result =
      ServiceWorkerMetrics::REQUEST_JOB_ERROR_BAD_DELEGATE;
  if (!delegate_->RequestStillValid(&result)) {
    ReturnNetworkError();
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
    ReturnNetworkError();
    return;
  }

  // Creates a new HttpResponseInfo using the the ServiceWorker script's
  // HttpResponseInfo to show HTTPS padlock.
  // TODO(horo): When we support mixed-content (HTTP) no-cors requests from a
  // ServiceWorker, we have to check the security level of the responses.
  const net::HttpResponseInfo* main_script_http_info =
      version->GetMainScriptHttpResponseInfo();
  // TODO(kinuko)
  // Fix this here.
  if (main_script_http_info)
    ssl_info_ = main_script_http_info->ssl_info;

  std::move(loader_callback_)
      .Run(base::BindOnce(&ServiceWorkerURLLoaderJob::StartResponse,
                          weak_factory_.GetWeakPtr(), response, version,
                          std::move(body_as_stream), std::move(body_as_blob)));
}

void ServiceWorkerURLLoaderJob::StartResponse(
    const ServiceWorkerResponse& response,
    scoped_refptr<ServiceWorkerVersion> version,
    blink::mojom::ServiceWorkerStreamHandlePtr body_as_stream,
    storage::mojom::BlobPtr body_as_blob,
    mojom::URLLoaderRequest request,
    mojom::URLLoaderClientPtr client) {
  DCHECK(!binding_.is_bound());
  DCHECK(!url_loader_client_.is_bound());
  binding_.Bind(std::move(request));
  binding_.set_connection_error_handler(base::BindOnce(
      &ServiceWorkerURLLoaderJob::Cancel, base::Unretained(this)));
  url_loader_client_ = std::move(client);

  ServiceWorkerLoaderHelpers::SaveResponseInfo(response, &response_head_);
  ServiceWorkerLoaderHelpers::SaveResponseHeaders(
      response.status_code, response.status_text, response.headers,
      &response_head_);

  response_head_.did_service_worker_navigation_preload =
      did_navigation_preload_;
  response_head_.load_timing.receive_headers_end = base::TimeTicks::Now();

  // Handle a redirect response. ComputeRedirectInfo returns non-null redirect
  // info if the given response is a redirect.
  base::Optional<net::RedirectInfo> redirect_info =
      ServiceWorkerLoaderHelpers::ComputeRedirectInfo(
          resource_request_, response_head_,
          ssl_info_ && ssl_info_->token_binding_negotiated);
  if (redirect_info) {
    response_head_.encoded_data_length = 0;
    url_loader_client_->OnReceiveRedirect(*redirect_info, response_head_);
    status_ = Status::kCompleted;
    return;
  }

  // Handle a stream response body.
  if (!body_as_stream.is_null() && body_as_stream->stream.is_valid()) {
    stream_waiter_ = std::make_unique<StreamWaiter>(
        this, std::move(version), std::move(body_as_stream->callback_request));
    CommitResponseHeaders();
    url_loader_client_->OnStartLoadingResponseBody(
        std::move(body_as_stream->stream));
    // StreamWaiter will call CommitCompleted() when done.
    return;
  }

  // Handle a blob response body. Ideally we'd just get a data pipe from
  // SWFetchDispatcher, and this could be treated the same as a stream response.
  // |body_as_blob| must be kept around until here to ensure the blob is alive.
  // See:
  // https://docs.google.com/a/google.com/document/d/1_ROmusFvd8ATwIZa29-P6Ls5yyLjfld0KvKchVfA84Y/edit?usp=drive_web
  if (!response.blob_uuid.empty() && blob_storage_context_) {
    std::unique_ptr<storage::BlobDataHandle> blob_data_handle =
        blob_storage_context_->GetBlobDataFromUUID(response.blob_uuid);
    mojom::URLLoaderRequest request;
    mojom::URLLoaderClientPtr client;
    blob_client_binding_.Bind(mojo::MakeRequest(&client));
    BlobURLLoaderFactory::CreateLoaderAndStart(
        std::move(request), resource_request_, std::move(client),
        std::move(blob_data_handle), nullptr /* file_system_context */);
    return;
  }

  // The response has no body.
  CommitResponseHeaders();
  CommitCompleted(net::OK);
}

void ServiceWorkerURLLoaderJob::StartErrorResponse(
    mojom::URLLoaderRequest request,
    mojom::URLLoaderClientPtr client) {
  DCHECK_EQ(Status::kStarted, status_);
  DCHECK(!url_loader_client_.is_bound());
  url_loader_client_ = std::move(client);
  CommitCompleted(net::ERR_FAILED);
}

// URLLoader implementation----------------------------------------

void ServiceWorkerURLLoaderJob::FollowRedirect() {
  NOTIMPLEMENTED();
}

void ServiceWorkerURLLoaderJob::SetPriority(net::RequestPriority priority,
                                            int32_t intra_priority_value) {
  NOTIMPLEMENTED();
}

void ServiceWorkerURLLoaderJob::PauseReadingBodyFromNet() {}

void ServiceWorkerURLLoaderJob::ResumeReadingBodyFromNet() {}

// Blob URLLoaderClient implementation for blob reading ------------

void ServiceWorkerURLLoaderJob::OnReceiveResponse(
    const ResourceResponseHead& response_head,
    const base::Optional<net::SSLInfo>& ssl_info,
    mojom::DownloadedTempFilePtr downloaded_file) {
  DCHECK_EQ(Status::kStarted, status_);
  DCHECK(url_loader_client_.is_bound());
  status_ = Status::kSentHeader;
  if (response_head.headers->response_code() >= 400) {
    DVLOG(1) << "Blob::OnReceiveResponse got error: "
             << response_head.headers->response_code();
    response_head_.headers = response_head.headers;
  }
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
  DCHECK(url_loader_client_.is_bound());
  url_loader_client_->OnStartLoadingResponseBody(std::move(body));
}

void ServiceWorkerURLLoaderJob::OnComplete(
    const ResourceRequestCompletionStatus& status) {
  DCHECK_EQ(Status::kSentHeader, status_);
  DCHECK(url_loader_client_.is_bound());
  status_ = Status::kCompleted;
  url_loader_client_->OnComplete(status);
}

}  // namespace content
