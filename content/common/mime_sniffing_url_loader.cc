// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/mime_sniffing_url_loader.h"

#include "base/bind.h"
#include "content/common/mime_sniffing_throttle.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "net/base/mime_sniffer.h"

namespace content {

// static
const char MimeSniffingURLLoader::kDefaultMimeType[] = "text/plain";

// static
std::tuple<network::mojom::URLLoaderPtr,
           network::mojom::URLLoaderClientRequest,
           MimeSniffingURLLoader*>
MimeSniffingURLLoader::CreateLoader(
    base::WeakPtr<MimeSniffingThrottle> throttle,
    const GURL& response_url,
    const network::ResourceResponseHead& response_head,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  network::mojom::URLLoaderPtr url_loader;
  network::mojom::URLLoaderClientPtr url_loader_client;
  network::mojom::URLLoaderClientRequest url_loader_client_request =
      mojo::MakeRequest(&url_loader_client);
  auto loader = base::WrapUnique(new MimeSniffingURLLoader(
      std::move(throttle), response_url, response_head,
      std::move(url_loader_client), std::move(task_runner)));
  MimeSniffingURLLoader* loader_rawptr = loader.get();
  mojo::MakeStrongBinding(std::move(loader), mojo::MakeRequest(&url_loader));
  return std::make_tuple(std::move(url_loader),
                         std::move(url_loader_client_request), loader_rawptr);
}

MimeSniffingURLLoader::MimeSniffingURLLoader(
    base::WeakPtr<MimeSniffingThrottle> throttle,
    const GURL& response_url,
    const network::ResourceResponseHead& response_head,
    network::mojom::URLLoaderClientPtr destination_url_loader_client,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : throttle_(throttle),
      source_url_client_binding_(this),
      destination_url_loader_client_(std::move(destination_url_loader_client)),
      response_url_(response_url),
      response_head_(response_head),
      task_runner_(task_runner),
      body_consumer_watcher_(FROM_HERE,
                             mojo::SimpleWatcher::ArmingPolicy::MANUAL,
                             task_runner),
      body_producer_watcher_(FROM_HERE,
                             mojo::SimpleWatcher::ArmingPolicy::MANUAL,
                             std::move(task_runner)) {}

MimeSniffingURLLoader::~MimeSniffingURLLoader() = default;

void MimeSniffingURLLoader::Start(
    network::mojom::URLLoaderPtr source_url_loader,
    network::mojom::URLLoaderClientRequest source_url_loader_client_request) {
  source_url_loader_ = std::move(source_url_loader);
  source_url_client_binding_.Bind(std::move(source_url_loader_client_request),
                                  task_runner_);
}

void MimeSniffingURLLoader::OnReceiveResponse(
    const network::ResourceResponseHead& response_head) {
  // OnReceiveResponse() shouldn't be called because MimeSniffingURLLoader is
  // created by MimeSniffingThrottle::WillProcessResponse(), which is equivalent
  // to OnReceiveResponse().
  NOTREACHED();
}

void MimeSniffingURLLoader::OnReceiveRedirect(
    const net::RedirectInfo& redirect_info,
    const network::ResourceResponseHead& response_head) {
  // OnReceiveRedirect() shouldn't be called because MimeSniffingURLLoader is
  // created by MimeSniffingThrottle::WillProcessResponse(), which is equivalent
  // to OnReceiveResponse().
  NOTREACHED();
}

void MimeSniffingURLLoader::OnUploadProgress(
    int64_t current_position,
    int64_t total_size,
    OnUploadProgressCallback ack_callback) {
  destination_url_loader_client_->OnUploadProgress(current_position, total_size,
                                                   std::move(ack_callback));
}

void MimeSniffingURLLoader::OnReceiveCachedMetadata(
    const std::vector<uint8_t>& data) {
  destination_url_loader_client_->OnReceiveCachedMetadata(data);
}

void MimeSniffingURLLoader::OnTransferSizeUpdated(int32_t transfer_size_diff) {
  destination_url_loader_client_->OnTransferSizeUpdated(transfer_size_diff);
}

void MimeSniffingURLLoader::OnStartLoadingResponseBody(
    mojo::ScopedDataPipeConsumerHandle body) {
  state_ = State::kSniffing;
  body_consumer_handle_ = std::move(body);
  body_consumer_watcher_.Watch(
      body_consumer_handle_.get(),
      MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
      base::BindRepeating(&MimeSniffingURLLoader::OnBodyReadable,
                          base::Unretained(this)));
  body_consumer_watcher_.ArmOrNotify();
}

void MimeSniffingURLLoader::OnComplete(
    const network::URLLoaderCompletionStatus& status) {
  DCHECK(!complete_status_.has_value());
  switch (state_) {
    case State::kWaitForBody:
      // An error occured before receiving any data.
      DCHECK_NE(net::OK, status.error_code);
      state_ = State::kCompleted;
      response_head_.mime_type = kDefaultMimeType;
      if (!throttle_) {
        Abort();
        return;
      }
      throttle_->ResumeWithNewResponseHead(response_head_);
      destination_url_loader_client_->OnComplete(status);
      return;
    case State::kSniffing:
    case State::kSending:
      // Defer calling OnComplete() until mime sniffing has finished and all
      // data is sent.
      complete_status_ = status;
      return;
    case State::kCompleted:
      destination_url_loader_client_->OnComplete(status);
      return;
    case State::kAborted:
      NOTREACHED();
      return;
  }
  NOTREACHED();
}

void MimeSniffingURLLoader::FollowRedirect(
    const std::vector<std::string>& removed_headers,
    const net::HttpRequestHeaders& modified_headers,
    const base::Optional<GURL>& new_url) {
  // MimeSniffingURLLoader starts handling the request after
  // OnReceivedResponse(). A redirect response is not expected.
  NOTREACHED();
}

void MimeSniffingURLLoader::ProceedWithResponse() {
  if (state_ == State::kAborted)
    return;
  source_url_loader_->ProceedWithResponse();
}

void MimeSniffingURLLoader::SetPriority(net::RequestPriority priority,
                                        int32_t intra_priority_value) {
  if (state_ == State::kAborted)
    return;
  source_url_loader_->SetPriority(priority, intra_priority_value);
}

void MimeSniffingURLLoader::PauseReadingBodyFromNet() {
  if (state_ == State::kAborted)
    return;
  source_url_loader_->PauseReadingBodyFromNet();
}

void MimeSniffingURLLoader::ResumeReadingBodyFromNet() {
  if (state_ == State::kAborted)
    return;
  source_url_loader_->ResumeReadingBodyFromNet();
}

void MimeSniffingURLLoader::OnBodyReadable(MojoResult) {
  if (state_ == State::kSending) {
    // The pipe becoming readable when kSending means all buffered body has
    // already been sent.
    ForwardBodyToClient();
    return;
  }
  DCHECK_EQ(State::kSniffing, state_);

  size_t start_size = buffered_body_.size();
  uint32_t read_bytes = net::kMaxBytesToSniff;
  buffered_body_.resize(start_size + read_bytes);
  MojoResult result =
      body_consumer_handle_->ReadData(buffered_body_.data() + start_size,
                                      &read_bytes, MOJO_READ_DATA_FLAG_NONE);
  switch (result) {
    case MOJO_RESULT_OK:
      break;
    case MOJO_RESULT_FAILED_PRECONDITION:
      // Finished the body before mime type is completely decided.
      buffered_body_.resize(start_size);
      CompleteSniffing();
      return;
    case MOJO_RESULT_SHOULD_WAIT:
      body_consumer_watcher_.ArmOrNotify();
      return;
    default:
      NOTREACHED();
      return;
  }

  DCHECK_EQ(MOJO_RESULT_OK, result);
  buffered_body_.resize(start_size + read_bytes);
  std::string new_type;
  bool made_final_decision =
      net::SniffMimeType(buffered_body_.data(), buffered_body_.size(),
                         response_url_, response_head_.mime_type,
                         net::ForceSniffFileUrlsForHtml::kDisabled, &new_type);
  response_head_.mime_type = new_type;
  response_head_.did_mime_sniff = true;
  if (made_final_decision) {
    CompleteSniffing();
    return;
  }
  body_consumer_watcher_.ArmOrNotify();
}

void MimeSniffingURLLoader::OnBodyWritable(MojoResult) {
  DCHECK_EQ(State::kSending, state_);
  if (bytes_remaining_in_buffer_ > 0) {
    SendReceivedBodyToClient();
  } else {
    ForwardBodyToClient();
  }
}

void MimeSniffingURLLoader::CompleteSniffing() {
  DCHECK_EQ(State::kSniffing, state_);
  if (buffered_body_.empty()) {
    // The URLLoader ended before sending any data. There is not enough
    // information to determine the MIME type.
    response_head_.mime_type = kDefaultMimeType;
  }

  state_ = State::kSending;
  bytes_remaining_in_buffer_ = buffered_body_.size();
  if (!throttle_) {
    Abort();
    return;
  }
  throttle_->ResumeWithNewResponseHead(response_head_);
  mojo::ScopedDataPipeConsumerHandle body_to_send;
  MojoResult result =
      mojo::CreateDataPipe(nullptr, &body_producer_handle_, &body_to_send);
  if (result != MOJO_RESULT_OK) {
    Abort();
    return;
  }
  // Set up the watcher for the producer handle.
  body_producer_watcher_.Watch(
      body_producer_handle_.get(),
      MOJO_HANDLE_SIGNAL_WRITABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
      base::BindRepeating(&MimeSniffingURLLoader::OnBodyWritable,
                          base::Unretained(this)));

  // Send deferred message.
  destination_url_loader_client_->OnStartLoadingResponseBody(
      std::move(body_to_send));

  if (bytes_remaining_in_buffer_) {
    SendReceivedBodyToClient();
    return;
  }

  CompleteSending();
}

void MimeSniffingURLLoader::CompleteSending() {
  DCHECK_EQ(State::kSending, state_);
  state_ = State::kCompleted;
  // Call client's OnComplete() if |this|'s OnComplete() has already been
  // called.
  if (complete_status_.has_value())
    destination_url_loader_client_->OnComplete(complete_status_.value());

  body_consumer_watcher_.Cancel();
  body_producer_watcher_.Cancel();
  body_consumer_handle_.reset();
  body_producer_handle_.reset();
}

void MimeSniffingURLLoader::SendReceivedBodyToClient() {
  DCHECK_EQ(State::kSending, state_);
  // Send the buffered data first.
  DCHECK_GT(bytes_remaining_in_buffer_, 0u);
  size_t start_position = buffered_body_.size() - bytes_remaining_in_buffer_;
  uint32_t bytes_sent = bytes_remaining_in_buffer_;
  MojoResult result =
      body_producer_handle_->WriteData(buffered_body_.data() + start_position,
                                       &bytes_sent, MOJO_WRITE_DATA_FLAG_NONE);
  switch (result) {
    case MOJO_RESULT_OK:
      break;
    case MOJO_RESULT_FAILED_PRECONDITION:
      // The pipe is closed unexpectedly. |this| should be deleted once
      // URLLoaderPtr on the destination is released.
      Abort();
      return;
    case MOJO_RESULT_SHOULD_WAIT:
      body_producer_watcher_.ArmOrNotify();
      return;
    default:
      NOTREACHED();
      return;
  }
  bytes_remaining_in_buffer_ -= bytes_sent;
  body_producer_watcher_.ArmOrNotify();
}

void MimeSniffingURLLoader::ForwardBodyToClient() {
  DCHECK_EQ(0u, bytes_remaining_in_buffer_);
  // Send the body from the consumer to the producer.
  const void* buffer;
  uint32_t buffer_size = 0;
  MojoResult result = body_consumer_handle_->BeginReadData(
      &buffer, &buffer_size, MOJO_BEGIN_READ_DATA_FLAG_NONE);
  switch (result) {
    case MOJO_RESULT_OK:
      break;
    case MOJO_RESULT_SHOULD_WAIT:
      body_consumer_watcher_.ArmOrNotify();
      return;
    case MOJO_RESULT_FAILED_PRECONDITION:
      // All data has been sent.
      CompleteSending();
      return;
    default:
      NOTREACHED();
      return;
  }

  result = body_producer_handle_->WriteData(buffer, &buffer_size,
                                            MOJO_WRITE_DATA_FLAG_NONE);
  switch (result) {
    case MOJO_RESULT_OK:
      break;
    case MOJO_RESULT_FAILED_PRECONDITION:
      // The pipe is closed unexpectedly. |this| should be deleted once
      // URLLoaderPtr on the destination is released.
      Abort();
      return;
    case MOJO_RESULT_SHOULD_WAIT:
      body_consumer_handle_->EndReadData(0);
      body_producer_watcher_.ArmOrNotify();
      return;
    default:
      NOTREACHED();
      return;
  }

  body_consumer_handle_->EndReadData(buffer_size);
  body_consumer_watcher_.ArmOrNotify();
}

void MimeSniffingURLLoader::Abort() {
  state_ = State::kAborted;
  body_consumer_watcher_.Cancel();
  body_producer_watcher_.Cancel();
  source_url_loader_.reset();
  source_url_client_binding_.Close();
  destination_url_loader_client_.reset();
  // |this| should be removed since the owner will destroy |this| or the owner
  // has already been destroyed by some reason.
}

}  // namespace content
