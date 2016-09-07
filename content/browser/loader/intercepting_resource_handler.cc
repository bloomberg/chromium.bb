// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/intercepting_resource_handler.h"

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "content/public/common/resource_response.h"
#include "net/base/io_buffer.h"

namespace content {

InterceptingResourceHandler::InterceptingResourceHandler(
    std::unique_ptr<ResourceHandler> next_handler,
    net::URLRequest* request)
    : LayeredResourceHandler(request, std::move(next_handler)),
      state_(State::STARTING),
      first_read_buffer_size_(0) {}

InterceptingResourceHandler::~InterceptingResourceHandler() {}

bool InterceptingResourceHandler::OnResponseStarted(ResourceResponse* response,
                                                    bool* defer) {
  // If there's no need to switch handlers, just start acting as a blind
  // pass-through ResourceHandler.
  if (!new_handler_) {
    state_ = State::DONE;
    first_read_buffer_ = nullptr;
    return next_handler_->OnResponseStarted(response, defer);
  }

  // Otherwise, switch handlers. First, inform the original ResourceHandler
  // that this will be handled entirely by the new ResourceHandler.
  // TODO(clamy): We will probably need to check the return values of these for
  // PlzNavigate.
  bool defer_ignored = false;
  next_handler_->OnResponseStarted(response, &defer_ignored);

  // Although deferring OnResponseStarted is legal, the only downstream handler
  // which does so is CrossSiteResourceHandler. Cross-site transitions should
  // not trigger when switching handlers.
  DCHECK(!defer_ignored);

  // Make a copy of the data in the first read buffer. Despite not having been
  // informed of any data being stored in first_read_buffer_, the
  // MimeSniffingResourceHandler has read the data, it's just holding it back.
  // This data should be passed to the alternate ResourceHandler and not to to
  // the current ResourceHandler.
  // TODO(clamy): see if doing the copy should be moved to the
  // MimeSniffingResourceHandler.
  if (first_read_buffer_) {
    first_read_buffer_copy_ = new net::IOBuffer(first_read_buffer_size_);
    memcpy(first_read_buffer_copy_->data(), first_read_buffer_->data(),
           first_read_buffer_size_);
  }

  // Send the payload to the old handler.
  SendPayloadToOldHandler();
  first_read_buffer_ = nullptr;

  // The original ResourceHandler is now no longer needed, so replace it with
  // the new one, before sending the response to the new one.
  next_handler_ = std::move(new_handler_);

  state_ =
      first_read_buffer_copy_ ? State::WAITING_FOR_BUFFER_COPY : State::DONE;

  return next_handler_->OnResponseStarted(response, defer);
}

bool InterceptingResourceHandler::OnWillRead(scoped_refptr<net::IOBuffer>* buf,
                                             int* buf_size,
                                             int min_size) {
  if (state_ == State::DONE)
    return next_handler_->OnWillRead(buf, buf_size, min_size);

  DCHECK_EQ(State::STARTING, state_);
  DCHECK_EQ(-1, min_size);

  if (!next_handler_->OnWillRead(buf, buf_size, min_size))
    return false;

  first_read_buffer_ = *buf;
  first_read_buffer_size_ = *buf_size;
  return true;
}

bool InterceptingResourceHandler::OnReadCompleted(int bytes_read, bool* defer) {
  DCHECK(bytes_read >= 0);
  if (state_ == State::DONE)
    return next_handler_->OnReadCompleted(bytes_read, defer);

  DCHECK_EQ(State::WAITING_FOR_BUFFER_COPY, state_);
  state_ = State::DONE;

  // Copy the data from the first read to the new ResourceHandler.
  scoped_refptr<net::IOBuffer> buf;
  int buf_len = 0;
  if (!next_handler_->OnWillRead(&buf, &buf_len, bytes_read))
    return false;

  CHECK(buf_len >= bytes_read);
  CHECK_GE(first_read_buffer_size_, static_cast<size_t>(bytes_read));
  memcpy(buf->data(), first_read_buffer_copy_->data(), bytes_read);

  first_read_buffer_copy_ = nullptr;

  // TODO(clamy): Add a unit test to check that the deferral value is properly
  // passed to the caller.
  return next_handler_->OnReadCompleted(bytes_read, defer);
}

void InterceptingResourceHandler::UseNewHandler(
    std::unique_ptr<ResourceHandler> new_handler,
    const std::string& payload_for_old_handler) {
  new_handler_ = std::move(new_handler);
  new_handler_->SetController(controller());
  payload_for_old_handler_ = payload_for_old_handler;
}

void InterceptingResourceHandler::SendPayloadToOldHandler() {
  bool defer_ignored = false;
  if (payload_for_old_handler_.empty()) {
    // If there is no payload, just finalize the request on the old handler.
    net::URLRequestStatus status(net::URLRequestStatus::CANCELED,
                                 net::ERR_ABORTED);
    next_handler_->OnResponseCompleted(status, &defer_ignored);
    DCHECK(!defer_ignored);
    return;
  }

  // Ensure the old ResourceHandler has a buffer that can store the payload.
  scoped_refptr<net::IOBuffer> buf;
  int size = 0;
  if (first_read_buffer_) {
    // The first read buffer can be reused. The data inside it has been copied
    // before calling this function, so it can safely be overriden.
    buf = first_read_buffer_;
    size = first_read_buffer_size_;
  }

  // If there is no first read buffer, ask the old ResourceHandler to create a
  // buffer that can contain payload.
  if (!buf)
    next_handler_->OnWillRead(&buf, &size, -1);

  DCHECK(buf);
  CHECK_GE(size, static_cast<int>(payload_for_old_handler_.length()));
  memcpy(buf->data(), payload_for_old_handler_.c_str(),
         payload_for_old_handler_.length());
  next_handler_->OnReadCompleted(payload_for_old_handler_.length(),
                                 &defer_ignored);
  payload_for_old_handler_ = std::string();
  DCHECK(!defer_ignored);

  // Finalize the request.
  net::URLRequestStatus status(net::URLRequestStatus::SUCCESS, 0);
  next_handler_->OnResponseCompleted(status, &defer_ignored);
  DCHECK(!defer_ignored);
}

}  // namespace content
