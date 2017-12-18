// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/url_loader_interceptor.h"

#include "base/test/bind_test_util.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/loader/resource_message_filter.h"
#include "content/browser/loader/url_loader_factory_impl.h"
#include "content/browser/url_loader_factory_getter.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_features.h"
#include "content/public/common/url_loader.mojom.h"
#include "net/http/http_util.h"

namespace content {

class URLLoaderInterceptor::Interceptor : public mojom::URLLoaderFactory {
 public:
  using ProcessIdGetter = base::Callback<int()>;
  using OriginalFactoryGetter = base::Callback<mojom::URLLoaderFactory*()>;

  Interceptor(URLLoaderInterceptor* parent,
              const ProcessIdGetter& process_id_getter,
              const OriginalFactoryGetter& original_factory_getter)
      : parent_(parent),
        process_id_getter_(process_id_getter),
        original_factory_getter_(original_factory_getter) {}

  ~Interceptor() override {}

 private:
  // mojom::URLLoaderFactory implementation:
  void CreateLoaderAndStart(mojom::URLLoaderRequest request,
                            int32_t routing_id,
                            int32_t request_id,
                            uint32_t options,
                            const ResourceRequest& url_request,
                            mojom::URLLoaderClientPtr client,
                            const net::MutableNetworkTrafficAnnotationTag&
                                traffic_annotation) override {
    RequestParams params;
    params.process_id = process_id_getter_.Run();
    params.request = std::move(request);
    params.routing_id = routing_id;
    params.request_id = request_id;
    params.options = options;
    params.url_request = std::move(url_request);
    params.client = std::move(client);
    params.traffic_annotation = traffic_annotation;
    // We don't intercept the blob URL for plznavigate requests.
    if (url_request.resource_body_stream_url.is_empty() &&
        parent_->callback_.Run(&params))
      return;

    original_factory_getter_.Run()->CreateLoaderAndStart(
        std::move(params.request), params.routing_id, params.request_id,
        params.options, std::move(params.url_request), std::move(params.client),
        params.traffic_annotation);
  }

  void Clone(mojom::URLLoaderFactoryRequest request) override { NOTREACHED(); }

  URLLoaderInterceptor* parent_;
  ProcessIdGetter process_id_getter_;
  OriginalFactoryGetter original_factory_getter_;

  DISALLOW_COPY_AND_ASSIGN(Interceptor);
};

// This class intercepts calls to each StoragePartition's URLLoaderFactoryGetter
// so that it can intercept frame requests.
class URLLoaderInterceptor::URLLoaderFactoryGetterWrapper {
 public:
  URLLoaderFactoryGetterWrapper(
      URLLoaderFactoryGetter* url_loader_factory_getter,
      URLLoaderInterceptor* parent)
      : url_loader_factory_getter_(url_loader_factory_getter) {
    frame_interceptor_ = std::make_unique<Interceptor>(
        parent, base::BindRepeating([]() { return 0; }),
        base::BindLambdaForTesting([=]() -> mojom::URLLoaderFactory* {
          return url_loader_factory_getter
              ->original_network_factory_for_testing()
              ->get();
        }));
    url_loader_factory_getter_->SetNetworkFactoryForTesting(
        frame_interceptor_.get());
  }

  ~URLLoaderFactoryGetterWrapper() {
    url_loader_factory_getter_->SetNetworkFactoryForTesting(nullptr);
  }

 private:
  std::unique_ptr<Interceptor> frame_interceptor_;
  URLLoaderFactoryGetter* url_loader_factory_getter_;
};

// This class is sent along a RenderFrame commit message as a subresource
// loader so that it can intercept subresource requests.
class URLLoaderInterceptor::SubresourceWrapper {
 public:
  SubresourceWrapper(mojom::URLLoaderFactoryRequest factory_request,
                     int process_id,
                     URLLoaderInterceptor* parent,
                     mojom::URLLoaderFactoryPtrInfo original_factory)
      : interceptor_(
            parent,
            base::BindRepeating([](int process_id) { return process_id; },
                                process_id),
            base::BindRepeating(&SubresourceWrapper::GetOriginalFactory,
                                base::Unretained(this))),
        binding_(&interceptor_, std::move(factory_request)),
        original_factory_(std::move(original_factory)) {
    binding_.set_connection_error_handler(
        base::BindOnce(&URLLoaderInterceptor::SubresourceWrapperBindingError,
                       base::Unretained(parent), this));
  }

  ~SubresourceWrapper() {}

 private:
  mojom::URLLoaderFactory* GetOriginalFactory() {
    return original_factory_.get();
  }

  Interceptor interceptor_;
  mojo::Binding<mojom::URLLoaderFactory> binding_;
  mojom::URLLoaderFactoryPtr original_factory_;

  DISALLOW_COPY_AND_ASSIGN(SubresourceWrapper);
};

URLLoaderInterceptor::RequestParams::RequestParams() = default;
URLLoaderInterceptor::RequestParams::~RequestParams() = default;

URLLoaderInterceptor::URLLoaderInterceptor(const InterceptCallback& callback,
                                           bool intercept_frame_requests,
                                           bool intercept_subresources)
    : callback_(callback),
      intercept_frame_requests_(intercept_frame_requests),
      intercept_subresources_(intercept_subresources) {
  DCHECK(!BrowserThread::IsThreadInitialized(BrowserThread::UI) ||
         BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (intercept_subresources &&
      base::FeatureList::IsEnabled(features::kNetworkService)) {
    RenderFrameHostImpl::SetNetworkFactoryForTesting(base::BindRepeating(
        &URLLoaderInterceptor::CreateURLLoaderFactoryForSubresources,
        base::Unretained(this)));
  }

  if (BrowserThread::IsThreadInitialized(BrowserThread::IO)) {
    base::RunLoop run_loop;
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::BindOnce(&URLLoaderInterceptor::InitializeOnIOThread,
                       base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
  } else {
    InitializeOnIOThread(base::OnceClosure());
  }
}

URLLoaderInterceptor::~URLLoaderInterceptor() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (intercept_subresources_ &&
      base::FeatureList::IsEnabled(features::kNetworkService)) {
    RenderFrameHostImpl::SetNetworkFactoryForTesting(
        RenderFrameHostImpl::CreateNetworkFactoryCallback());
  }

  base::RunLoop run_loop;
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::BindOnce(&URLLoaderInterceptor::ShutdownOnIOThread,
                     base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();
}

void URLLoaderInterceptor::WriteResponse(const std::string& headers,
                                         const std::string& body,
                                         mojom::URLLoaderClient* client) {
  net::HttpResponseInfo info;
  info.headers = new net::HttpResponseHeaders(
      net::HttpUtil::AssembleRawHeaders(headers.c_str(), headers.length()));
  content::ResourceResponseHead response;
  response.headers = info.headers;
  response.headers->GetMimeType(&response.mime_type);
  client->OnReceiveResponse(response, base::nullopt, nullptr);

  uint32_t bytes_written = body.size();
  mojo::DataPipe data_pipe;
  data_pipe.producer_handle->WriteData(body.data(), &bytes_written,
                                       MOJO_WRITE_DATA_FLAG_ALL_OR_NONE);
  client->OnStartLoadingResponseBody(std::move(data_pipe.consumer_handle));

  network::URLLoaderCompletionStatus status;
  status.error_code = net::OK;
  client->OnComplete(status);
}

void URLLoaderInterceptor::CreateURLLoaderFactoryForSubresources(
    mojom::URLLoaderFactoryRequest request,
    int process_id,
    mojom::URLLoaderFactoryPtrInfo original_factory) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&URLLoaderInterceptor::CreateURLLoaderFactoryForSubresources,
                   base::Unretained(this), base::Passed(&request), process_id,
                   base::Passed(&original_factory)));
    return;
  }

  subresource_wrappers_.emplace(std::make_unique<SubresourceWrapper>(
      std::move(request), process_id, this, std::move(original_factory)));
}

void URLLoaderInterceptor::GetNetworkFactoryCallback(
    URLLoaderFactoryGetter* url_loader_factory_getter) {
  url_loader_factory_getter_wrappers_.emplace(
      std::make_unique<URLLoaderFactoryGetterWrapper>(url_loader_factory_getter,
                                                      this));
}

void URLLoaderInterceptor::SubresourceWrapperBindingError(
    SubresourceWrapper* wrapper) {
  for (auto& it : subresource_wrappers_) {
    if (it.get() == wrapper) {
      subresource_wrappers_.erase(it);
      return;
    }
  }

  NOTREACHED();
}

void URLLoaderInterceptor::InitializeOnIOThread(base::OnceClosure closure) {
  // Once http://crbug.com/747130 is fixed, the codepath above will work for
  // the non-network service code path.
  if (intercept_frame_requests_ &&
      base::FeatureList::IsEnabled(features::kNetworkService)) {
    URLLoaderFactoryGetter::SetGetNetworkFactoryCallbackForTesting(
        base::BindRepeating(&URLLoaderInterceptor::GetNetworkFactoryCallback,
                            base::Unretained(this)));
  }

  if (intercept_subresources_ &&
      !base::FeatureList::IsEnabled(features::kNetworkService)) {
    rmf_interceptor_ = std::make_unique<Interceptor>(
        this, base::BindRepeating([]() {
          return ResourceMessageFilter::GetCurrentForTesting()->child_id();
        }),
        base::BindRepeating([]() {
          mojom::URLLoaderFactory* factory =
              ResourceMessageFilter::GetCurrentForTesting();
          return factory;
        }));
    ResourceMessageFilter::SetNetworkFactoryForTesting(rmf_interceptor_.get());
  }

  if (closure)
    std::move(closure).Run();
}

void URLLoaderInterceptor::ShutdownOnIOThread(base::OnceClosure closure) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  url_loader_factory_getter_wrappers_.clear();
  subresource_wrappers_.clear();

  if (intercept_frame_requests_ &&
      base::FeatureList::IsEnabled(features::kNetworkService)) {
    URLLoaderFactoryGetter::SetGetNetworkFactoryCallbackForTesting(
        URLLoaderFactoryGetter::GetNetworkFactoryCallback());
  }

  if (intercept_subresources_ &&
      !base::FeatureList::IsEnabled(features::kNetworkService)) {
    ResourceMessageFilter::SetNetworkFactoryForTesting(nullptr);
  }

  std::move(closure).Run();
}

}  // namespace content
