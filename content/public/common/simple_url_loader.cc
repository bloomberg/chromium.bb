// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/simple_url_loader.h"

#include <stdint.h>

#include <algorithm>
#include <limits>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "content/public/common/resource_request.h"
#include "content/public/common/resource_response.h"
#include "content/public/common/url_loader.mojom.h"
#include "content/public/common/url_loader_factory.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/simple_watcher.h"
#include "net/base/net_errors.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace content {

namespace {

class SimpleURLLoaderImpl;

// Class to drive the pipe for reading the body, handle the results of the body
// read as appropriate, and invoke the consumer's callback to notify it of
// request completion.
class BodyHandler {
 public:
  // A raw pointer is safe, since |simple_url_loader| owns the BodyHandler.
  explicit BodyHandler(SimpleURLLoaderImpl* simple_url_loader)
      : simple_url_loader_(simple_url_loader) {}
  virtual ~BodyHandler() {}

  // Called with the data pipe received from the URLLoader. The BodyHandler is
  // responsible for reading from it and monitoring it for closure. Should call
  // either SimpleURLLoaderImpl::OnBodyPipeClosed() when the body pipe is
  // closed, or SimpleURLLoaderImpl::FinishWithResult() with an error code if
  // some sort of unexpected error occurs, like a write to a file fails. Must
  // not call both.
  virtual void OnStartLoadingResponseBody(
      mojo::ScopedDataPipeConsumerHandle body_data_pipe) = 0;

  // Notifies the consumer that the request has completed, either successfully
  // or with an error. May be invoked before OnStartLoadingResponseBody(), or
  // before the BodyHandler has notified SimplerURLloader of completion or an
  // error. Once this is called, must not invoke any of SimpleURLLoaderImpl's
  // callbacks.
  virtual void NotifyConsumerOfCompletion() = 0;

 protected:
  SimpleURLLoaderImpl* simple_url_loader() { return simple_url_loader_; }

 private:
  SimpleURLLoaderImpl* const simple_url_loader_;

  DISALLOW_COPY_AND_ASSIGN(BodyHandler);
};

// BodyHandler implementation for consuming the response as a string.
class SaveToStringBodyHandler : public BodyHandler {
 public:
  SaveToStringBodyHandler(
      SimpleURLLoaderImpl* simple_url_loader,
      SimpleURLLoader::BodyAsStringCallback body_as_string_callback,
      size_t max_body_size)
      : BodyHandler(simple_url_loader),
        max_body_size_(max_body_size),
        body_as_string_callback_(std::move(body_as_string_callback)),
        handle_watcher_(FROM_HERE, mojo::SimpleWatcher::ArmingPolicy::MANUAL) {}

  ~SaveToStringBodyHandler() override {}

  // BodyHandler implementation:
  void OnStartLoadingResponseBody(
      mojo::ScopedDataPipeConsumerHandle body_data_pipe) override;
  void NotifyConsumerOfCompletion() override;

 private:
  void MojoReadyCallback(MojoResult result,
                         const mojo::HandleSignalsState& state);

  // Reads as much data as possible from |body_data_pipe_|, copying it to
  // |body_|. Arms |handle_watcher_| when data is not currently available.
  void ReadData();

  // Frees Mojo resources and prevents any more Mojo messages from arriving.
  void ClosePipe();

  const size_t max_body_size_;

  SimpleURLLoader::BodyAsStringCallback body_as_string_callback_;

  std::unique_ptr<std::string> body_;

  mojo::ScopedDataPipeConsumerHandle body_data_pipe_;
  mojo::SimpleWatcher handle_watcher_;

  DISALLOW_COPY_AND_ASSIGN(SaveToStringBodyHandler);
};

class SimpleURLLoaderImpl : public SimpleURLLoader,
                            public mojom::URLLoaderClient {
 public:
  SimpleURLLoaderImpl();
  ~SimpleURLLoaderImpl() override;

  // SimpleURLLoader implementation.
  void DownloadToString(const ResourceRequest& resource_request,
                        mojom::URLLoaderFactory* url_loader_factory,
                        const net::NetworkTrafficAnnotationTag& annotation_tag,
                        BodyAsStringCallback body_as_string_callback,
                        size_t max_body_size) override;
  void DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      const ResourceRequest& resource_request,
      mojom::URLLoaderFactory* url_loader_factory,
      const net::NetworkTrafficAnnotationTag& annotation_tag,
      BodyAsStringCallback body_as_string_callback) override;
  void SetAllowPartialResults(bool allow_partial_results) override;
  void SetAllowHttpErrorResults(bool allow_http_error_results) override;
  int NetError() const override;
  const ResourceResponseHead* ResponseInfo() const override;

  bool allow_partial_results() const { return allow_partial_results_; }

  // Called by BodyHandler when the body pipe was closed. This could indicate an
  // error, concellation, or completion. To determine which case this is, the
  // size will also be compared to the size reported in
  // ResourceRequestCompletionStatus(), if ResourceRequestCompletionStatus
  // indicates a success.
  void OnBodyPipeClosed(int64_t received_body_size);

  // Finished the request with the provided error code, after freeing Mojo
  // resources. Closes any open pipes, so no URLLoader or BodyHandlers callbacks
  // will be invoked after this is called.
  void FinishWithResult(int net_error);

 private:
  void StartInternal(const ResourceRequest& resource_request,
                     mojom::URLLoaderFactory* url_loader_factory,
                     const net::NetworkTrafficAnnotationTag& annotation_tag);

  // mojom::URLLoaderClient implementation;
  void OnReceiveResponse(const ResourceResponseHead& response_head,
                         const base::Optional<net::SSLInfo>& ssl_info,
                         mojom::DownloadedTempFilePtr downloaded_file) override;
  void OnReceiveRedirect(const net::RedirectInfo& redirect_info,
                         const ResourceResponseHead& response_head) override;
  void OnDataDownloaded(int64_t data_length, int64_t encoded_length) override;
  void OnReceiveCachedMetadata(const std::vector<uint8_t>& data) override;
  void OnTransferSizeUpdated(int32_t transfer_size_diff) override;
  void OnUploadProgress(int64_t current_position,
                        int64_t total_size,
                        OnUploadProgressCallback ack_callback) override;
  void OnStartLoadingResponseBody(
      mojo::ScopedDataPipeConsumerHandle body) override;
  void OnComplete(const ResourceRequestCompletionStatus& status) override;

  // Bound to the URLLoaderClient message pipe (|client_binding_|) via
  // set_connection_error_handler.
  void OnConnectionError();

  // Completes the request by calling FinishWithResult() if OnComplete() was
  // called and either no body pipe was ever received, or the body pipe was
  // closed.
  void MaybeComplete();

  bool allow_partial_results_ = false;
  bool allow_http_error_results_ = false;

  mojom::URLLoaderPtr url_loader_;
  mojo::Binding<mojom::URLLoaderClient> client_binding_;
  std::unique_ptr<BodyHandler> body_handler_;

  bool request_completed_ = false;
  // The expected total size of the body, taken from
  // ResourceRequestCompletionStatus.
  int64_t expected_body_size_ = 0;

  bool body_started_ = false;
  bool body_completed_ = false;
  // Final size of the body. Set once the body's Mojo pipe has been closed.
  int64_t received_body_size_ = 0;

  // Set to true when FinishWithResult() is called. Once that happens, the
  // consumer is informed of completion, and both pipes are closed.
  bool finished_ = false;

  // Result of the request.
  int net_error_ = net::ERR_IO_PENDING;

  std::unique_ptr<ResourceResponseHead> response_info_;

  DISALLOW_COPY_AND_ASSIGN(SimpleURLLoaderImpl);
};

void SaveToStringBodyHandler::OnStartLoadingResponseBody(
    mojo::ScopedDataPipeConsumerHandle body_data_pipe) {
  body_data_pipe_ = std::move(body_data_pipe);
  body_ = base::MakeUnique<std::string>();
  handle_watcher_.Watch(
      body_data_pipe_.get(),
      MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
      MOJO_WATCH_CONDITION_SATISFIED,
      base::BindRepeating(&SaveToStringBodyHandler::MojoReadyCallback,
                          base::Unretained(this)));
  ReadData();
}

void SaveToStringBodyHandler::NotifyConsumerOfCompletion() {
  ClosePipe();

  if (simple_url_loader()->NetError() != net::OK &&
      !simple_url_loader()->allow_partial_results()) {
    // If it's a partial download or an error was received, erase the body.
    body_.reset();
  }

  std::move(body_as_string_callback_).Run(std::move(body_));
}

void SaveToStringBodyHandler::MojoReadyCallback(
    MojoResult result,
    const mojo::HandleSignalsState& state) {
  ReadData();
}

void SaveToStringBodyHandler::ReadData() {
  while (true) {
    const void* body_data;
    uint32_t read_size;
    MojoResult result = body_data_pipe_->BeginReadData(
        &body_data, &read_size, MOJO_READ_DATA_FLAG_NONE);
    if (result == MOJO_RESULT_SHOULD_WAIT) {
      handle_watcher_.ArmOrNotify();
      return;
    }

    // If the pipe was closed, unclear if it was an error or success. Notify
    // the SimpleURLLoaderImpl of how much data was received.
    if (result != MOJO_RESULT_OK) {
      // The only error other than MOJO_RESULT_SHOULD_WAIT this should fail
      // with is MOJO_RESULT_FAILED_PRECONDITION, in the case the pipe was
      // closed.
      DCHECK_EQ(MOJO_RESULT_FAILED_PRECONDITION, result);
      ClosePipe();
      simple_url_loader()->OnBodyPipeClosed(body_->size());
      return;
    }

    // Check size against the limit.
    size_t copy_size = std::min(max_body_size_ - body_->size(),
                                static_cast<size_t>(read_size));
    body_->append(static_cast<const char*>(body_data), copy_size);
    body_data_pipe_->EndReadData(copy_size);

    // Fail the request if the size limit was exceeded.
    if (copy_size < read_size) {
      ClosePipe();
      simple_url_loader()->FinishWithResult(net::ERR_INSUFFICIENT_RESOURCES);
      return;
    }
  }
}

void SaveToStringBodyHandler::ClosePipe() {
  handle_watcher_.Cancel();
  body_data_pipe_.reset();
}

SimpleURLLoaderImpl::SimpleURLLoaderImpl() : client_binding_(this) {}

SimpleURLLoaderImpl::~SimpleURLLoaderImpl() {}

void SimpleURLLoaderImpl::DownloadToString(
    const ResourceRequest& resource_request,
    mojom::URLLoaderFactory* url_loader_factory,
    const net::NetworkTrafficAnnotationTag& annotation_tag,
    BodyAsStringCallback body_as_string_callback,
    size_t max_body_size) {
  DCHECK_LE(max_body_size, kMaxBoundedStringDownloadSize);
  body_handler_ = base::MakeUnique<SaveToStringBodyHandler>(
      this, std::move(body_as_string_callback), max_body_size);
  StartInternal(resource_request, url_loader_factory, annotation_tag);
}

void SimpleURLLoaderImpl::DownloadToStringOfUnboundedSizeUntilCrashAndDie(
    const ResourceRequest& resource_request,
    mojom::URLLoaderFactory* url_loader_factory,
    const net::NetworkTrafficAnnotationTag& annotation_tag,
    BodyAsStringCallback body_as_string_callback) {
  body_handler_ = base::MakeUnique<SaveToStringBodyHandler>(
      this, std::move(body_as_string_callback),
      // int64_t because ResourceRequestCompletionStatus::decoded_body_length is
      // an int64_t, not a size_t.
      std::numeric_limits<int64_t>::max());
  StartInternal(resource_request, url_loader_factory, annotation_tag);
}

void SimpleURLLoaderImpl::SetAllowPartialResults(bool allow_partial_results) {
  // Simplest way to check if a request has not yet been started.
  DCHECK(!body_handler_);
  allow_partial_results_ = allow_partial_results;
}

void SimpleURLLoaderImpl::SetAllowHttpErrorResults(
    bool allow_http_error_results) {
  // Simplest way to check if a request has not yet been started.
  DCHECK(!body_handler_);
  allow_http_error_results_ = allow_http_error_results;
}

int SimpleURLLoaderImpl::NetError() const {
  // Should only be called once the request is compelete.
  DCHECK(finished_);
  DCHECK_NE(net::ERR_IO_PENDING, net_error_);
  return net_error_;
}

const ResourceResponseHead* SimpleURLLoaderImpl::ResponseInfo() const {
  // Should only be called once the request is compelete.
  DCHECK(finished_);
  return response_info_.get();
}

void SimpleURLLoaderImpl::OnBodyPipeClosed(int64_t received_body_size) {
  DCHECK(body_started_);
  DCHECK(!body_completed_);

  body_completed_ = true;
  received_body_size_ = received_body_size;
  MaybeComplete();
}

void SimpleURLLoaderImpl::FinishWithResult(int net_error) {
  DCHECK(!finished_);

  client_binding_.Close();
  url_loader_.reset();

  finished_ = true;
  net_error_ = net_error;
  body_handler_->NotifyConsumerOfCompletion();
}

void SimpleURLLoaderImpl::StartInternal(
    const ResourceRequest& resource_request,
    mojom::URLLoaderFactory* url_loader_factory,
    const net::NetworkTrafficAnnotationTag& annotation_tag) {
  // It's illegal to use a single SimpleURLLoaderImpl to make multiple requests.
  DCHECK(!finished_);
  DCHECK(!url_loader_);
  DCHECK(!body_started_);

  mojom::URLLoaderClientPtr client_ptr;
  client_binding_.Bind(mojo::MakeRequest(&client_ptr));
  client_binding_.set_connection_error_handler(base::BindOnce(
      &SimpleURLLoaderImpl::OnConnectionError, base::Unretained(this)));
  url_loader_factory->CreateLoaderAndStart(
      mojo::MakeRequest(&url_loader_), 0 /* routing_id */, 0 /* request_id */,
      0 /* options */, resource_request, std::move(client_ptr),
      net::MutableNetworkTrafficAnnotationTag(annotation_tag));
}

void SimpleURLLoaderImpl::OnReceiveResponse(
    const ResourceResponseHead& response_head,
    const base::Optional<net::SSLInfo>& ssl_info,
    mojom::DownloadedTempFilePtr downloaded_file) {
  if (response_info_) {
    // The final headers have already been received, so the URLLoader is
    // violating the API contract.
    FinishWithResult(net::ERR_UNEXPECTED);
    return;
  }

  response_info_ = base::MakeUnique<ResourceResponseHead>(response_head);
  if (!allow_http_error_results_ && response_head.headers &&
      response_head.headers->response_code() / 100 != 2) {
    FinishWithResult(net::ERR_FAILED);
  }
}

void SimpleURLLoaderImpl::OnReceiveRedirect(
    const net::RedirectInfo& redirect_info,
    const ResourceResponseHead& response_head) {
  if (response_info_) {
    // If the headers have already been received, the URLLoader is violating the
    // API contract.
    FinishWithResult(net::ERR_UNEXPECTED);
    return;
  }
  url_loader_->FollowRedirect();
}

void SimpleURLLoaderImpl::OnDataDownloaded(int64_t data_length,
                                           int64_t encoded_length) {
  NOTIMPLEMENTED();
}

void SimpleURLLoaderImpl::OnReceiveCachedMetadata(
    const std::vector<uint8_t>& data) {
  NOTREACHED();
}

void SimpleURLLoaderImpl::OnTransferSizeUpdated(int32_t transfer_size_diff) {}

void SimpleURLLoaderImpl::OnUploadProgress(
    int64_t current_position,
    int64_t total_size,
    OnUploadProgressCallback ack_callback) {}

void SimpleURLLoaderImpl::OnStartLoadingResponseBody(
    mojo::ScopedDataPipeConsumerHandle body) {
  if (body_started_ || !response_info_) {
    // If this was already called, or the headers have not yet been received,
    // the URLLoader is violating the API contract.
    FinishWithResult(net::ERR_UNEXPECTED);
    return;
  }
  body_started_ = true;
  body_handler_->OnStartLoadingResponseBody(std::move(body));
}

void SimpleURLLoaderImpl::OnComplete(
    const ResourceRequestCompletionStatus& status) {
  // Request should not have been completed yet.
  DCHECK(!finished_);
  DCHECK(!request_completed_);

  // Close pipes to ignore any subsequent close notification.
  client_binding_.Close();
  url_loader_.reset();

  request_completed_ = true;
  expected_body_size_ = status.decoded_body_length;
  net_error_ = status.error_code;
  // If |status| indicates success, but the body pipe was never received, the
  // URLLoader is violating the API contract.
  if (net_error_ == net::OK && !body_started_)
    net_error_ = net::ERR_UNEXPECTED;

  MaybeComplete();
}

void SimpleURLLoaderImpl::OnConnectionError() {
  // |this| closes the pipe to the URLLoader in OnComplete(), so this method
  // being called indicates the pipe was closed before completion, most likely
  // due to peer death, or peer not calling OnComplete() on cancellation.

  // Request should not have been completed yet.
  DCHECK(!finished_);
  DCHECK(!request_completed_);
  DCHECK_EQ(net::ERR_IO_PENDING, net_error_);

  request_completed_ = true;
  net_error_ = net::ERR_FAILED;
  // Wait to receive any pending data on the data pipe before reporting the
  // failure.
  MaybeComplete();
}

void SimpleURLLoaderImpl::MaybeComplete() {
  // Request should not have completed yet.
  DCHECK(!finished_);

  // Make sure the URLLoader's pipe has been closed.
  if (!request_completed_)
    return;

  // Make sure the body pipe either was never opened or has been closed. Even if
  // the request failed, if allow_partial_results_ is true, may still be able to
  // read more data.
  if (body_started_ && !body_completed_)
    return;

  // When OnCompleted sees a success result, still need to report an error if
  // the size isn't what was expected.
  if (net_error_ == net::OK && expected_body_size_ != received_body_size_) {
    if (expected_body_size_ > received_body_size_) {
      // The body pipe was closed before it received the entire body.
      net_error_ = net::ERR_FAILED;
    } else {
      // The caller provided more data through the pipe than it reported in
      // ResourceRequestCompletionStatus, so the URLLoader is violating the API
      // contract. Just fail the request.
      net_error_ = net::ERR_UNEXPECTED;
    }
  }

  FinishWithResult(net_error_);
}

}  // namespace

std::unique_ptr<SimpleURLLoader> SimpleURLLoader::Create() {
  return base::MakeUnique<SimpleURLLoaderImpl>();
}

SimpleURLLoader::~SimpleURLLoader() {}

SimpleURLLoader::SimpleURLLoader() {}

}  // namespace content
