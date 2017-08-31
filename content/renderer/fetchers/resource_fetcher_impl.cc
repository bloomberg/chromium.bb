// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/fetchers/resource_fetcher_impl.h"

#include <stdint.h>

#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "content/child/resource_dispatcher.h"
#include "content/child/web_url_request_util.h"
#include "content/common/possibly_associated_interface_ptr.h"
#include "content/public/child/child_url_loader_factory_getter.h"
#include "content/public/common/resource_request_body.h"
#include "content/public/renderer/render_frame.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "net/base/net_errors.h"
#include "net/http/http_request_headers.h"
#include "net/url_request/url_request_context.h"
#include "third_party/WebKit/public/platform/WebSecurityOrigin.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/platform/WebURLResponse.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"

namespace {

constexpr int32_t kRoutingId = 0;
const char kAccessControlAllowOriginHeader[] = "Access-Control-Allow-Origin";

}  // namespace

namespace content {

// static
ResourceFetcher* ResourceFetcher::Create(const GURL& url) {
  return new ResourceFetcherImpl(url);
}

// TODO(toyoshim): Internal implementation might be replaced with
// SimpleURLLoader, and content::ResourceFetcher could be a thin-wrapper
// class to use SimpleURLLoader with blink-friendly types.
class ResourceFetcherImpl::ClientImpl : public mojom::URLLoaderClient {
 public:
  ClientImpl(ResourceFetcherImpl* parent,
             const Callback& callback,
             size_t maximum_download_size)
      : parent_(parent),
        client_binding_(this),
        data_pipe_watcher_(FROM_HERE,
                           mojo::SimpleWatcher::ArmingPolicy::MANUAL),
        status_(Status::kNotStarted),
        completed_(false),
        maximum_download_size_(maximum_download_size),
        callback_(callback) {}

  ~ClientImpl() override {}

  void Start(const ResourceRequest& request,
             mojom::URLLoaderFactory* url_loader_factory,
             const net::NetworkTrafficAnnotationTag& annotation_tag,
             base::SingleThreadTaskRunner* task_runner) {
    status_ = Status::kStarted;
    response_.SetURL(request.url);

    mojom::URLLoaderClientPtr client;
    client_binding_.Bind(mojo::MakeRequest(&client), task_runner);

    url_loader_factory->CreateLoaderAndStart(
        mojo::MakeRequest(&loader_), kRoutingId,
        ResourceDispatcher::MakeRequestID(), mojom::kURLLoadOptionNone, request,
        std::move(client),
        net::MutableNetworkTrafficAnnotationTag(annotation_tag));
  }

  void Cancel() {
    ClearReceivedDataToFail();

    // Close will invoke OnComplete() eventually.
    Close();

    // Reset |loader_| to avoid unexpected other callbacks invocations.
    loader_.reset();
  }

  bool IsActive() const {
    return status_ == Status::kStarted || status_ == Status::kFetching ||
           status_ == Status::kClosed;
  }

 private:
  enum class Status {
    kNotStarted,  // Initial state.
    kStarted,     // Start() is called, but data pipe is not ready yet.
    kFetching,    // Fetching via data pipe.
    kClosed,      // Data pipe is already closed, but may not be completed yet.
    kCompleted,   // Final state.
  };

  void MayComplete() {
    DCHECK(IsActive()) << "status: " << static_cast<int>(status_);
    DCHECK_NE(Status::kCompleted, status_);

    if (status_ == Status::kFetching || !completed_)
      return;

    status_ = Status::kCompleted;
    loader_.reset();

    parent_->OnLoadComplete();

    if (callback_.is_null())
      return;

    // Take a reference to the callback as running the callback may lead to our
    // destruction.
    Callback callback = callback_;
    callback.Run(response_, data_);
  }

  void ClearReceivedDataToFail() {
    response_ = blink::WebURLResponse();
    data_.clear();
  }

  void ReadDataPipe() {
    DCHECK_EQ(Status::kFetching, status_);

    for (;;) {
      const void* data;
      uint32_t size;
      MojoResult result =
          data_pipe_->BeginReadData(&data, &size, MOJO_READ_DATA_FLAG_NONE);
      if (result == MOJO_RESULT_SHOULD_WAIT) {
        data_pipe_watcher_.ArmOrNotify();
        return;
      }

      if (result == MOJO_RESULT_FAILED_PRECONDITION) {
        // Complete to read the data pipe successfully.
        Close();
        return;
      }
      DCHECK_EQ(MOJO_RESULT_OK, result);  // Only program errors can fire.

      if (data_.size() + size > maximum_download_size_) {
        data_pipe_->EndReadData(size);
        Cancel();
        return;
      }

      data_.append(static_cast<const char*>(data), size);

      result = data_pipe_->EndReadData(size);
      DCHECK_EQ(MOJO_RESULT_OK, result);  // Only program errors can fire.
    }
  }

  void Close() {
    if (status_ == Status::kFetching) {
      data_pipe_watcher_.Cancel();
      data_pipe_.reset();
    }
    status_ = Status::kClosed;
    MayComplete();
  }

  void OnDataPipeSignaled(MojoResult result,
                          const mojo::HandleSignalsState& state) {
    ReadDataPipe();
  }

  // mojom::URLLoaderClient overrides:
  void OnReceiveResponse(
      const ResourceResponseHead& response_head,
      const base::Optional<net::SSLInfo>& ssl_info,
      mojom::DownloadedTempFilePtr downloaded_file) override {
    DCHECK_EQ(Status::kStarted, status_);
    // Existing callers need URL and HTTP status code. URL is already set in
    // Start().
    if (response_head.headers)
      response_.SetHTTPStatusCode(response_head.headers->response_code());
  }
  void OnReceiveRedirect(const net::RedirectInfo& redirect_info,
                         const ResourceResponseHead& response_head) override {
    DCHECK_EQ(Status::kStarted, status_);
    loader_->FollowRedirect();
    response_.SetURL(redirect_info.new_url);
  }
  void OnDataDownloaded(int64_t data_len, int64_t encoded_data_len) override {}
  void OnUploadProgress(int64_t current_position,
                        int64_t total_size,
                        OnUploadProgressCallback ack_callback) override {}
  void OnReceiveCachedMetadata(const std::vector<uint8_t>& data) override {}
  void OnTransferSizeUpdated(int32_t transfer_size_diff) override {}
  void OnStartLoadingResponseBody(
      mojo::ScopedDataPipeConsumerHandle body) override {
    DCHECK_EQ(Status::kStarted, status_);
    status_ = Status::kFetching;

    data_pipe_ = std::move(body);
    data_pipe_watcher_.Watch(
        data_pipe_.get(),
        MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
        MOJO_WATCH_CONDITION_SATISFIED,
        base::BindRepeating(
            &ResourceFetcherImpl::ClientImpl::OnDataPipeSignaled,
            base::Unretained(this)));
    ReadDataPipe();
  }
  void OnComplete(const ResourceRequestCompletionStatus& status) override {
    DCHECK(IsActive()) << "status: " << static_cast<int>(status_);
    if (status.error_code != net::OK) {
      ClearReceivedDataToFail();
      Close();
    }
    completed_ = true;
    MayComplete();
  }

 private:
  ResourceFetcherImpl* parent_;
  mojom::URLLoaderPtr loader_;
  mojo::Binding<mojom::URLLoaderClient> client_binding_;
  mojo::ScopedDataPipeConsumerHandle data_pipe_;
  mojo::SimpleWatcher data_pipe_watcher_;
  PossiblyAssociatedInterfacePtr<mojom::URLLoaderFactory> loader_factory_;

  Status status_;

  // A flag to represent if OnComplete() is already called. |data_pipe_| can be
  // ready even after OnComplete() is called.
  bool completed_;

  // Maximum download size to be stored in |data_|.
  const size_t maximum_download_size_;

  // Received data to be passed to the |callback_|.
  std::string data_;

  // Response to be passed to the |callback_|.
  blink::WebURLResponse response_;

  // Callback when we're done.
  Callback callback_;

  DISALLOW_COPY_AND_ASSIGN(ClientImpl);
};

ResourceFetcherImpl::ResourceFetcherImpl(const GURL& url) {
  DCHECK(url.is_valid());
  request_.url = url;
}

ResourceFetcherImpl::~ResourceFetcherImpl() {
  if (client_->IsActive())
    client_->Cancel();
}

void ResourceFetcherImpl::SetMethod(const std::string& method) {
  DCHECK(!client_);
  request_.method = method;
}

void ResourceFetcherImpl::SetBody(const std::string& body) {
  DCHECK(!client_);
  request_.request_body =
      ResourceRequestBody::CreateFromBytes(body.data(), body.size());
}

void ResourceFetcherImpl::SetHeader(const std::string& header,
                                    const std::string& value) {
  DCHECK(!client_);
  if (base::LowerCaseEqualsASCII(header, net::HttpRequestHeaders::kReferer)) {
    request_.referrer = GURL(value);
    DCHECK(request_.referrer.is_valid());
    request_.referrer_policy = blink::kWebReferrerPolicyDefault;
  } else {
    headers_.SetHeader(header, value);
  }
}

void ResourceFetcherImpl::Start(
    blink::WebLocalFrame* frame,
    blink::WebURLRequest::RequestContext request_context,
    const Callback& callback) {
  static const net::NetworkTrafficAnnotationTag annotation_tag =
      net::DefineNetworkTrafficAnnotation("content_resource_fetcher", R"(
    semantics {
      sender: "content ResourceFetcher"
      description:
        "Chrome content API initiated request, which includes network error "
        "pages and mojo internal component downloader."
      trigger:
        "Showing network error pages, or needs to download mojo component."
      data: "Anything the initiator wants."
      destination: OTHER
    }
    policy {
      cookies_allowed: YES
      cookies_store: "user"
      setting: "These requests cannot be disabled in settings."
      policy_exception_justification:
        "Not implemented. Without these requests, Chrome will not work."
    })");

  mojom::URLLoaderFactory* url_loader_factory =
      RenderFrame::FromWebFrame(frame)
          ->GetDefaultURLLoaderFactoryGetter()
          ->GetNetworkLoaderFactory();
  DCHECK(url_loader_factory);

  Start(frame, request_context, url_loader_factory, annotation_tag, callback,
        kDefaultMaximumDownloadSize);
}

void ResourceFetcherImpl::Start(
    blink::WebLocalFrame* frame,
    blink::WebURLRequest::RequestContext request_context,
    mojom::URLLoaderFactory* url_loader_factory,
    const net::NetworkTrafficAnnotationTag& annotation_tag,
    const Callback& callback,
    size_t maximum_download_size) {
  DCHECK(!client_);
  DCHECK(frame);
  DCHECK(url_loader_factory);
  DCHECK(!frame->GetDocument().IsNull());
  if (request_.method.empty())
    request_.method = net::HttpRequestHeaders::kGetMethod;
  if (request_.request_body) {
    DCHECK(!base::LowerCaseEqualsASCII(request_.method,
                                       net::HttpRequestHeaders::kGetMethod))
        << "GETs can't have bodies.";
  }

  request_.request_context = request_context;
  request_.site_for_cookies = frame->GetDocument().SiteForCookies();
  if (!frame->GetDocument().GetSecurityOrigin().IsNull()) {
    request_.request_initiator =
        static_cast<url::Origin>(frame->GetDocument().GetSecurityOrigin());
    SetHeader(kAccessControlAllowOriginHeader,
              blink::WebSecurityOrigin::CreateUnique().ToString().Ascii());
  }
  if (!headers_.IsEmpty())
    request_.headers = headers_.ToString();

  request_.resource_type = WebURLRequestContextToResourceType(request_context);

  client_ = base::MakeUnique<ClientImpl>(this, callback, maximum_download_size);
  client_->Start(request_, url_loader_factory, annotation_tag,
                 frame->LoadingTaskRunner());

  // No need to hold on to the request; reset it now.
  request_ = ResourceRequest();
}

void ResourceFetcherImpl::SetTimeout(const base::TimeDelta& timeout) {
  DCHECK(client_);
  DCHECK(client_->IsActive());

  timeout_timer_.Start(FROM_HERE, timeout, this, &ResourceFetcherImpl::Cancel);
}

void ResourceFetcherImpl::OnLoadComplete() {
  timeout_timer_.Stop();
}

void ResourceFetcherImpl::Cancel() {
  DCHECK(client_);
  DCHECK(client_->IsActive());
  client_->Cancel();
}

}  // namespace content
