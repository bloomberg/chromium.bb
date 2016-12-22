// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/url_loader_client_impl.h"

#include "base/callback.h"
#include "base/single_thread_task_runner.h"
#include "content/child/resource_dispatcher.h"
#include "content/child/url_response_body_consumer.h"
#include "content/common/resource_messages.h"
#include "mojo/public/cpp/bindings/associated_group.h"
#include "net/url_request/redirect_info.h"

namespace content {

URLLoaderClientImpl::URLLoaderClientImpl(
    int request_id,
    ResourceDispatcher* resource_dispatcher,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : binding_(this),
      request_id_(request_id),
      resource_dispatcher_(resource_dispatcher),
      task_runner_(std::move(task_runner)) {}

URLLoaderClientImpl::~URLLoaderClientImpl() {
  if (body_consumer_)
    body_consumer_->Cancel();
}

void URLLoaderClientImpl::Bind(
    mojom::URLLoaderClientAssociatedPtrInfo* client_ptr_info,
    mojo::AssociatedGroup* associated_group) {
  binding_.Bind(client_ptr_info, associated_group);
}

void URLLoaderClientImpl::OnReceiveResponse(
    const ResourceResponseHead& response_head,
    mojom::DownloadedTempFilePtr downloaded_file) {
  has_received_response_ = true;
  if (body_consumer_)
    body_consumer_->Start();
  downloaded_file_ = std::move(downloaded_file);
  Dispatch(ResourceMsg_ReceivedResponse(request_id_, response_head));
}

void URLLoaderClientImpl::OnReceiveRedirect(
    const net::RedirectInfo& redirect_info,
    const ResourceResponseHead& response_head) {
  DCHECK(!has_received_response_);
  DCHECK(!body_consumer_);
  Dispatch(
      ResourceMsg_ReceivedRedirect(request_id_, redirect_info, response_head));
}

void URLLoaderClientImpl::OnDataDownloaded(int64_t data_len,
                                           int64_t encoded_data_len) {
  Dispatch(ResourceMsg_DataDownloaded(request_id_, data_len, encoded_data_len));
}

void URLLoaderClientImpl::OnTransferSizeUpdated(int32_t transfer_size_diff) {
  resource_dispatcher_->OnTransferSizeUpdated(request_id_,
                                              transfer_size_diff);
}

void URLLoaderClientImpl::OnStartLoadingResponseBody(
    mojo::ScopedDataPipeConsumerHandle body) {
  DCHECK(!body_consumer_);
  body_consumer_ = new URLResponseBodyConsumer(
      request_id_, resource_dispatcher_, std::move(body), task_runner_);
  if (has_received_response_)
    body_consumer_->Start();
}

void URLLoaderClientImpl::OnComplete(
    const ResourceRequestCompletionStatus& status) {
  if (!body_consumer_) {
    Dispatch(ResourceMsg_RequestComplete(request_id_, status));
    return;
  }
  body_consumer_->OnComplete(status);
}

void URLLoaderClientImpl::Dispatch(const IPC::Message& message) {
  resource_dispatcher_->OnMessageReceived(message);
}

}  // namespace content
