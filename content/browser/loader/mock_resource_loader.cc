// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/mock_resource_loader.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "content/browser/loader/resource_controller.h"
#include "content/browser/loader/resource_handler.h"
#include "content/public/common/resource_response.h"
#include "net/base/io_buffer.h"
#include "net/url_request/url_request_status.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

MockResourceLoader::MockResourceLoader(ResourceHandler* resource_handler)
    : resource_handler_(resource_handler) {
  resource_handler_->SetController(this);
}

MockResourceLoader::~MockResourceLoader() {}

MockResourceLoader::Status MockResourceLoader::OnWillStart(const GURL& url) {
  EXPECT_EQ(Status::IDLE, status_);
  status_ = Status::CALLING_HANDLER;

  bool defer = false;
  bool result = resource_handler_->OnWillStart(url, &defer);
  // The second case isn't really allowed, but a number of classes do it
  // anyways.
  EXPECT_TRUE(status_ == Status::CALLING_HANDLER ||
              (result == false && status_ == Status::CANCELED));
  if (!result) {
    // In the case of double-cancels, keep the old error code.
    // TODO(mmenke):  Once there are no more double cancel cases, remove this
    // code.
    if (status_ != Status::CANCELED)
      error_code_ = net::ERR_ABORTED;
    status_ = Status::CANCELED;
  } else if (defer) {
    status_ = Status::CALLBACK_PENDING;
  } else {
    status_ = Status::IDLE;
  }
  return status_;
}

MockResourceLoader::Status MockResourceLoader::OnRequestRedirected(
    const net::RedirectInfo& redirect_info,
    scoped_refptr<ResourceResponse> response) {
  EXPECT_EQ(Status::IDLE, status_);
  status_ = Status::CALLING_HANDLER;

  bool defer = false;
  // Note that |this| does not hold onto |response|, to match ResourceLoader's
  // behavior. If |resource_handler_| wants to use |response| asynchronously, it
  // needs to hold onto its own pointer to it.
  bool result = resource_handler_->OnRequestRedirected(redirect_info,
                                                       response.get(), &defer);
  // The second case isn't really allowed, but a number of classes do it
  // anyways.
  EXPECT_TRUE(status_ == Status::CALLING_HANDLER ||
              (result == false && status_ == Status::CANCELED));
  if (!result) {
    // In the case of double-cancels, keep the old error code.
    if (status_ != Status::CANCELED)
      error_code_ = net::ERR_ABORTED;
    status_ = Status::CANCELED;
  } else if (defer) {
    status_ = Status::CALLBACK_PENDING;
  } else {
    status_ = Status::IDLE;
  }
  return status_;
}

MockResourceLoader::Status MockResourceLoader::OnResponseStarted(
    scoped_refptr<ResourceResponse> response) {
  EXPECT_EQ(Status::IDLE, status_);
  status_ = Status::CALLING_HANDLER;

  bool defer = false;
  // Note that |this| does not hold onto |response|, to match ResourceLoader's
  // behavior. If |resource_handler_| wants to use |response| asynchronously, it
  // needs to hold onto its own pointer to it.
  bool result = resource_handler_->OnResponseStarted(response.get(), &defer);
  // The second case isn't really allowed, but a number of classes do it
  // anyways.
  EXPECT_TRUE(status_ == Status::CALLING_HANDLER ||
              (result == false && status_ == Status::CANCELED));
  if (!result) {
    // In the case of double-cancels, keep the old error code.
    if (status_ != Status::CANCELED)
      error_code_ = net::ERR_ABORTED;
    status_ = Status::CANCELED;
  } else if (defer) {
    status_ = Status::CALLBACK_PENDING;
  } else {
    status_ = Status::IDLE;
  }
  return status_;
}

MockResourceLoader::Status MockResourceLoader::OnWillRead(int min_size) {
  EXPECT_EQ(Status::IDLE, status_);
  EXPECT_FALSE(io_buffer_);
  EXPECT_EQ(0, io_buffer_size_);

  status_ = Status::CALLING_HANDLER;

  bool result =
      resource_handler_->OnWillRead(&io_buffer_, &io_buffer_size_, min_size);
  // The second case isn't really allowed, but a number of classes do it
  // anyways.
  EXPECT_TRUE(status_ == Status::CALLING_HANDLER ||
              (result == false && status_ == Status::CANCELED));
  if (!result) {
    // In the case of double-cancels, keep the old error code.
    if (status_ != Status::CANCELED)
      error_code_ = net::ERR_ABORTED;
    EXPECT_EQ(0, io_buffer_size_);
    EXPECT_FALSE(io_buffer_);
    status_ = Status::CANCELED;
  } else {
    EXPECT_LE(min_size, io_buffer_size_);
    EXPECT_LT(0, io_buffer_size_);
    EXPECT_TRUE(io_buffer_);
    status_ = Status::IDLE;
  }
  return status_;
};

MockResourceLoader::Status MockResourceLoader::OnReadCompleted(
    base::StringPiece bytes) {
  EXPECT_EQ(Status::IDLE, status_);
  EXPECT_LE(bytes.size(), static_cast<size_t>(io_buffer_size_));

  status_ = Status::CALLING_HANDLER;
  std::copy(bytes.begin(), bytes.end(), io_buffer_->data());
  io_buffer_ = nullptr;
  io_buffer_size_ = 0;
  status_ = Status::CALLING_HANDLER;

  bool defer = false;
  bool result = resource_handler_->OnReadCompleted(bytes.size(), &defer);
  // The second case isn't really allowed, but a number of classes do it
  // anyways.
  EXPECT_TRUE(status_ == Status::CALLING_HANDLER ||
              (result == false && status_ == Status::CANCELED));
  if (!result) {
    // In the case of double-cancels, keep the old error code.
    if (status_ != Status::CANCELED)
      error_code_ = net::ERR_ABORTED;
    status_ = Status::CANCELED;
  } else if (defer) {
    status_ = Status::CALLBACK_PENDING;
  } else {
    status_ = Status::IDLE;
  }
  return status_;
}

MockResourceLoader::Status MockResourceLoader::OnResponseCompleted(
    const net::URLRequestStatus& status) {
  // This should only happen while the ResourceLoader is idle or the request was
  // canceled.
  EXPECT_TRUE(status_ == Status::IDLE ||
              (!status.is_success() && status_ == Status::CANCELED &&
               error_code_ == status.error()));

  io_buffer_ = nullptr;
  io_buffer_size_ = 0;
  status_ = Status::CALLING_HANDLER;

  bool defer = false;
  resource_handler_->OnResponseCompleted(status, &defer);
  EXPECT_EQ(Status::CALLING_HANDLER, status_);
  if (defer) {
    status_ = Status::CALLBACK_PENDING;
  } else {
    status_ = Status::IDLE;
  }
  return status_;
}

MockResourceLoader::Status
MockResourceLoader::OnResponseCompletedFromExternalOutOfBandCancel(
    const net::URLRequestStatus& url_request_status) {
  // This can happen at any point, except from a recursive call from
  // ResourceHandler.
  EXPECT_NE(Status::CALLING_HANDLER, status_);

  io_buffer_ = nullptr;
  io_buffer_size_ = 0;
  status_ = Status::CALLING_HANDLER;

  bool defer = false;
  resource_handler_->OnResponseCompleted(url_request_status, &defer);
  EXPECT_EQ(Status::CALLING_HANDLER, status_);
  if (defer) {
    status_ = Status::CALLBACK_PENDING;
  } else {
    status_ = Status::IDLE;
  }
  return status_;
}

void MockResourceLoader::WaitUntilIdleOrCanceled() {
  if (status_ == Status::IDLE || status_ == Status::CANCELED)
    return;
  EXPECT_FALSE(canceled_or_idle_run_loop_);
  canceled_or_idle_run_loop_.reset(new base::RunLoop());
  canceled_or_idle_run_loop_->Run();
  EXPECT_TRUE(status_ == Status::IDLE || status_ == Status::CANCELED);
}

void MockResourceLoader::Cancel() {
  CancelWithError(net::ERR_ABORTED);
}

void MockResourceLoader::CancelAndIgnore() {
  NOTREACHED();
}

void MockResourceLoader::CancelWithError(int error_code) {
  EXPECT_LT(error_code, 0);
  // Ignore double cancels.  Classes shouldn't be doing this, as
  // ResourceLoader doesn't really support it, but they do anywways.  :(
  // TODO(mmenke): Remove this check.
  if (error_code_ != net::OK)
    return;

  // ResourceLoader really expects canceled not to be called synchronously, but
  // a lot of code does it, so allow it.
  // TODO(mmenke):  Remove CALLING_HANDLER.
  EXPECT_TRUE(status_ == Status::CALLBACK_PENDING ||
              status_ == Status::CALLING_HANDLER || status_ == Status::IDLE);
  status_ = Status::CANCELED;
  error_code_ = error_code;
  if (canceled_or_idle_run_loop_)
    canceled_or_idle_run_loop_->Quit();
}

void MockResourceLoader::Resume() {
  EXPECT_EQ(Status::CALLBACK_PENDING, status_);
  status_ = Status::IDLE;
  if (canceled_or_idle_run_loop_)
    canceled_or_idle_run_loop_->Quit();
}

}  // namespace content
