// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/url_loader_client_impl.h"

#include "base/callback.h"
#include "base/single_thread_task_runner.h"
#include "content/child/resource_dispatcher.h"
#include "content/child/url_response_body_consumer.h"
#include "content/common/resource_messages.h"
#include "net/url_request/redirect_info.h"

namespace content {

URLLoaderClientImpl::URLLoaderClientImpl(
    int request_id,
    ResourceDispatcher* resource_dispatcher,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : binding_(this),
      request_id_(request_id),
      resource_dispatcher_(resource_dispatcher),
      task_runner_(std::move(task_runner)),
      weak_factory_(this) {}

URLLoaderClientImpl::~URLLoaderClientImpl() {
  if (body_consumer_)
    body_consumer_->Cancel();
}

void URLLoaderClientImpl::Bind(mojom::URLLoaderClientPtr* client_ptr) {
  binding_.Bind(client_ptr, task_runner_);
}

void URLLoaderClientImpl::SetDefersLoading() {
  is_deferred_ = true;
  if (body_consumer_)
    body_consumer_->SetDefersLoading();
}

void URLLoaderClientImpl::UnsetDefersLoading() {
  is_deferred_ = false;
}

void URLLoaderClientImpl::FlushDeferredMessages() {
  DCHECK(!is_deferred_);
  std::vector<IPC::Message> messages;
  messages.swap(deferred_messages_);
  bool has_completion_message = false;
  base::WeakPtr<URLLoaderClientImpl> weak_this = weak_factory_.GetWeakPtr();
  // First, dispatch all messages excluding the followings:
  //  - response body (dispatched by |body_consumer_|)
  //  - transfer size change (dispatched later)
  //  - completion (dispatched by |body_consumer_| or dispatched later)
  for (size_t index = 0; index < messages.size(); ++index) {
    if (messages[index].type() == ResourceMsg_RequestComplete::ID) {
      // The completion message arrives at the end of the message queue.
      DCHECK(!has_completion_message);
      DCHECK_EQ(index, messages.size() - 1);
      has_completion_message = true;
      break;
    }
    Dispatch(messages[index]);
    if (!weak_this)
      return;
    if (is_deferred_) {
      deferred_messages_.insert(deferred_messages_.begin(),
                                messages.begin() + index + 1, messages.end());
      return;
    }
  }

  // Dispatch the transfer size update.
  if (accumulated_transfer_size_diff_during_deferred_ > 0) {
    auto transfer_size_diff = accumulated_transfer_size_diff_during_deferred_;
    accumulated_transfer_size_diff_during_deferred_ = 0;
    resource_dispatcher_->OnTransferSizeUpdated(request_id_,
                                                transfer_size_diff);
    if (!weak_this)
      return;
    if (is_deferred_) {
      if (has_completion_message) {
        DCHECK_GT(messages.size(), 0u);
        DCHECK_EQ(messages.back().type(),
                  static_cast<uint32_t>(ResourceMsg_RequestComplete::ID));
        deferred_messages_.emplace_back(std::move(messages.back()));
      }
      return;
    }
  }

  if (body_consumer_) {
    // When we have |body_consumer_|, the completion message is dispatched by
    // it, not by this object.
    DCHECK(!has_completion_message);
    // Dispatch the response body.
    body_consumer_->UnsetDefersLoading();
    return;
  }

  // Dispatch the completion message.
  if (has_completion_message) {
    DCHECK_GT(messages.size(), 0u);
    DCHECK_EQ(messages.back().type(),
              static_cast<uint32_t>(ResourceMsg_RequestComplete::ID));
    Dispatch(messages.back());
  }
}

void URLLoaderClientImpl::OnReceiveResponse(
    const ResourceResponseHead& response_head,
    mojom::DownloadedTempFilePtr downloaded_file) {
  has_received_response_ = true;
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

void URLLoaderClientImpl::OnReceiveCachedMetadata(
    const std::vector<uint8_t>& data) {
  const char* data_ptr = reinterpret_cast<const char*>(data.data());
  Dispatch(ResourceMsg_ReceivedCachedMetadata(
      request_id_, std::vector<char>(data_ptr, data_ptr + data.size())));
}

void URLLoaderClientImpl::OnTransferSizeUpdated(int32_t transfer_size_diff) {
  if (is_deferred_) {
    accumulated_transfer_size_diff_during_deferred_ += transfer_size_diff;
  } else {
    resource_dispatcher_->OnTransferSizeUpdated(request_id_,
                                                transfer_size_diff);
  }
}

void URLLoaderClientImpl::OnStartLoadingResponseBody(
    mojo::ScopedDataPipeConsumerHandle body) {
  DCHECK(!body_consumer_);
  DCHECK(has_received_response_);
  body_consumer_ = new URLResponseBodyConsumer(
      request_id_, resource_dispatcher_, std::move(body), task_runner_);
  if (is_deferred_)
    body_consumer_->SetDefersLoading();
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
  if (is_deferred_) {
    deferred_messages_.push_back(message);
  } else if (deferred_messages_.size() > 0) {
    deferred_messages_.push_back(message);
    FlushDeferredMessages();
  } else {
    resource_dispatcher_->DispatchMessage(message);
  }
}

void URLLoaderClientImpl::OnUploadProgress(int64_t current_position,
                                           int64_t total_size,
                                           const base::Closure& ack_callback) {
  Dispatch(
      ResourceMsg_UploadProgress(request_id_, current_position, total_size));
  ack_callback.Run();
}

}  // namespace content
