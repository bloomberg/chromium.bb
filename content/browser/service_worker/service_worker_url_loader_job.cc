// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_url_loader_job.h"

#include "base/command_line.h"
#include "content/browser/service_worker/service_worker_fetch_dispatcher.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/common/service_worker/service_worker_utils.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/common/browser_side_navigation_policy.h"
#include "content/public/common/content_switches.h"
#include "net/base/io_buffer.h"
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
      binding_(this),
      weak_factory_(this) {
  DCHECK(IsBrowserSideNavigationEnabled() &&
         base::CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kEnableNetworkService));
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
  canceled_ = true;
}

bool ServiceWorkerURLLoaderJob::WasCanceled() const {
  return canceled_;
}

void ServiceWorkerURLLoaderJob::StartRequest() {
  DCHECK_EQ(FORWARD_TO_SERVICE_WORKER, response_type_);

  ServiceWorkerMetrics::URLRequestJobResult result =
      ServiceWorkerMetrics::REQUEST_JOB_ERROR_BAD_DELEGATE;
  ServiceWorkerVersion* active_worker =
      delegate_->GetServiceWorkerVersion(&result);

  fetch_dispatcher_.reset(new ServiceWorkerFetchDispatcher(
      CreateFetchRequest(resource_request_), active_worker,
      resource_request_.resource_type, base::nullopt,
      net::NetLogWithSource() /* TODO(scottmg): net log? */,
      base::Bind(&ServiceWorkerURLLoaderJob::DidPrepareFetchEvent,
                 weak_factory_.GetWeakPtr(), active_worker),
      base::Bind(&ServiceWorkerURLLoaderJob::DidDispatchFetchEvent,
                 weak_factory_.GetWeakPtr())));
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

void ServiceWorkerURLLoaderJob::DidPrepareFetchEvent(
    scoped_refptr<ServiceWorkerVersion> version) {}

void ServiceWorkerURLLoaderJob::DidDispatchFetchEvent(
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
      blob_reader_->Read(buffer.get(), blob_reader_->total_size(), &bytes_read,
                         base::Bind(&ServiceWorkerURLLoaderJob::AfterRead,
                                    weak_factory_.GetWeakPtr(), buffer));
    }
  }
}

void ServiceWorkerURLLoaderJob::AfterRead(scoped_refptr<net::IOBuffer> buffer,
                                          int bytes) {
  uint32_t bytes_written = static_cast<uint32_t>(bytes);
  mojo::WriteDataRaw(data_pipe_.producer_handle.get(), buffer->data(),
                     &bytes_written, MOJO_WRITE_DATA_FLAG_NONE);
  url_loader_client_->OnStartLoadingResponseBody(
      std::move(data_pipe_.consumer_handle));
}

void ServiceWorkerURLLoaderJob::FollowRedirect() {
  NOTIMPLEMENTED();
}

void ServiceWorkerURLLoaderJob::SetPriority(net::RequestPriority priority,
                                            int32_t intra_priority_value) {
  NOTIMPLEMENTED();
}

}  // namespace content
