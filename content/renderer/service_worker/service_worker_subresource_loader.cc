// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/service_worker/service_worker_subresource_loader.h"

#include "base/atomic_sequence_num.h"
#include "base/callback.h"
#include "base/optional.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/common/service_worker/service_worker_loader_helpers.h"
#include "content/common/service_worker/service_worker_types.h"
#include "content/public/common/content_features.h"
#include "content/public/renderer/child_url_loader_factory_getter.h"
#include "content/renderer/service_worker/controller_service_worker_connector.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "net/url_request/redirect_info.h"
#include "net/url_request/redirect_util.h"
#include "ui/base/page_transition_types.h"

namespace content {

namespace {

// Max number of http redirects to follow. The Fetch spec says: "If request’s
// redirect count is twenty, return a network error."
// https://fetch.spec.whatwg.org/#http-redirect-fetch
const int kMaxRedirects = 20;

ResourceResponseHead RewriteServiceWorkerTime(
    base::TimeTicks service_worker_start_time,
    base::TimeTicks service_worker_ready_time,
    const ResourceResponseHead& response_head) {
  ResourceResponseHead new_head = response_head;
  new_head.service_worker_start_time = service_worker_start_time;
  new_head.service_worker_ready_time = service_worker_ready_time;
  return new_head;
}

// A wrapper URLLoaderClient that invokes given RewriteHeaderCallback whenever
// response or redirect is received. It self-destruct itself when the Mojo
// connection is closed.
class HeaderRewritingURLLoaderClient : public mojom::URLLoaderClient {
 public:
  using RewriteHeaderCallback =
      base::Callback<ResourceResponseHead(const ResourceResponseHead&)>;

  static mojom::URLLoaderClientPtr CreateAndBind(
      mojom::URLLoaderClientPtr url_loader_client,
      RewriteHeaderCallback rewrite_header_callback) {
    return (new HeaderRewritingURLLoaderClient(std::move(url_loader_client),
                                               rewrite_header_callback))
        ->CreateInterfacePtrAndBind();
  }

  ~HeaderRewritingURLLoaderClient() override {}

 private:
  HeaderRewritingURLLoaderClient(mojom::URLLoaderClientPtr url_loader_client,
                                 RewriteHeaderCallback rewrite_header_callback)
      : url_loader_client_(std::move(url_loader_client)),
        binding_(this),
        rewrite_header_callback_(rewrite_header_callback) {}

  mojom::URLLoaderClientPtr CreateInterfacePtrAndBind() {
    DCHECK(!binding_.is_bound());
    mojom::URLLoaderClientPtr ptr;
    binding_.Bind(mojo::MakeRequest(&ptr));
    binding_.set_connection_error_handler(
        base::Bind(&HeaderRewritingURLLoaderClient::OnClientConnectionError,
                   base::Unretained(this)));
    return ptr;
  }

  void OnClientConnectionError() {
    base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, this);
  }

  // mojom::URLLoaderClient implementation:
  void OnReceiveResponse(
      const ResourceResponseHead& response_head,
      const base::Optional<net::SSLInfo>& ssl_info,
      mojom::DownloadedTempFilePtr downloaded_file) override {
    DCHECK(url_loader_client_.is_bound());
    url_loader_client_->OnReceiveResponse(
        rewrite_header_callback_.Run(response_head), ssl_info,
        std::move(downloaded_file));
  }

  void OnReceiveRedirect(const net::RedirectInfo& redirect_info,
                         const ResourceResponseHead& response_head) override {
    DCHECK(url_loader_client_.is_bound());
    url_loader_client_->OnReceiveRedirect(
        redirect_info, rewrite_header_callback_.Run(response_head));
  }

  void OnDataDownloaded(int64_t data_len, int64_t encoded_data_len) override {
    DCHECK(url_loader_client_.is_bound());
    url_loader_client_->OnDataDownloaded(data_len, encoded_data_len);
  }

  void OnUploadProgress(int64_t current_position,
                        int64_t total_size,
                        OnUploadProgressCallback ack_callback) override {
    DCHECK(url_loader_client_.is_bound());
    url_loader_client_->OnUploadProgress(current_position, total_size,
                                         std::move(ack_callback));
  }

  void OnReceiveCachedMetadata(const std::vector<uint8_t>& data) override {
    DCHECK(url_loader_client_.is_bound());
    url_loader_client_->OnReceiveCachedMetadata(data);
  }

  void OnTransferSizeUpdated(int32_t transfer_size_diff) override {
    DCHECK(url_loader_client_.is_bound());
    url_loader_client_->OnTransferSizeUpdated(transfer_size_diff);
  }

  void OnStartLoadingResponseBody(
      mojo::ScopedDataPipeConsumerHandle body) override {
    DCHECK(url_loader_client_.is_bound());
    url_loader_client_->OnStartLoadingResponseBody(std::move(body));
  }

  void OnComplete(const ResourceRequestCompletionStatus& status) override {
    DCHECK(url_loader_client_.is_bound());
    url_loader_client_->OnComplete(status);
  }

  mojom::URLLoaderClientPtr url_loader_client_;
  mojo::Binding<mojom::URLLoaderClient> binding_;
  RewriteHeaderCallback rewrite_header_callback_;
};

}  // namespace

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
    scoped_refptr<base::RefCountedData<blink::mojom::BlobRegistryPtr>>
        blob_registry)
    : redirect_limit_(kMaxRedirects),
      url_loader_client_(std::move(client)),
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
  response_head_.request_start = base::TimeTicks::Now();
  response_head_.load_timing.request_start = base::TimeTicks::Now();
  response_head_.load_timing.request_start_time = base::Time::Now();
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

  response_head_.service_worker_start_time = base::TimeTicks::Now();
  // TODO(horo): Reset |service_worker_ready_time| when the the connection to
  // the service worker is revived.
  response_head_.service_worker_ready_time = base::TimeTicks::Now();
  response_head_.load_timing.send_start = base::TimeTicks::Now();
  response_head_.load_timing.send_end = base::TimeTicks::Now();
  // At this point controller should be non-null.
  // TODO(kinuko): re-start the request if we get connection error before we
  // get response for this.
  // TODO(kinuko): Implement request timeout and ask the browser to kill
  // the controller if it takes too long. (crbug.com/774374)
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
    blink::mojom::BlobPtr body_as_blob,
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
  // When the request mode is CORS or CORS-with-forced-preflight and the origin
  // of the request URL is different from the security origin of the document,
  // we can't simply fallback to the network here. It is because the CORS
  // preflight logic is implemented in Blink. So we return a "fallback required"
  // response to Blink.
  if ((resource_request_.fetch_request_mode == FETCH_REQUEST_MODE_CORS ||
       resource_request_.fetch_request_mode ==
           FETCH_REQUEST_MODE_CORS_WITH_FORCED_PREFLIGHT) &&
      (!resource_request_.request_initiator.has_value() ||
       !resource_request_.request_initiator->IsSameOriginWith(
           url::Origin::Create(resource_request_.url)))) {
    response_head_.was_fetched_via_service_worker = true;
    response_head_.was_fallback_required_by_service_worker = true;
    CommitResponseHeaders();
    CommitCompleted(net::OK);
    return;
  }

  // Hand over to the network loader.
  // Per spec, redirects after this point are not intercepted by the service
  // worker again. (https://crbug.com/517364)
  default_loader_factory_getter_->GetNetworkLoaderFactory()
      ->CreateLoaderAndStart(
          url_loader_binding_.Unbind(), routing_id_, request_id_, options_,
          resource_request_,
          HeaderRewritingURLLoaderClient::CreateAndBind(
              std::move(url_loader_client_),
              base::Bind(&RewriteServiceWorkerTime,
                         response_head_.service_worker_start_time,
                         response_head_.service_worker_ready_time)),
          traffic_annotation_);
  DeleteSoon();
}

void ServiceWorkerSubresourceLoader::StartResponse(
    const ServiceWorkerResponse& response,
    blink::mojom::BlobPtr body_as_blob,
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
  response_head_.response_start = base::TimeTicks::Now();
  response_head_.load_timing.receive_headers_end = base::TimeTicks::Now();

  // Handle a redirect response. ComputeRedirectInfo returns non-null redirect
  // info if the given response is a redirect.
  redirect_info_ = ServiceWorkerLoaderHelpers::ComputeRedirectInfo(
      resource_request_, response_head_, false /* token_binding_negotiated */);
  if (redirect_info_) {
    if (redirect_limit_-- == 0) {
      CommitCompleted(net::ERR_TOO_MANY_REDIRECTS);
      return;
    }
    response_head_.encoded_data_length = 0;
    url_loader_client_->OnReceiveRedirect(*redirect_info_, response_head_);
    status_ = Status::kCompleted;
    return;
  }

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
  DCHECK(redirect_info_);

  bool should_clear_upload = false;
  net::RedirectUtil::UpdateHttpRequest(
      resource_request_.url, resource_request_.method, *redirect_info_,
      &resource_request_.headers, &should_clear_upload);
  if (should_clear_upload)
    resource_request_.request_body = nullptr;

  resource_request_.url = redirect_info_->new_url;
  resource_request_.method = redirect_info_->new_method;
  resource_request_.site_for_cookies = redirect_info_->new_site_for_cookies;
  resource_request_.referrer = GURL(redirect_info_->new_referrer);
  resource_request_.referrer_policy =
      Referrer::NetReferrerPolicyToBlinkReferrerPolicy(
          redirect_info_->new_referrer_policy);

  // Restart the request.
  status_ = Status::kNotStarted;
  redirect_info_.reset();
  response_callback_binding_.Close();
  // We don't support the body of redirect responses.
  DCHECK(!blob_loader_);
  StartRequest(resource_request_);
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
    scoped_refptr<base::RefCountedData<blink::mojom::BlobRegistryPtr>>
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
