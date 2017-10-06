// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/service_worker/service_worker_subresource_loader.h"

#include "base/atomic_sequence_num.h"
#include "base/callback.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/child/service_worker/controller_service_worker_connector.h"
#include "content/common/service_worker/service_worker_loader_helpers.h"
#include "content/common/service_worker/service_worker_types.h"
#include "content/public/child/child_url_loader_factory_getter.h"
#include "content/public/common/content_features.h"
#include "ui/base/page_transition_types.h"

namespace content {

// ServiceWorkerSubresourceLoader -------------------------------------------

ServiceWorkerSubresourceLoader::ServiceWorkerSubresourceLoader(
    mojom::URLLoaderRequest request,
    int32_t routing_id,
    int32_t request_id,
    uint32_t options,
    const ResourceRequest& resource_request,
    mojom::URLLoaderClientPtr client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
    scoped_refptr<ControllerServiceWorkerConnector> controller_connector,
    scoped_refptr<ChildURLLoaderFactoryGetter> default_loader_factory_getter,
    const GURL& controller_origin,
    scoped_refptr<base::RefCountedData<storage::mojom::BlobRegistryPtr>>
        blob_registry)
    : url_loader_client_(std::move(client)),
      url_loader_binding_(this, std::move(request)),
      response_callback_binding_(this),
      controller_connector_(std::move(controller_connector)),
      routing_id_(routing_id),
      request_id_(request_id),
      options_(options),
      resource_request_(resource_request),
      traffic_annotation_(traffic_annotation),
      controller_origin_(controller_origin),
      blob_client_binding_(this),
      blob_registry_(std::move(blob_registry)),
      default_loader_factory_getter_(std::move(default_loader_factory_getter)),
      weak_factory_(this) {
  DCHECK(controller_connector_);
  url_loader_binding_.set_connection_error_handler(base::BindOnce(
      &ServiceWorkerSubresourceLoader::DeleteSoon, weak_factory_.GetWeakPtr()));
  StartRequest(resource_request);
}

ServiceWorkerSubresourceLoader::~ServiceWorkerSubresourceLoader() = default;

void ServiceWorkerSubresourceLoader::DeleteSoon() {
  base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, this);
}

void ServiceWorkerSubresourceLoader::StartRequest(
    const ResourceRequest& resource_request) {
  // TODO(kinuko): Implement request.request_body handling.
  DCHECK(!resource_request.request_body);
  std::unique_ptr<ServiceWorkerFetchRequest> request =
      ServiceWorkerLoaderHelpers::CreateFetchRequest(resource_request);
  DCHECK_EQ(Status::kNotStarted, status_);
  status_ = Status::kStarted;

  mojom::ServiceWorkerFetchResponseCallbackPtr response_callback_ptr;
  response_callback_binding_.Bind(mojo::MakeRequest(&response_callback_ptr));

  // At this point controller should be non-null.
  // TODO(kinuko): re-start the request if we get connection error before we
  // get response for this.
  controller_connector_->GetControllerServiceWorker()->DispatchFetchEvent(
      *request, std::move(response_callback_ptr),
      base::BindOnce(&ServiceWorkerSubresourceLoader::OnFetchEventFinished,
                     weak_factory_.GetWeakPtr()));
}

void ServiceWorkerSubresourceLoader::OnFetchEventFinished(
    blink::mojom::ServiceWorkerEventStatus status,
    base::Time dispatch_event_time) {
  switch (status) {
    case blink::mojom::ServiceWorkerEventStatus::COMPLETED:
      // ServiceWorkerFetchResponseCallback interface (OnResponse*() or
      // OnFallback() below) is expected to be called normally and handle this
      // request.
      break;
    case blink::mojom::ServiceWorkerEventStatus::REJECTED:
      // OnResponse() is expected to called with an error about the rejected
      // promise, and handle this request.
      break;
    case blink::mojom::ServiceWorkerEventStatus::ABORTED:
      // We have an unexpected error: fetch event dispatch failed. Return
      // network error.
      weak_factory_.InvalidateWeakPtrs();
      CommitCompleted(net::ERR_FAILED);
  }
}

void ServiceWorkerSubresourceLoader::OnResponse(
    const ServiceWorkerResponse& response,
    base::Time dispatch_event_time) {
  StartResponse(response, nullptr /* body_as_blob */,
                nullptr /* body_as_stream */);
}

void ServiceWorkerSubresourceLoader::OnResponseBlob(
    const ServiceWorkerResponse& response,
    storage::mojom::BlobPtr body_as_blob,
    base::Time dispatch_event_time) {
  StartResponse(response, std::move(body_as_blob),
                nullptr /* body_as_stream */);
}

void ServiceWorkerSubresourceLoader::OnResponseStream(
    const ServiceWorkerResponse& response,
    blink::mojom::ServiceWorkerStreamHandlePtr body_as_stream,
    base::Time dispatch_event_time) {
  StartResponse(response, nullptr /* body_as_blob */,
                std::move(body_as_stream));
}

void ServiceWorkerSubresourceLoader::OnFallback(
    base::Time dispatch_event_time) {
  DCHECK(default_loader_factory_getter_);
  // Hand over to the network loader.
  // Per spec, redirects after this point are not intercepted by the service
  // worker again. (https://crbug.com/517364)
  default_loader_factory_getter_->GetNetworkLoaderFactory()
      ->CreateLoaderAndStart(url_loader_binding_.Unbind(), routing_id_,
                             request_id_, options_, resource_request_,
                             std::move(url_loader_client_),
                             traffic_annotation_);
  DeleteSoon();
}

void ServiceWorkerSubresourceLoader::StartResponse(
    const ServiceWorkerResponse& response,
    storage::mojom::BlobPtr body_as_blob,
    blink::mojom::ServiceWorkerStreamHandlePtr body_as_stream) {
  // A response with status code 0 is Blink telling us to respond with network
  // error.
  if (response.status_code == 0) {
    CommitCompleted(net::ERR_FAILED);
    return;
  }

  ServiceWorkerLoaderHelpers::SaveResponseInfo(response, &response_head_);
  ServiceWorkerLoaderHelpers::SaveResponseHeaders(
      response.status_code, response.status_text, response.headers,
      &response_head_);

  // Handle a stream response body.
  if (!body_as_stream.is_null() && body_as_stream->stream.is_valid()) {
    CommitResponseHeaders();
    DCHECK(url_loader_client_.is_bound());
    url_loader_client_->OnStartLoadingResponseBody(
        std::move(body_as_stream->stream));
    // TODO(falken): Call CommitCompleted() when stream finished.
    // See https://crbug.com/758455
    CommitCompleted(net::OK);
    return;
  }

  // Handle a blob response body. Ideally we'd just get a data pipe from
  // SWFetchDispatcher, and this could be treated the same as a stream response.
  // See:
  // https://docs.google.com/a/google.com/document/d/1_ROmusFvd8ATwIZa29-P6Ls5yyLjfld0KvKchVfA84Y/edit?usp=drive_web
  // TODO(kinuko): This code is hacked up on top of the legacy API, migrate
  // to mojo-fied Blob code once it becomes ready.
  if (!response.blob_uuid.empty()) {
    GURL blob_url =
        GURL("blob:" + controller_origin_.spec() + "/" + response.blob_uuid);
    blob_registry_->data->RegisterURL(std::move(body_as_blob), blob_url,
                                      &blob_url_handle_);
    resource_request_.url = blob_url;

    mojom::URLLoaderClientPtr blob_loader_client;
    blob_client_binding_.Bind(mojo::MakeRequest(&blob_loader_client));
    default_loader_factory_getter_->GetBlobLoaderFactory()
        ->CreateLoaderAndStart(mojo::MakeRequest(&blob_loader_), routing_id_,
                               request_id_, options_, resource_request_,
                               std::move(blob_loader_client),
                               traffic_annotation_);
    return;
  }

  // The response has no body.
  CommitResponseHeaders();
  CommitCompleted(net::OK);
}

void ServiceWorkerSubresourceLoader::CommitResponseHeaders() {
  DCHECK_EQ(Status::kStarted, status_);
  DCHECK(url_loader_client_.is_bound());
  status_ = Status::kSentHeader;
  // TODO(kinuko): Fill the ssl_info.
  url_loader_client_->OnReceiveResponse(response_head_,
                                        base::nullopt /* ssl_info_ */,
                                        nullptr /* downloaded_file */);
}

void ServiceWorkerSubresourceLoader::CommitCompleted(int error_code) {
  DCHECK_LT(status_, Status::kCompleted);
  DCHECK(url_loader_client_.is_bound());
  status_ = Status::kCompleted;
  ResourceRequestCompletionStatus completion_status;
  completion_status.error_code = error_code;
  completion_status.completion_time = base::TimeTicks::Now();
  url_loader_client_->OnComplete(completion_status);
}

// ServiceWorkerSubresourceLoader: URLLoader implementation -----------------

void ServiceWorkerSubresourceLoader::FollowRedirect() {
  // TODO(kinuko): Need to implement for the cases where a redirect is returned
  // by a ServiceWorker and the page determined to follow the redirect.
  NOTIMPLEMENTED();
}

void ServiceWorkerSubresourceLoader::SetPriority(net::RequestPriority priority,
                                                 int intra_priority_value) {
  // Not supported (do nothing).
}

void ServiceWorkerSubresourceLoader::PauseReadingBodyFromNet() {}

void ServiceWorkerSubresourceLoader::ResumeReadingBodyFromNet() {}

// ServiceWorkerSubresourceLoader: URLLoaderClient for Blob reading ---------

void ServiceWorkerSubresourceLoader::OnReceiveResponse(
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

void ServiceWorkerSubresourceLoader::OnReceiveRedirect(
    const net::RedirectInfo& redirect_info,
    const ResourceResponseHead& response_head) {
  NOTREACHED();
}

void ServiceWorkerSubresourceLoader::OnDataDownloaded(
    int64_t data_len,
    int64_t encoded_data_len) {
  NOTREACHED();
}

void ServiceWorkerSubresourceLoader::OnUploadProgress(
    int64_t current_position,
    int64_t total_size,
    OnUploadProgressCallback ack_callback) {
  NOTREACHED();
}

void ServiceWorkerSubresourceLoader::OnReceiveCachedMetadata(
    const std::vector<uint8_t>& data) {
  // TODO(kinuko): Support this.
  NOTIMPLEMENTED();
}

void ServiceWorkerSubresourceLoader::OnTransferSizeUpdated(
    int32_t transfer_size_diff) {
  NOTREACHED();
}

void ServiceWorkerSubresourceLoader::OnStartLoadingResponseBody(
    mojo::ScopedDataPipeConsumerHandle body) {
  DCHECK(url_loader_client_.is_bound());
  url_loader_client_->OnStartLoadingResponseBody(std::move(body));
}

void ServiceWorkerSubresourceLoader::OnComplete(
    const ResourceRequestCompletionStatus& status) {
  DCHECK_EQ(Status::kSentHeader, status_);
  DCHECK(url_loader_client_.is_bound());
  status_ = Status::kCompleted;
  url_loader_client_->OnComplete(status);
}

// ServiceWorkerSubresourceLoaderFactory ------------------------------------

ServiceWorkerSubresourceLoaderFactory::ServiceWorkerSubresourceLoaderFactory(
    scoped_refptr<ControllerServiceWorkerConnector> controller_connector,
    scoped_refptr<ChildURLLoaderFactoryGetter> default_loader_factory_getter,
    const GURL& controller_origin,
    scoped_refptr<base::RefCountedData<storage::mojom::BlobRegistryPtr>>
        blob_registry)
    : controller_connector_(std::move(controller_connector)),
      default_loader_factory_getter_(std::move(default_loader_factory_getter)),
      controller_origin_(controller_origin),
      blob_registry_(std::move(blob_registry)) {
  DCHECK_EQ(controller_origin, controller_origin.GetOrigin());
  DCHECK(default_loader_factory_getter_);
}

ServiceWorkerSubresourceLoaderFactory::
    ~ServiceWorkerSubresourceLoaderFactory() = default;

void ServiceWorkerSubresourceLoaderFactory::CreateLoaderAndStart(
    mojom::URLLoaderRequest request,
    int32_t routing_id,
    int32_t request_id,
    uint32_t options,
    const ResourceRequest& resource_request,
    mojom::URLLoaderClientPtr client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  // This loader destructs itself, as we want to transparently switch to the
  // network loader when fallback happens. When that happens the loader unbinds
  // the request, passes the request to the Network Loader Factory, and
  // destructs itself (while the loader client continues to work).
  new ServiceWorkerSubresourceLoader(
      std::move(request), routing_id, request_id, options, resource_request,
      std::move(client), traffic_annotation, controller_connector_,
      default_loader_factory_getter_, controller_origin_, blob_registry_);
}

void ServiceWorkerSubresourceLoaderFactory::Clone(
    mojom::URLLoaderFactoryRequest request) {
  NOTREACHED();
}

}  // namespace content
