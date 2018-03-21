// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/loader/sync_load_context.h"

#include <string>

#include "base/logging.h"
#include "base/synchronization/waitable_event.h"
#include "content/public/common/url_loader_throttle.h"
#include "content/renderer/loader/sync_load_response.h"
#include "net/url_request/redirect_info.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/resource_response_info.h"

namespace content {

// static
void SyncLoadContext::StartAsyncWithWaitableEvent(
    std::unique_ptr<network::ResourceRequest> request,
    int routing_id,
    scoped_refptr<base::SingleThreadTaskRunner> loading_task_runner,
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    std::unique_ptr<network::SharedURLLoaderFactoryInfo>
        url_loader_factory_info,
    std::vector<std::unique_ptr<URLLoaderThrottle>> throttles,
    SyncLoadResponse* response,
    base::WaitableEvent* event) {
  auto* context =
      new SyncLoadContext(request.get(), std::move(url_loader_factory_info),
                          response, event, loading_task_runner);

  context->request_id_ = context->resource_dispatcher_->StartAsync(
      std::move(request), routing_id, std::move(loading_task_runner),
      traffic_annotation, true /* is_sync */, base::WrapUnique(context),
      context->url_loader_factory_, std::move(throttles),
      network::mojom::URLLoaderClientEndpointsPtr(),
      nullptr /* continue_for_navigation */);
}

SyncLoadContext::SyncLoadContext(
    network::ResourceRequest* request,
    std::unique_ptr<network::SharedURLLoaderFactoryInfo> url_loader_factory,
    SyncLoadResponse* response,
    base::WaitableEvent* event,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : response_(response), event_(event), task_runner_(std::move(task_runner)) {
  url_loader_factory_ =
      network::SharedURLLoaderFactory::Create(std::move(url_loader_factory));

  // Constructs a new ResourceDispatcher specifically for this request.
  resource_dispatcher_ = std::make_unique<ResourceDispatcher>();

  // Initialize the final URL with the original request URL. It will be
  // overwritten on redirects.
  response_->url = request->url;
}

SyncLoadContext::~SyncLoadContext() {}

void SyncLoadContext::OnUploadProgress(uint64_t position, uint64_t size) {}

bool SyncLoadContext::OnReceivedRedirect(
    const net::RedirectInfo& redirect_info,
    const network::ResourceResponseInfo& info) {
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

void SyncLoadContext::OnReceivedResponse(
    const network::ResourceResponseInfo& info) {
  response_->headers = info.headers;
  response_->mime_type = info.mime_type;
  response_->charset = info.charset;
  response_->request_time = info.request_time;
  response_->response_time = info.response_time;
  response_->load_timing = info.load_timing;
  response_->raw_request_response_info = info.raw_request_response_info;
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
  response_->extended_error_code = status.extended_error_code;
  if (status.cors_error_status)
    response_->cors_error = status.cors_error_status->cors_error;
  response_->encoded_data_length = status.encoded_data_length;
  response_->encoded_body_length = status.encoded_body_length;
  event_->Signal();

  // This will indirectly cause this object to be deleted.
  resource_dispatcher_->RemovePendingRequest(request_id_, task_runner_);
}

}  // namespace content
