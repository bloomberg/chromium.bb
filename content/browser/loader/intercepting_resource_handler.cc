// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/intercepting_resource_handler.h"

#include "base/auto_reset.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/browser/loader/null_resource_controller.h"
#include "content/browser/loader/resource_controller.h"
#include "content/public/common/resource_response.h"
#include "net/base/io_buffer.h"
#include "net/url_request/url_request.h"

namespace content {

class InterceptingResourceHandler::Controller : public ResourceController {
 public:
  explicit Controller(InterceptingResourceHandler* mime_handler)
      : intercepting_handler_(mime_handler) {}

  void Resume() override {
    MarkAsUsed();
    intercepting_handler_->ResumeInternal();
  }

  void Cancel() override {
    MarkAsUsed();
    intercepting_handler_->Cancel();
  }

  void CancelAndIgnore() override {
    MarkAsUsed();
    intercepting_handler_->CancelAndIgnore();
  }

  void CancelWithError(int error_code) override {
    MarkAsUsed();
    intercepting_handler_->CancelWithError(error_code);
  }

 private:
  void MarkAsUsed() {
#if DCHECK_IS_ON()
    DCHECK(!used_);
    used_ = true;
#endif
  }

#if DCHECK_IS_ON()
  bool used_ = false;
#endif
  InterceptingResourceHandler* intercepting_handler_;

  DISALLOW_COPY_AND_ASSIGN(Controller);
};

InterceptingResourceHandler::InterceptingResourceHandler(
    std::unique_ptr<ResourceHandler> next_handler,
    net::URLRequest* request)
    : LayeredResourceHandler(request, std::move(next_handler)),
      weak_ptr_factory_(this) {
}

InterceptingResourceHandler::~InterceptingResourceHandler() {}

void InterceptingResourceHandler::OnResponseStarted(
    ResourceResponse* response,
    std::unique_ptr<ResourceController> controller) {
  // If there's no need to switch handlers, just start acting as a blind
  // pass-through ResourceHandler.
  if (!new_handler_) {
    state_ = State::PASS_THROUGH;
    next_handler_->OnResponseStarted(response, std::move(controller));
    return;
  }

  DCHECK_EQ(state_, State::STARTING);

  // TODO(yhirano): Retaining ownership from a raw pointer is bad.
  response_ = response;

  // Otherwise, switch handlers. First, inform the original ResourceHandler
  // that this will be handled entirely by the new ResourceHandler.
  HoldController(std::move(controller));
  state_ = State::SWAPPING_HANDLERS;

  DoLoop();
}

bool InterceptingResourceHandler::OnWillRead(scoped_refptr<net::IOBuffer>* buf,
                                             int* buf_size) {
  if (state_ == State::PASS_THROUGH)
    return next_handler_->OnWillRead(buf, buf_size);

  DCHECK_EQ(State::STARTING, state_);

  if (!next_handler_->OnWillRead(buf, buf_size))
    return false;

  first_read_buffer_ = *buf;
  first_read_buffer_size_ = *buf_size;
  first_read_buffer_double_ = new net::IOBuffer(static_cast<size_t>(*buf_size));
  *buf = first_read_buffer_double_;
  return true;
}

void InterceptingResourceHandler::OnReadCompleted(
    int bytes_read,
    std::unique_ptr<ResourceController> controller) {
  DCHECK(!has_controller());

  DCHECK_GE(bytes_read, 0);
  if (state_ == State::PASS_THROUGH) {
    if (first_read_buffer_double_) {
      // |first_read_buffer_double_| was allocated and the user wrote data to
      // the buffer, but switching has not been done after all.
      memcpy(first_read_buffer_->data(), first_read_buffer_double_->data(),
             bytes_read);
      first_read_buffer_ = nullptr;
      first_read_buffer_double_ = nullptr;
    }
    next_handler_->OnReadCompleted(bytes_read, std::move(controller));
    return;
  }

  DCHECK_EQ(State::WAITING_FOR_ON_READ_COMPLETED, state_);
  first_read_buffer_bytes_read_ = bytes_read;
  state_ = State::SENDING_BUFFER_TO_NEW_HANDLER;
  HoldController(std::move(controller));
  DoLoop();
}

void InterceptingResourceHandler::OnResponseCompleted(
    const net::URLRequestStatus& status,
    std::unique_ptr<ResourceController> controller) {
  if (state_ == State::PASS_THROUGH) {
    LayeredResourceHandler::OnResponseCompleted(status, std::move(controller));
    return;
  }
  if (!new_handler_) {
    // Therer is only one ResourceHandler in this InterceptingResourceHandler.
    state_ = State::PASS_THROUGH;
    first_read_buffer_double_ = nullptr;
    next_handler_->OnResponseCompleted(status, std::move(controller));
    return;
  }

  // There are two ResourceHandlers in this InterceptingResourceHandler.
  // |next_handler_| is the old handler and |new_handler_| is the new handler.
  // As written in the class comment, this class assumes that the old handler
  // will immediately call Resume() in OnResponseCompleted.
  bool was_resumed = false;
  // TODO(mmenke): Get rid of NullResourceController and do something more
  // reasonable.
  next_handler_->OnResponseCompleted(
      status, base::MakeUnique<NullResourceController>(&was_resumed));
  DCHECK(was_resumed);

  state_ = State::PASS_THROUGH;
  first_read_buffer_double_ = nullptr;
  next_handler_ = std::move(new_handler_);
  next_handler_->OnResponseCompleted(status, std::move(controller));
}

void InterceptingResourceHandler::UseNewHandler(
    std::unique_ptr<ResourceHandler> new_handler,
    const std::string& payload_for_old_handler) {
  new_handler_ = std::move(new_handler);
  new_handler_->SetDelegate(delegate());
  payload_for_old_handler_ = payload_for_old_handler;
}

void InterceptingResourceHandler::DoLoop() {
  DCHECK(!in_do_loop_);
  DCHECK(!advance_to_next_state_);

  base::AutoReset<bool> auto_in_do_loop(&in_do_loop_, true);
  advance_to_next_state_ = true;

  while (advance_to_next_state_) {
    advance_to_next_state_ = false;

    switch (state_) {
      case State::STARTING:
      case State::WAITING_FOR_ON_READ_COMPLETED:
      case State::PASS_THROUGH:
        NOTREACHED();
        break;
      case State::SENDING_ON_WILL_START_TO_NEW_HANDLER:
        SendOnResponseStartedToNewHandler();
        break;
      case State::SENDING_ON_RESPONSE_STARTED_TO_NEW_HANDLER:
        if (first_read_buffer_double_) {
          // OnWillRead has been called, so copying the data from
          // |first_read_buffer_double_| to |first_read_buffer_| will be needed
          // when OnReadCompleted is called.
          state_ = State::WAITING_FOR_ON_READ_COMPLETED;
        } else {
          // OnWillRead has not been called, so no special handling will be
          // needed from now on.
          state_ = State::PASS_THROUGH;
        }
        ResumeInternal();
        break;
      case State::SWAPPING_HANDLERS:
        SendOnResponseStartedToOldHandler();
        break;
      case State::SENDING_PAYLOAD_TO_OLD_HANDLER:
        SendPayloadToOldHandler();
        break;
      case State::SENDING_BUFFER_TO_NEW_HANDLER:
        SendFirstReadBufferToNewHandler();
        break;
    }
  }
}

void InterceptingResourceHandler::ResumeInternal() {
  DCHECK(has_controller());
  if (state_ == State::STARTING ||
      state_ == State::WAITING_FOR_ON_READ_COMPLETED ||
      state_ == State::PASS_THROUGH) {
    // Uninteresting Resume: just delegate to the original resource controller.
    Resume();
    return;
  }

  // If called recusively from a DoLoop, advance state when returning to the
  // loop.
  if (in_do_loop_) {
    DCHECK(!advance_to_next_state_);
    advance_to_next_state_ = true;
    return;
  }

  // Can't call DoLoop synchronously, as it may call into |next_handler_|
  // synchronously, which is what called Resume().
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&InterceptingResourceHandler::DoLoop,
                            weak_ptr_factory_.GetWeakPtr()));
}

void InterceptingResourceHandler::SendOnResponseStartedToOldHandler() {
  state_ = State::SENDING_PAYLOAD_TO_OLD_HANDLER;
  next_handler_->OnResponseStarted(response_.get(),
                                   base::MakeUnique<Controller>(this));
}

void InterceptingResourceHandler::SendPayloadToOldHandler() {
  DCHECK_EQ(State::SENDING_PAYLOAD_TO_OLD_HANDLER, state_);
  if (payload_bytes_written_ == payload_for_old_handler_.size()) {
    net::URLRequestStatus status(net::URLRequestStatus::SUCCESS, 0);
    if (payload_for_old_handler_.empty()) {
      // If there is no payload, just finalize the request on the old handler.
      status = net::URLRequestStatus::FromError(net::ERR_ABORTED);
    }
    bool was_resumed = false;
    // TODO(mmenke): Get rid of NullResourceController and do something more
    // reasonable.
    next_handler_->OnResponseCompleted(
        status, base::MakeUnique<NullResourceController>(&was_resumed));
    DCHECK(was_resumed);

    next_handler_ = std::move(new_handler_);
    state_ = State::SENDING_ON_WILL_START_TO_NEW_HANDLER;
    next_handler_->OnWillStart(request()->url(),
                               base::MakeUnique<Controller>(this));
    return;
  }

  scoped_refptr<net::IOBuffer> buffer;
  int size = 0;
  if (first_read_buffer_) {
    // |first_read_buffer_| is a buffer gotten from |next_handler_| via
    // OnWillRead. Use the buffer.
    buffer = first_read_buffer_;
    size = first_read_buffer_size_;

    first_read_buffer_ = nullptr;
    first_read_buffer_size_ = 0;
  } else {
    if (!next_handler_->OnWillRead(&buffer, &size)) {
      Cancel();
      return;
    }
  }

  size = std::min(size, static_cast<int>(payload_for_old_handler_.size() -
                                         payload_bytes_written_));
  memcpy(buffer->data(),
         payload_for_old_handler_.data() + payload_bytes_written_, size);
  payload_bytes_written_ += size;
  next_handler_->OnReadCompleted(size, base::MakeUnique<Controller>(this));
}

void InterceptingResourceHandler::SendOnResponseStartedToNewHandler() {
  state_ = State::SENDING_ON_RESPONSE_STARTED_TO_NEW_HANDLER;
  next_handler_->OnResponseStarted(response_.get(),
                                   base::MakeUnique<Controller>(this));
}

void InterceptingResourceHandler::SendFirstReadBufferToNewHandler() {
  DCHECK_EQ(state_, State::SENDING_BUFFER_TO_NEW_HANDLER);

  if (first_read_buffer_bytes_written_ == first_read_buffer_bytes_read_) {
    state_ = State::PASS_THROUGH;
    first_read_buffer_double_ = nullptr;
    ResumeInternal();
    return;
  }

  scoped_refptr<net::IOBuffer> buf;
  int size = 0;
  if (!next_handler_->OnWillRead(&buf, &size)) {
    Cancel();
    return;
  }
  size = std::min(size, static_cast<int>(first_read_buffer_bytes_read_ -
                                         first_read_buffer_bytes_written_));
  memcpy(buf->data(),
         first_read_buffer_double_->data() + first_read_buffer_bytes_written_,
         size);
  first_read_buffer_bytes_written_ += size;
  next_handler_->OnReadCompleted(size, base::MakeUnique<Controller>(this));
}

}  // namespace content
