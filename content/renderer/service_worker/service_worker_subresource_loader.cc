// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/service_worker/service_worker_subresource_loader.h"

#include "base/atomic_sequence_num.h"
#include "base/callback.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "base/optional.h"
#include "base/trace_event/trace_event.h"
#include "content/common/service_worker/service_worker_loader_helpers.h"
#include "content/common/service_worker/service_worker_types.h"
#include "content/common/service_worker/service_worker_utils.h"
#include "content/public/common/content_features.h"
#include "content/renderer/loader/web_url_request_util.h"
#include "content/renderer/render_thread_impl.h"
#include "content/renderer/renderer_blink_platform_impl.h"
#include "content/renderer/service_worker/controller_service_worker_connector.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "net/base/net_errors.h"
#include "net/url_request/redirect_util.h"
#include "net/url_request/url_request.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/blink/public/common/service_worker/service_worker_type_converters.h"
#include "third_party/blink/public/mojom/blob/blob.mojom.h"
#include "third_party/blink/public/mojom/service_worker/dispatch_fetch_event_params.mojom.h"
#include "third_party/blink/public/platform/interface_provider.h"
#include "third_party/blink/public/platform/modules/service_worker/web_service_worker_request.h"
#include "third_party/blink/public/platform/web_http_body.h"
#include "third_party/blink/public/platform/web_string.h"
#include "ui/base/page_transition_types.h"

namespace content {

namespace {

network::ResourceResponseHead RewriteServiceWorkerTime(
    base::TimeTicks service_worker_start_time,
    base::TimeTicks service_worker_ready_time,
    const network::ResourceResponseHead& response_head) {
  network::ResourceResponseHead new_head = response_head;
  new_head.service_worker_start_time = service_worker_start_time;
  new_head.service_worker_ready_time = service_worker_ready_time;
  return new_head;
}

// A wrapper URLLoaderClient that invokes the given RewriteHeaderCallback
// whenever a response or redirect is received.
class HeaderRewritingURLLoaderClient : public network::mojom::URLLoaderClient {
 public:
  using RewriteHeaderCallback = base::Callback<network::ResourceResponseHead(
      const network::ResourceResponseHead&)>;

  HeaderRewritingURLLoaderClient(
      network::mojom::URLLoaderClientPtr url_loader_client,
      RewriteHeaderCallback rewrite_header_callback)
      : url_loader_client_(std::move(url_loader_client)),
        rewrite_header_callback_(rewrite_header_callback) {}
  ~HeaderRewritingURLLoaderClient() override {}

 private:
  // network::mojom::URLLoaderClient implementation:
  void OnReceiveResponse(
      const network::ResourceResponseHead& response_head) override {
    DCHECK(url_loader_client_.is_bound());
    url_loader_client_->OnReceiveResponse(
        rewrite_header_callback_.Run(response_head));
  }

  void OnReceiveRedirect(
      const net::RedirectInfo& redirect_info,
      const network::ResourceResponseHead& response_head) override {
    DCHECK(url_loader_client_.is_bound());
    url_loader_client_->OnReceiveRedirect(
        redirect_info, rewrite_header_callback_.Run(response_head));
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

  void OnComplete(const network::URLLoaderCompletionStatus& status) override {
    DCHECK(url_loader_client_.is_bound());
    url_loader_client_->OnComplete(status);
  }

  network::mojom::URLLoaderClientPtr url_loader_client_;
  RewriteHeaderCallback rewrite_header_callback_;
};
}  // namespace

// A ServiceWorkerStreamCallback implementation which waits for completion of
// a stream response for subresource loading. It calls
// ServiceWorkerSubresourceLoader::CommitCompleted() upon completion of the
// response.
class ServiceWorkerSubresourceLoader::StreamWaiter
    : public blink::mojom::ServiceWorkerStreamCallback {
 public:
  StreamWaiter(ServiceWorkerSubresourceLoader* owner,
               blink::mojom::ServiceWorkerStreamCallbackRequest request)
      : owner_(owner), binding_(this, std::move(request)) {
    DCHECK(owner_);
    binding_.set_connection_error_handler(
        base::BindOnce(&StreamWaiter::OnAborted, base::Unretained(this)));
  }

  // mojom::ServiceWorkerStreamCallback implementations:
  void OnCompleted() override { owner_->CommitCompleted(net::OK); }
  void OnAborted() override { owner_->CommitCompleted(net::ERR_ABORTED); }

 private:
  ServiceWorkerSubresourceLoader* owner_;
  mojo::Binding<blink::mojom::ServiceWorkerStreamCallback> binding_;

  DISALLOW_COPY_AND_ASSIGN(StreamWaiter);
};

// ServiceWorkerSubresourceLoader -------------------------------------------

ServiceWorkerSubresourceLoader::ServiceWorkerSubresourceLoader(
    network::mojom::URLLoaderRequest request,
    int32_t routing_id,
    int32_t request_id,
    uint32_t options,
    const network::ResourceRequest& resource_request,
    network::mojom::URLLoaderClientPtr client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
    scoped_refptr<ControllerServiceWorkerConnector> controller_connector,
    scoped_refptr<network::SharedURLLoaderFactory> fallback_factory,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : redirect_limit_(net::URLRequest::kMaxRedirects),
      url_loader_client_(std::move(client)),
      url_loader_binding_(this, std::move(request)),
      response_callback_binding_(this),
      controller_connector_(std::move(controller_connector)),
      controller_connector_observer_(this),
      fetch_request_restarted_(false),
      routing_id_(routing_id),
      request_id_(request_id),
      options_(options),
      traffic_annotation_(traffic_annotation),
      resource_request_(resource_request),
      fallback_factory_(std::move(fallback_factory)),
      task_runner_(std::move(task_runner)),
      weak_factory_(this) {
  DCHECK(controller_connector_);
  response_head_.request_start = base::TimeTicks::Now();
  response_head_.load_timing.request_start = base::TimeTicks::Now();
  response_head_.load_timing.request_start_time = base::Time::Now();
  // base::Unretained() is safe since |url_loader_binding_| is owned by |this|.
  url_loader_binding_.set_connection_error_handler(
      base::BindOnce(&ServiceWorkerSubresourceLoader::OnConnectionError,
                     base::Unretained(this)));
  StartRequest(resource_request);
}

ServiceWorkerSubresourceLoader::~ServiceWorkerSubresourceLoader() = default;

void ServiceWorkerSubresourceLoader::OnConnectionError() {
  delete this;
}

void ServiceWorkerSubresourceLoader::StartRequest(
    const network::ResourceRequest& resource_request) {
  TRACE_EVENT_WITH_FLOW1(
      "ServiceWorker", "ServiceWorkerSubresourceLoader::StartRequest", this,
      TRACE_EVENT_FLAG_FLOW_OUT, "url", resource_request.url.spec());
  DCHECK_EQ(Status::kNotStarted, status_);
  status_ = Status::kStarted;

  DCHECK(!ServiceWorkerUtils::IsMainResourceType(
      static_cast<ResourceType>(resource_request.resource_type)));

  DCHECK(!controller_connector_observer_.IsObservingSources());
  controller_connector_observer_.Add(controller_connector_.get());
  fetch_request_restarted_ = false;

  response_head_.service_worker_start_time = base::TimeTicks::Now();
  // TODO(horo): Reset |service_worker_ready_time| when the the connection to
  // the service worker is revived.
  response_head_.service_worker_ready_time = base::TimeTicks::Now();
  response_head_.load_timing.send_start = base::TimeTicks::Now();
  response_head_.load_timing.send_end = base::TimeTicks::Now();
  DispatchFetchEvent();
}

void ServiceWorkerSubresourceLoader::DispatchFetchEvent() {
  mojom::ServiceWorkerFetchResponseCallbackPtr response_callback_ptr;
  response_callback_binding_.Bind(mojo::MakeRequest(&response_callback_ptr));
  mojom::ControllerServiceWorker* controller =
      controller_connector_->GetControllerServiceWorker(
          mojom::ControllerServiceWorkerPurpose::FETCH_SUB_RESOURCE);
  TRACE_EVENT1("ServiceWorker",
               "ServiceWorkerSubresourceLoader::DispatchFetchEvent",
               "controller", (controller ? "exists" : "does not exist"));
  if (!controller) {
    auto controller_state = controller_connector_->state();
    if (controller_state ==
        ControllerServiceWorkerConnector::State::kNoController) {
      // The controller was lost after this loader or its loader factory was
      // created.
      fallback_factory_->CreateLoaderAndStart(
          url_loader_binding_.Unbind(), routing_id_, request_id_, options_,
          resource_request_, std::move(url_loader_client_),
          traffic_annotation_);
      delete this;
      return;
    }

    // When kNoContainerHost, the network request will be aborted soon since the
    // network provider has already been discarded. In that case, we don't need
    // to return an error as the client must be shutting down.
    DCHECK_EQ(ControllerServiceWorkerConnector::State::kNoContainerHost,
              controller_state);
    SettleFetchEventDispatch(base::nullopt);
    return;
  }

  auto params = blink::mojom::DispatchFetchEventParams::New();
  params->request = resource_request_;
  params->client_id = controller_connector_->client_id();

  // S13nServiceWorker without NetworkService:
  // BlobPtr for each blob data element in the request body needs to be created
  // before dispatching the fetch event for keeping the blob alive.
  if (resource_request_.request_body &&
      !base::FeatureList::IsEnabled(network::features::kNetworkService)) {
    // We need this as GetBlobFromUUID is a sync IPC.
    // TODO(kinuko): Remove the friend for ScopedAllowBaseSyncPrimitives
    // in //base as well when we remove this code.
    base::ScopedAllowBaseSyncPrimitives allow_sync_primitives;
    params->request_body_blob_ptrs =
        GetBlobPtrsForRequestBody(*resource_request_.request_body);
  }

  controller->DispatchFetchEvent(
      std::move(params), std::move(response_callback_ptr),
      base::BindOnce(&ServiceWorkerSubresourceLoader::OnFetchEventFinished,
                     weak_factory_.GetWeakPtr(), base::Time::Now()));
}

void ServiceWorkerSubresourceLoader::OnFetchEventFinished(
    base::Time request_dispatch_time,
    blink::mojom::ServiceWorkerEventStatus status,
    base::Time actual_dispatch_time) {
  TRACE_EVENT_WITH_FLOW1("ServiceWorker",
                         "ServiceWorkerSubresourceLoader::OnFetchEventFinished",
                         this, TRACE_EVENT_FLAG_FLOW_IN, "status",
                         ServiceWorkerUtils::MojoEnumToString(status));

  // Stop restarting logic here since OnFetchEventFinished() indicates that the
  // fetch event dispatch reached the renderer.
  SettleFetchEventDispatch(
      mojo::ConvertTo<blink::ServiceWorkerStatusCode>(status));

  base::TimeDelta delay = actual_dispatch_time - request_dispatch_time;
  UMA_HISTOGRAM_TIMES("ServiceWorker.EventDispatchingDelay", delay);
  UMA_HISTOGRAM_TIMES("ServiceWorker.EventDispatchingDelay_FETCH_SUB_RESOURCE",
                      delay);

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
      // Fetch event dispatch did not complete, possibly due to timeout of
      // respondWith() or waitUntil(). Return network error.

      // TODO(falken): This seems racy. respondWith() may have been called
      // already and we could have an outstanding stream or blob in progress,
      // and we might hit CommitCompleted() twice once that settles.
      CommitCompleted(net::ERR_FAILED);
  }
}

void ServiceWorkerSubresourceLoader::OnConnectionClosed() {
  response_callback_binding_.Close();

  // If the connection to the service worker gets disconnected after dispatching
  // a fetch event and before getting the response of the fetch event, restart
  // the fetch event again. If it has already been restarted, that means
  // starting worker failed. In that case, abort the request.
  if (fetch_request_restarted_) {
    SettleFetchEventDispatch(
        blink::ServiceWorkerStatusCode::kErrorStartWorkerFailed);
    CommitCompleted(net::ERR_FAILED);
    return;
  }
  fetch_request_restarted_ = true;
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&ServiceWorkerSubresourceLoader::DispatchFetchEvent,
                     weak_factory_.GetWeakPtr()));
}

void ServiceWorkerSubresourceLoader::SettleFetchEventDispatch(
    base::Optional<blink::ServiceWorkerStatusCode> status) {
  if (!controller_connector_observer_.IsObservingSources()) {
    // Already settled.
    return;
  }
  controller_connector_observer_.RemoveAll();

  if (status) {
    blink::ServiceWorkerStatusCode value = status.value();
    UMA_HISTOGRAM_ENUMERATION("ServiceWorker.FetchEvent.Subresource.Status",
                              value);
  }
}

void ServiceWorkerSubresourceLoader::OnResponse(
    const ServiceWorkerResponse& response,
    base::Time dispatch_event_time) {
  TRACE_EVENT_WITH_FLOW0("ServiceWorker",
                         "ServiceWorkerSubresourceLoader::OnResponse", this,
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);
  SettleFetchEventDispatch(blink::ServiceWorkerStatusCode::kOk);
  StartResponse(response, nullptr /* body_as_blob */,
                nullptr /* body_as_stream */);
}

void ServiceWorkerSubresourceLoader::OnResponseBlob(
    const ServiceWorkerResponse& response,
    blink::mojom::BlobPtr body_as_blob,
    base::Time dispatch_event_time) {
  TRACE_EVENT_WITH_FLOW0("ServiceWorker",
                         "ServiceWorkerSubresourceLoader::OnResponseBlob", this,
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);
  SettleFetchEventDispatch(blink::ServiceWorkerStatusCode::kOk);
  StartResponse(response, std::move(body_as_blob),
                nullptr /* body_as_stream */);
}

void ServiceWorkerSubresourceLoader::OnResponseStream(
    const ServiceWorkerResponse& response,
    blink::mojom::ServiceWorkerStreamHandlePtr body_as_stream,
    base::Time dispatch_event_time) {
  TRACE_EVENT_WITH_FLOW0(
      "ServiceWorker", "ServiceWorkerSubresourceLoader::OnResponseStream", this,
      TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);
  SettleFetchEventDispatch(blink::ServiceWorkerStatusCode::kOk);
  StartResponse(response, nullptr /* body_as_blob */,
                std::move(body_as_stream));
}

void ServiceWorkerSubresourceLoader::OnFallback(
    base::Time dispatch_event_time) {
  SettleFetchEventDispatch(blink::ServiceWorkerStatusCode::kOk);
  // When the request mode is CORS or CORS-with-forced-preflight and the origin
  // of the request URL is different from the security origin of the document,
  // we can't simply fallback to the network here. It is because the CORS
  // preflight logic is implemented in Blink. So we return a "fallback required"
  // response to Blink.
  if ((resource_request_.fetch_request_mode ==
           network::mojom::FetchRequestMode::kCORS ||
       resource_request_.fetch_request_mode ==
           network::mojom::FetchRequestMode::kCORSWithForcedPreflight) &&
      (!resource_request_.request_initiator.has_value() ||
       !resource_request_.request_initiator->IsSameOriginWith(
           url::Origin::Create(resource_request_.url)))) {
    TRACE_EVENT_WITH_FLOW0(
        "ServiceWorker", "ServiceWorkerSubresourceLoader::OnFallback", this,
        TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);
    response_head_.was_fetched_via_service_worker = true;
    response_head_.was_fallback_required_by_service_worker = true;
    CommitResponseHeaders();
    CommitCompleted(net::OK);
    return;
  }
  TRACE_EVENT_WITH_FLOW0("ServiceWorker",
                         "ServiceWorkerSubresourceLoader::OnFallback", this,
                         TRACE_EVENT_FLAG_FLOW_IN);

  // Hand over to the network loader.
  network::mojom::URLLoaderClientPtr client;
  auto client_impl = std::make_unique<HeaderRewritingURLLoaderClient>(
      std::move(url_loader_client_),
      base::BindRepeating(&RewriteServiceWorkerTime,
                          response_head_.service_worker_start_time,
                          response_head_.service_worker_ready_time));
  mojo::MakeStrongBinding(std::move(client_impl), mojo::MakeRequest(&client));

  fallback_factory_->CreateLoaderAndStart(
      url_loader_binding_.Unbind(), routing_id_, request_id_, options_,
      resource_request_, std::move(client), traffic_annotation_);
  // Per spec, redirects after this point are not intercepted by the service
  // worker again (https://crbug.com/517364). So this loader is done.
  //
  // Assume ServiceWorkerSubresourceLoaderFactory is still alive and also
  // has a ref to fallback_factory_, so it's OK to destruct here. If that
  // factory dies, the web context that made the request is dead so the request
  // is moot.
  DCHECK(!fallback_factory_->HasOneRef());
  delete this;
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
    // Set status to complete, but we expect to restart in FollowRedirect.
    status_ = Status::kCompleted;
    return;
  }

  // We have a non-redirect response. Send the headers to the client.
  CommitResponseHeaders();

  // Handle a stream response body.
  if (!body_as_stream.is_null() && body_as_stream->stream.is_valid()) {
    DCHECK(!body_as_blob);
    DCHECK(url_loader_client_.is_bound());
    stream_waiter_ = std::make_unique<StreamWaiter>(
        this, std::move(body_as_stream->callback_request));
    url_loader_client_->OnStartLoadingResponseBody(
        std::move(body_as_stream->stream));
    return;
  }

  // Handle a blob response body.
  if (body_as_blob) {
    DCHECK(!body_as_stream);
    body_as_blob_ = std::move(body_as_blob);
    body_as_blob_->ReadSideData(base::BindOnce(
        &ServiceWorkerSubresourceLoader::OnBlobSideDataReadingComplete,
        base::Unretained(this)));
    return;
  }

  // The response has no body.
  CommitCompleted(net::OK);
}

void ServiceWorkerSubresourceLoader::CommitResponseHeaders() {
  DCHECK_EQ(Status::kStarted, status_);
  DCHECK(url_loader_client_.is_bound());
  status_ = Status::kSentHeader;
  // TODO(kinuko): Fill the ssl_info.
  url_loader_client_->OnReceiveResponse(response_head_);
}

void ServiceWorkerSubresourceLoader::CommitCompleted(int error_code) {
  TRACE_EVENT_WITH_FLOW1(
      "ServiceWorker", "ServiceWorkerSubresourceLoader::CommitCompleted", this,
      TRACE_EVENT_FLAG_FLOW_IN, "error_code", net::ErrorToString(error_code));

  DCHECK_LT(status_, Status::kCompleted);
  DCHECK(url_loader_client_.is_bound());
  stream_waiter_.reset();
  status_ = Status::kCompleted;
  network::URLLoaderCompletionStatus status;
  status.error_code = error_code;
  status.completion_time = base::TimeTicks::Now();
  url_loader_client_->OnComplete(status);
}

// ServiceWorkerSubresourceLoader: URLLoader implementation -----------------

void ServiceWorkerSubresourceLoader::FollowRedirect(
    const base::Optional<std::vector<std::string>>&
        to_be_removed_request_headers,
    const base::Optional<net::HttpRequestHeaders>& modified_request_headers) {
  TRACE_EVENT_WITH_FLOW1("ServiceWorker",
                         "ServiceWorkerSubresourceLoader::FollowRedirect", this,
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT,
                         "new_url", redirect_info_->new_url.spec());
  DCHECK(!modified_request_headers.has_value()) << "Redirect with modified "
                                                   "headers was not supported "
                                                   "yet. crbug.com/845683";
  DCHECK(redirect_info_);

  bool should_clear_upload = false;
  net::RedirectUtil::UpdateHttpRequest(
      resource_request_.url, resource_request_.method, *redirect_info_,
      modified_request_headers, &resource_request_.headers,
      &should_clear_upload);
  if (should_clear_upload)
    resource_request_.request_body = nullptr;

  resource_request_.url = redirect_info_->new_url;
  resource_request_.method = redirect_info_->new_method;
  resource_request_.site_for_cookies = redirect_info_->new_site_for_cookies;
  resource_request_.referrer = GURL(redirect_info_->new_referrer);
  resource_request_.referrer_policy = redirect_info_->new_referrer_policy;

  // Restart the request.
  status_ = Status::kNotStarted;
  redirect_info_.reset();
  response_callback_binding_.Close();
  StartRequest(resource_request_);
}

void ServiceWorkerSubresourceLoader::ProceedWithResponse() {
  NOTREACHED();
}

void ServiceWorkerSubresourceLoader::SetPriority(net::RequestPriority priority,
                                                 int intra_priority_value) {
  // Not supported (do nothing).
}

void ServiceWorkerSubresourceLoader::PauseReadingBodyFromNet() {}

void ServiceWorkerSubresourceLoader::ResumeReadingBodyFromNet() {}

void ServiceWorkerSubresourceLoader::OnBlobSideDataReadingComplete(
    const base::Optional<std::vector<uint8_t>>& metadata) {
  TRACE_EVENT_WITH_FLOW1(
      "ServiceWorker",
      "ServiceWorkerSubresourceLoader::OnBlobSideDataReadingComplete", this,
      TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT, "metadata size",
      (metadata ? metadata->size() : 0));
  DCHECK(url_loader_client_);
  if (metadata.has_value())
    url_loader_client_->OnReceiveCachedMetadata(metadata.value());
  mojo::ScopedDataPipeConsumerHandle data_pipe;
  int error = ServiceWorkerLoaderHelpers::ReadBlobResponseBody(
      &body_as_blob_, resource_request_.headers,
      base::BindOnce(&ServiceWorkerSubresourceLoader::OnBlobReadingComplete,
                     weak_factory_.GetWeakPtr()),
      &data_pipe);
  if (error != net::OK) {
    CommitCompleted(error);
    return;
  }
  url_loader_client_->OnStartLoadingResponseBody(std::move(data_pipe));
  // We continue in OnBlobReadingComplete().
}

void ServiceWorkerSubresourceLoader::OnBlobReadingComplete(int net_error) {
  TRACE_EVENT_WITH_FLOW0(
      "ServiceWorker", "ServiceWorkerSubresourceLoader::OnBlobReadingComplete",
      this, TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);
  CommitCompleted(net_error);
  body_as_blob_.reset();
}

// ServiceWorkerSubresourceLoaderFactory ------------------------------------

// static
void ServiceWorkerSubresourceLoaderFactory::Create(
    scoped_refptr<ControllerServiceWorkerConnector> controller_connector,
    scoped_refptr<network::SharedURLLoaderFactory> fallback_factory,
    network::mojom::URLLoaderFactoryRequest request,
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  new ServiceWorkerSubresourceLoaderFactory(
      std::move(controller_connector), std::move(fallback_factory),
      std::move(request), std::move(task_runner));
}

ServiceWorkerSubresourceLoaderFactory::ServiceWorkerSubresourceLoaderFactory(
    scoped_refptr<ControllerServiceWorkerConnector> controller_connector,
    scoped_refptr<network::SharedURLLoaderFactory> fallback_factory,
    network::mojom::URLLoaderFactoryRequest request,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : controller_connector_(std::move(controller_connector)),
      fallback_factory_(std::move(fallback_factory)),
      task_runner_(std::move(task_runner)) {
  DCHECK(fallback_factory_);
  bindings_.AddBinding(this, std::move(request));
  bindings_.set_connection_error_handler(base::BindRepeating(
      &ServiceWorkerSubresourceLoaderFactory::OnConnectionError,
      base::Unretained(this)));
}

ServiceWorkerSubresourceLoaderFactory::
    ~ServiceWorkerSubresourceLoaderFactory() = default;

void ServiceWorkerSubresourceLoaderFactory::CreateLoaderAndStart(
    network::mojom::URLLoaderRequest request,
    int32_t routing_id,
    int32_t request_id,
    uint32_t options,
    const network::ResourceRequest& resource_request,
    network::mojom::URLLoaderClientPtr client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  // This loader destructs itself, as we want to transparently switch to the
  // network loader when fallback happens. When that happens the loader unbinds
  // the request, passes the request to the fallback factory, and
  // destructs itself (while the loader client continues to work).
  new ServiceWorkerSubresourceLoader(
      std::move(request), routing_id, request_id, options, resource_request,
      std::move(client), traffic_annotation, controller_connector_,
      fallback_factory_, task_runner_);
}

void ServiceWorkerSubresourceLoaderFactory::Clone(
    network::mojom::URLLoaderFactoryRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

void ServiceWorkerSubresourceLoaderFactory::OnConnectionError() {
  if (!bindings_.empty())
    return;
  delete this;
}

}  // namespace content
