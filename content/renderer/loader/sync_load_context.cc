// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/loader/sync_load_context.h"

#include <string>

#include "base/logging.h"
#include "base/synchronization/waitable_event.h"
#include "content/public/common/resource_request.h"
#include "content/public/common/resource_response_info.h"
#include "content/public/common/url_loader_throttle.h"
#include "content/renderer/loader/sync_load_response.h"
#include "net/url_request/redirect_info.h"

namespace content {

// static
void SyncLoadContext::StartAsyncWithWaitableEvent(
    std::unique_ptr<ResourceRequest> request,
    int routing_id,
    const url::Origin& frame_origin,
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    mojom::URLLoaderFactoryPtrInfo url_loader_factory_pipe,
    std::vector<std::unique_ptr<URLLoaderThrottle>> throttles,
    SyncLoadResponse* response,
    base::WaitableEvent* event) {
  auto* context = new SyncLoadContext(
      request.get(), std::move(url_loader_factory_pipe), response, event);

  context->request_id_ = context->resource_dispatcher_->StartAsync(
      std::move(request), routing_id, nullptr, frame_origin, traffic_annotation,
      true /* is_sync */, base::WrapUnique(context),
      blink::WebURLRequest::LoadingIPCType::kMojo,
      context->url_loader_factory_.get(), std::move(throttles),
      mojo::ScopedDataPipeConsumerHandle());
}

SyncLoadContext::SyncLoadContext(
    ResourceRequest* request,
    mojom::URLLoaderFactoryPtrInfo url_loader_factory,
    SyncLoadResponse* response,
    base::WaitableEvent* event)
    : response_(response), event_(event) {
  url_loader_factory_.Bind(std::move(url_loader_factory));

  // Constructs a new ResourceDispatcher specifically for this request.
  resource_dispatcher_ = std::make_unique<ResourceDispatcher>(
      nullptr, base::ThreadTaskRunnerHandle::Get());

  // Initialize the final URL with the original request URL. It will be
  // overwritten on redirects.
  response_->url = request->url;
}

SyncLoadContext::~SyncLoadContext() {}

void SyncLoadContext::OnUploadProgress(uint64_t position, uint64_t size) {}

bool SyncLoadContext::OnReceivedRedirect(const net::RedirectInfo& redirect_info,
                                         const ResourceResponseInfo& info) {
  if (redirect_info.new_url.GetOrigin() != response_->url.GetOrigin()) {
    LOG(ERROR) << "Cross origin redirect denied";
    response_->error_code = net::ERR_ABORTED;
    event_->Signal();

    // Returning false here will cause the request to be cancelled and this
    // object deleted.
    return false;
  }

  response_->url = redirect_info.new_url;
  return true;
}

void SyncLoadContext::OnReceivedResponse(const ResourceResponseInfo& info) {
  response_->headers = info.headers;
  response_->mime_type = info.mime_type;
  response_->charset = info.charset;
  response_->request_time = info.request_time;
  response_->response_time = info.response_time;
  response_->load_timing = info.load_timing;
  response_->devtools_info = info.devtools_info;
  response_->download_file_path = info.download_file_path;
  response_->socket_address = info.socket_address;
}

void SyncLoadContext::OnDownloadedData(int len, int encoded_data_length) {
  // This method is only called when RequestInfo::download_to_file is true which
  // is not allowed when processing a synchronous request.
  NOTREACHED();
}

void SyncLoadContext::OnReceivedData(std::unique_ptr<ReceivedData> data) {
  response_->data.append(data->payload(), data->length());
}

void SyncLoadContext::OnTransferSizeUpdated(int transfer_size_diff) {}

void SyncLoadContext::OnCompletedRequest(
    const network::URLLoaderCompletionStatus& status) {
  response_->error_code = status.error_code;
  if (status.cors_error_status)
    response_->cors_error = status.cors_error_status->cors_error;
  response_->encoded_data_length = status.encoded_data_length;
  response_->encoded_body_length = status.encoded_body_length;
  event_->Signal();

  // This will indirectly cause this object to be deleted.
  resource_dispatcher_->RemovePendingRequest(request_id_);
}

}  // namespace content
