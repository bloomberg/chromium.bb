// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_url_job_wrapper.h"

#include "base/command_line.h"
#include "content/browser/service_worker/service_worker_response_type.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/common/service_worker/service_worker_utils.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/common/browser_side_navigation_policy.h"
#include "content/public/common/content_switches.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "net/base/io_buffer.h"
#include "storage/browser/blob/blob_storage_context.h"

namespace content {

namespace {

class URLLoaderImpl : public mojom::URLLoader {
 public:
  static void Start(
      const ServiceWorkerResponse& response,
      blink::mojom::ServiceWorkerStreamHandlePtr body_as_stream,
      base::WeakPtr<storage::BlobStorageContext> blob_storage_context,
      mojom::URLLoaderRequest request,
      mojom::URLLoaderClientPtr client) {
    mojo::MakeStrongBinding(base::MakeUnique<URLLoaderImpl>(
                                response, std::move(body_as_stream),
                                blob_storage_context, std::move(client)),
                            std::move(request));
  }

  URLLoaderImpl(const ServiceWorkerResponse& response,
                blink::mojom::ServiceWorkerStreamHandlePtr body_as_stream,
                base::WeakPtr<storage::BlobStorageContext> blob_storage_context,
                mojom::URLLoaderClientPtr url_loader_client)
      : blob_storage_context_(blob_storage_context),
        url_loader_client_(std::move(url_loader_client)),
        weak_factory_(this) {
    ResourceResponseHead head;
    // TODO(scottmg): More fields in |head| required?
    head.headers = new net::HttpResponseHeaders("");
    for (const auto& kv : response.headers)
      head.headers->AddHeader(kv.first + ": " + kv.second);
    head.url_list_via_service_worker = response.url_list;
    head.mime_type = "text/html";  // TODO(scottmg): No idea where to get this.
    head.was_fetched_via_service_worker = true;
    head.cors_exposed_header_names = response.cors_exposed_header_names;
    url_loader_client_->OnReceiveResponse(
        head, base::nullopt /* TODO(scottmg): ssl info */,
        mojom::DownloadedTempFilePtr());

    // Ideally, we would always get a data pipe fom SWFetchDispatcher and use
    // this case. See:
    // https://docs.google.com/a/google.com/document/d/1_ROmusFvd8ATwIZa29-P6Ls5yyLjfld0KvKchVfA84Y/edit?usp=drive_web
    if (!body_as_stream.is_null() && body_as_stream->stream.is_valid()) {
      url_loader_client_->OnStartLoadingResponseBody(
          std::move(body_as_stream->stream));
    } else {
      // TODO(scottmg): This is temporary way to load the blob right here and
      // turn it into a data pipe to respond with, until we are always able to
      // take the above path.
      if (!response.blob_uuid.empty() && blob_storage_context_) {
        std::unique_ptr<storage::BlobDataHandle> blob_data_handle =
            blob_storage_context_->GetBlobDataFromUUID(response.blob_uuid);
        blob_reader_ = blob_data_handle->CreateReader(
            nullptr /* file system context */,
            BrowserThread::GetTaskRunnerForThread(BrowserThread::FILE).get());
        CHECK(storage::BlobReader::Status::DONE ==
              blob_reader_->CalculateSize(net::CompletionCallback()));
        blob_reader_->SetReadRange(0, blob_reader_->total_size());
        scoped_refptr<net::IOBuffer> buffer(
            new net::IOBuffer(static_cast<size_t>(blob_reader_->total_size())));

        int bytes_read;
        blob_reader_->Read(buffer.get(), blob_reader_->total_size(),
                           &bytes_read,
                           base::Bind(&URLLoaderImpl::AfterRead,
                                      weak_factory_.GetWeakPtr(), buffer));
      }
    }
  }

  // mojom::URLLoader:
  void FollowRedirect() override { NOTIMPLEMENTED(); }

  void SetPriority(net::RequestPriority priority,
                   int32_t intra_priority_value) override {
    NOTIMPLEMENTED();
  }

 private:
  void AfterRead(scoped_refptr<net::IOBuffer> buffer, int bytes) {
    uint32_t bytes_written = static_cast<uint32_t>(bytes);
    mojo::WriteDataRaw(data_pipe_.producer_handle.get(), buffer->data(),
                       &bytes_written, MOJO_WRITE_DATA_FLAG_NONE);
    url_loader_client_->OnStartLoadingResponseBody(
        std::move(data_pipe_.consumer_handle));
  }

  base::WeakPtr<storage::BlobStorageContext> blob_storage_context_;
  mojom::URLLoaderClientPtr url_loader_client_;
  std::unique_ptr<storage::BlobReader> blob_reader_;
  mojo::DataPipe data_pipe_;

  base::WeakPtrFactory<URLLoaderImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(URLLoaderImpl);
};

}  // namespace

ServiceWorkerURLJobWrapper::ServiceWorkerURLJobWrapper(
    base::WeakPtr<ServiceWorkerURLRequestJob> url_request_job)
    : job_type_(JobType::kURLRequest),
      url_request_job_(std::move(url_request_job)),
      weak_factory_(this) {}

ServiceWorkerURLJobWrapper::ServiceWorkerURLJobWrapper(
    LoaderCallback callback,
    Delegate* delegate,
    const ResourceRequest& resource_request,
    base::WeakPtr<storage::BlobStorageContext> blob_storage_context)
    : job_type_(JobType::kURLLoader),
      loader_callback_(std::move(callback)),
      delegate_(delegate),
      resource_request_(resource_request),
      blob_storage_context_(blob_storage_context),
      weak_factory_(this) {
  DCHECK(IsBrowserSideNavigationEnabled() &&
         base::CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kEnableNetworkService));
}

ServiceWorkerURLJobWrapper::~ServiceWorkerURLJobWrapper() {}

void ServiceWorkerURLJobWrapper::FallbackToNetwork() {
  if (job_type_ == JobType::kURLLoader) {
    response_type_ = FALLBACK_TO_NETWORK;
    // This could be called multiple times in some cases because we simply
    // call this synchronously here and don't wait for a separate async
    // StartRequest cue like what URLRequestJob case does.
    // TODO(kinuko): Make sure this is ok or we need to make this async.
    if (!loader_callback_.is_null()) {
      std::move(loader_callback_).Run(StartLoaderCallback());
    }
  } else {
    url_request_job_->FallbackToNetwork();
  }
}

void ServiceWorkerURLJobWrapper::FallbackToNetworkOrRenderer() {
  if (job_type_ == JobType::kURLLoader) {
    // TODO(kinuko): Implement this. Now we always fallback to network.
    FallbackToNetwork();
  } else {
    url_request_job_->FallbackToNetworkOrRenderer();
  }
}

void ServiceWorkerURLJobWrapper::ForwardToServiceWorker() {
  if (job_type_ == JobType::kURLLoader) {
    response_type_ = FORWARD_TO_SERVICE_WORKER;
    StartRequest();
  } else {
    url_request_job_->ForwardToServiceWorker();
  }
}

bool ServiceWorkerURLJobWrapper::ShouldFallbackToNetwork() {
  if (job_type_ == JobType::kURLLoader) {
    return response_type_ == FALLBACK_TO_NETWORK;
  } else {
    return url_request_job_->ShouldFallbackToNetwork();
  }
}

ui::PageTransition ServiceWorkerURLJobWrapper::GetPageTransition() {
  if (job_type_ == JobType::kURLLoader) {
    NOTIMPLEMENTED();
    return ui::PAGE_TRANSITION_LINK;
  } else {
    const ResourceRequestInfo* info =
        ResourceRequestInfo::ForRequest(url_request_job_->request());
    // ResourceRequestInfo may not be set in some tests.
    if (!info)
      return ui::PAGE_TRANSITION_LINK;
    return info->GetPageTransition();
  }
}

size_t ServiceWorkerURLJobWrapper::GetURLChainSize() const {
  if (job_type_ == JobType::kURLLoader) {
    NOTIMPLEMENTED();
    return 0;
  } else {
    return url_request_job_->request()->url_chain().size();
  }
}

void ServiceWorkerURLJobWrapper::FailDueToLostController() {
  if (job_type_ == JobType::kURLLoader) {
    NOTIMPLEMENTED();
  } else {
    url_request_job_->FailDueToLostController();
  }
}

bool ServiceWorkerURLJobWrapper::WasCanceled() const {
  if (job_type_ == JobType::kURLLoader) {
    return loader_callback_.is_null();
  } else {
    return !url_request_job_;
  }
}

void ServiceWorkerURLJobWrapper::StartRequest() {
  DCHECK_EQ(FORWARD_TO_SERVICE_WORKER, response_type_);

  ServiceWorkerMetrics::URLRequestJobResult result =
      ServiceWorkerMetrics::REQUEST_JOB_ERROR_BAD_DELEGATE;
  ServiceWorkerVersion* active_worker =
      delegate_->GetServiceWorkerVersion(&result);

  fetch_dispatcher_.reset(new ServiceWorkerFetchDispatcher(
      CreateFetchRequest(resource_request_), active_worker,
      resource_request_.resource_type, base::nullopt,
      net::NetLogWithSource() /* TODO(scottmg): net log? */,
      base::Bind(&ServiceWorkerURLJobWrapper::DidPrepareFetchEvent,
                 weak_factory_.GetWeakPtr(), active_worker),
      base::Bind(&ServiceWorkerURLJobWrapper::DidDispatchFetchEvent,
                 weak_factory_.GetWeakPtr())));
  fetch_dispatcher_->Run();
}

std::unique_ptr<ServiceWorkerFetchRequest>
ServiceWorkerURLJobWrapper::CreateFetchRequest(const ResourceRequest& request) {
  std::string blob_uuid;
  uint64_t blob_size = 0;
  // TODO(scottmg): Implement passing body as blob to handler.
  DCHECK(!request.request_body);
  std::unique_ptr<ServiceWorkerFetchRequest> new_request(
      new ServiceWorkerFetchRequest());
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

void ServiceWorkerURLJobWrapper::DidPrepareFetchEvent(
    scoped_refptr<ServiceWorkerVersion> version) {}

void ServiceWorkerURLJobWrapper::DidDispatchFetchEvent(
    ServiceWorkerStatusCode status,
    ServiceWorkerFetchEventResult fetch_result,
    const ServiceWorkerResponse& response,
    blink::mojom::ServiceWorkerStreamHandlePtr body_as_stream,
    const scoped_refptr<ServiceWorkerVersion>& version) {
  if (fetch_result == SERVICE_WORKER_FETCH_EVENT_RESULT_FALLBACK) {
    std::move(loader_callback_).Run(StartLoaderCallback());
    return;
  }
  DCHECK_EQ(fetch_result, SERVICE_WORKER_FETCH_EVENT_RESULT_RESPONSE);
  std::move(loader_callback_)
      .Run(base::Bind(&URLLoaderImpl::Start, response,
                      base::Passed(std::move(body_as_stream)),
                      blob_storage_context_));
}

}  // namespace content
