// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/throttling_url_loader.h"

namespace content {

// static
std::unique_ptr<ThrottlingURLLoader> ThrottlingURLLoader::CreateLoaderAndStart(
    mojom::URLLoaderFactory* factory,
    std::vector<std::unique_ptr<URLLoaderThrottle>> throttles,
    int32_t routing_id,
    int32_t request_id,
    uint32_t options,
    std::unique_ptr<ResourceRequest> url_request,
    mojom::URLLoaderClient* client) {
  std::unique_ptr<ThrottlingURLLoader> loader(
      new ThrottlingURLLoader(std::move(throttles), client));
  loader->Start(factory, routing_id, request_id, options,
                std::move(url_request));
  return loader;
}

ThrottlingURLLoader::~ThrottlingURLLoader() {}

void ThrottlingURLLoader::FollowRedirect() {
  if (url_loader_)
    url_loader_->FollowRedirect();
}

void ThrottlingURLLoader::SetPriority(net::RequestPriority priority,
                                      int32_t intra_priority_value) {
  if (!url_loader_ && !cancelled_by_throttle_) {
    DCHECK_EQ(DEFERRED_START, deferred_stage_);
    set_priority_cached_ = true;
    priority_ = priority;
    intra_priority_value_ = intra_priority_value;
    return;
  }

  url_loader_->SetPriority(priority, intra_priority_value);
}

ThrottlingURLLoader::ThrottlingURLLoader(
    std::vector<std::unique_ptr<URLLoaderThrottle>> throttles,
    mojom::URLLoaderClient* client)
    : forwarding_client_(client), client_binding_(this) {
  if (throttles.size() > 0) {
    // TODO(yzshen): Implement a URLLoaderThrottle subclass which handles a list
    // of URLLoaderThrottles.
    CHECK_EQ(1u, throttles.size());
    throttle_ = std::move(throttles[0]);
    throttle_->set_delegate(this);
  }
}

void ThrottlingURLLoader::Start(mojom::URLLoaderFactory* factory,
                                int32_t routing_id,
                                int32_t request_id,
                                uint32_t options,
                                std::unique_ptr<ResourceRequest> url_request) {
  DCHECK_EQ(DEFERRED_NONE, deferred_stage_);
  DCHECK(!cancelled_by_throttle_);

  if (throttle_) {
    bool deferred = false;
    throttle_->WillStartRequest(url_request->url, url_request->load_flags,
                                url_request->resource_type, &deferred);
    if (cancelled_by_throttle_)
      return;

    if (deferred) {
      deferred_stage_ = DEFERRED_START;
      url_loader_factory_ = factory;
      routing_id_ = routing_id;
      request_id_ = request_id;
      options_ = options;
      url_request_ = std::move(url_request);
      return;
    }
  }

  factory->CreateLoaderAndStart(mojo::MakeRequest(&url_loader_), routing_id,
                                request_id, options, *url_request,
                                client_binding_.CreateInterfacePtrAndBind());
}

void ThrottlingURLLoader::OnReceiveResponse(
    const ResourceResponseHead& response_head,
    const base::Optional<net::SSLInfo>& ssl_info,
    mojom::DownloadedTempFilePtr downloaded_file) {
  DCHECK_EQ(DEFERRED_NONE, deferred_stage_);
  DCHECK(!cancelled_by_throttle_);

  if (throttle_) {
    bool deferred = false;
    throttle_->WillProcessResponse(&deferred);
    if (cancelled_by_throttle_)
      return;

    if (deferred) {
      deferred_stage_ = DEFERRED_RESPONSE;
      response_head_ = response_head;
      ssl_info_ = ssl_info;
      downloaded_file_ = std::move(downloaded_file);
      client_binding_.PauseIncomingMethodCallProcessing();
      return;
    }
  }

  forwarding_client_->OnReceiveResponse(response_head, ssl_info,
                                        std::move(downloaded_file));
}

void ThrottlingURLLoader::OnReceiveRedirect(
    const net::RedirectInfo& redirect_info,
    const ResourceResponseHead& response_head) {
  DCHECK_EQ(DEFERRED_NONE, deferred_stage_);
  DCHECK(!cancelled_by_throttle_);

  if (throttle_) {
    bool deferred = false;
    throttle_->WillRedirectRequest(redirect_info, &deferred);
    if (cancelled_by_throttle_)
      return;

    if (deferred) {
      deferred_stage_ = DEFERRED_REDIRECT;
      redirect_info_ = redirect_info;
      response_head_ = response_head;
      client_binding_.PauseIncomingMethodCallProcessing();
      return;
    }
  }

  forwarding_client_->OnReceiveRedirect(redirect_info, response_head);
}

void ThrottlingURLLoader::OnDataDownloaded(int64_t data_len,
                                           int64_t encoded_data_len) {
  DCHECK_EQ(DEFERRED_NONE, deferred_stage_);
  DCHECK(!cancelled_by_throttle_);

  forwarding_client_->OnDataDownloaded(data_len, encoded_data_len);
}

void ThrottlingURLLoader::OnUploadProgress(
    int64_t current_position,
    int64_t total_size,
    OnUploadProgressCallback ack_callback) {
  DCHECK_EQ(DEFERRED_NONE, deferred_stage_);
  DCHECK(!cancelled_by_throttle_);

  forwarding_client_->OnUploadProgress(current_position, total_size,
                                       std::move(ack_callback));
}

void ThrottlingURLLoader::OnReceiveCachedMetadata(
    const std::vector<uint8_t>& data) {
  DCHECK_EQ(DEFERRED_NONE, deferred_stage_);
  DCHECK(!cancelled_by_throttle_);

  forwarding_client_->OnReceiveCachedMetadata(data);
}

void ThrottlingURLLoader::OnTransferSizeUpdated(int32_t transfer_size_diff) {
  DCHECK_EQ(DEFERRED_NONE, deferred_stage_);
  DCHECK(!cancelled_by_throttle_);

  forwarding_client_->OnTransferSizeUpdated(transfer_size_diff);
}

void ThrottlingURLLoader::OnStartLoadingResponseBody(
    mojo::ScopedDataPipeConsumerHandle body) {
  DCHECK_EQ(DEFERRED_NONE, deferred_stage_);
  DCHECK(!cancelled_by_throttle_);

  forwarding_client_->OnStartLoadingResponseBody(std::move(body));
}

void ThrottlingURLLoader::OnComplete(
    const ResourceRequestCompletionStatus& status) {
  DCHECK_EQ(DEFERRED_NONE, deferred_stage_);
  DCHECK(!cancelled_by_throttle_);

  forwarding_client_->OnComplete(status);
}

void ThrottlingURLLoader::CancelWithError(int error_code) {
  // TODO(yzshen): Support a mode that cancellation also deletes the disk cache
  // entry.
  if (cancelled_by_throttle_)
    return;

  cancelled_by_throttle_ = true;

  ResourceRequestCompletionStatus request_complete_data;
  request_complete_data.error_code = error_code;
  request_complete_data.completion_time = base::TimeTicks::Now();

  deferred_stage_ = DEFERRED_NONE;
  client_binding_.Close();
  url_loader_.reset();

  forwarding_client_->OnComplete(request_complete_data);
}

void ThrottlingURLLoader::Resume() {
  if (cancelled_by_throttle_ || deferred_stage_ == DEFERRED_NONE)
    return;

  switch (deferred_stage_) {
    case DEFERRED_START: {
      url_loader_factory_->CreateLoaderAndStart(
          mojo::MakeRequest(&url_loader_), routing_id_, request_id_, options_,
          *url_request_, client_binding_.CreateInterfacePtrAndBind());

      if (set_priority_cached_) {
        set_priority_cached_ = false;
        url_loader_->SetPriority(priority_, intra_priority_value_);
      }
      break;
    }
    case DEFERRED_REDIRECT: {
      client_binding_.ResumeIncomingMethodCallProcessing();
      forwarding_client_->OnReceiveRedirect(redirect_info_, response_head_);
      break;
    }
    case DEFERRED_RESPONSE: {
      client_binding_.ResumeIncomingMethodCallProcessing();
      forwarding_client_->OnReceiveResponse(response_head_, ssl_info_,
                                            std::move(downloaded_file_));
      break;
    }
    default:
      NOTREACHED();
      break;
  }
  deferred_stage_ = DEFERRED_NONE;
}

}  // namespace content
