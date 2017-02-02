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

class MockResourceLoader::TestResourceController : public ResourceController {
 public:
  explicit TestResourceController(base::WeakPtr<MockResourceLoader> mock_loader)
      : mock_loader_(mock_loader) {}
  ~TestResourceController() override {}

  void Resume() override { mock_loader_->OnResume(); }

  void Cancel() override { CancelWithError(net::ERR_ABORTED); }

  void CancelAndIgnore() override {
    ADD_FAILURE() << "Unexpected CancelAndIgnore call.";
    Cancel();
  }

  void CancelWithError(int error_code) override {
    mock_loader_->OnCancel(error_code);
  }

  base::WeakPtr<MockResourceLoader> mock_loader_;
};

MockResourceLoader::MockResourceLoader(ResourceHandler* resource_handler)
    : resource_handler_(resource_handler), weak_factory_(this) {
  resource_handler_->SetDelegate(this);
}

MockResourceLoader::~MockResourceLoader() {}

MockResourceLoader::Status MockResourceLoader::OnWillStart(const GURL& url) {
  EXPECT_FALSE(weak_factory_.HasWeakPtrs());
  EXPECT_EQ(Status::IDLE, status_);

  status_ = Status::CALLING_HANDLER;
  resource_handler_->OnWillStart(url, base::MakeUnique<TestResourceController>(
                                          weak_factory_.GetWeakPtr()));
  if (status_ == Status::CALLING_HANDLER)
    status_ = Status::CALLBACK_PENDING;
  return status_;
}

MockResourceLoader::Status MockResourceLoader::OnRequestRedirected(
    const net::RedirectInfo& redirect_info,
    scoped_refptr<ResourceResponse> response) {
  EXPECT_FALSE(weak_factory_.HasWeakPtrs());
  EXPECT_EQ(Status::IDLE, status_);

  status_ = Status::CALLING_HANDLER;
  // Note that |this| does not hold onto |response|, to match ResourceLoader's
  // behavior. If |resource_handler_| wants to use |response| asynchronously, it
  // needs to hold onto its own pointer to it.
  resource_handler_->OnRequestRedirected(
      redirect_info, response.get(),
      base::MakeUnique<TestResourceController>(weak_factory_.GetWeakPtr()));
  if (status_ == Status::CALLING_HANDLER)
    status_ = Status::CALLBACK_PENDING;
  return status_;
}

MockResourceLoader::Status MockResourceLoader::OnResponseStarted(
    scoped_refptr<ResourceResponse> response) {
  EXPECT_FALSE(weak_factory_.HasWeakPtrs());
  EXPECT_EQ(Status::IDLE, status_);

  status_ = Status::CALLING_HANDLER;
  // Note that |this| does not hold onto |response|, to match ResourceLoader's
  // behavior. If |resource_handler_| wants to use |response| asynchronously, it
  // needs to hold onto its own pointer to it.
  resource_handler_->OnResponseStarted(
      response.get(),
      base::MakeUnique<TestResourceController>(weak_factory_.GetWeakPtr()));
  if (status_ == Status::CALLING_HANDLER)
    status_ = Status::CALLBACK_PENDING;
  return status_;
}

MockResourceLoader::Status MockResourceLoader::OnWillRead() {
  EXPECT_FALSE(weak_factory_.HasWeakPtrs());
  EXPECT_EQ(Status::IDLE, status_);

  status_ = Status::CALLING_HANDLER;
  bool result =
      resource_handler_->OnWillRead(&io_buffer_, &io_buffer_size_);

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
    EXPECT_LT(0, io_buffer_size_);
    EXPECT_TRUE(io_buffer_);
    status_ = Status::IDLE;
  }
  return status_;
};

MockResourceLoader::Status MockResourceLoader::OnReadCompleted(
    base::StringPiece bytes) {
  EXPECT_FALSE(weak_factory_.HasWeakPtrs());
  EXPECT_EQ(Status::IDLE, status_);
  EXPECT_LE(bytes.size(), static_cast<size_t>(io_buffer_size_));

  status_ = Status::CALLING_HANDLER;
  std::copy(bytes.begin(), bytes.end(), io_buffer_->data());
  io_buffer_ = nullptr;
  io_buffer_size_ = 0;
  resource_handler_->OnReadCompleted(
      bytes.size(),
      base::MakeUnique<TestResourceController>(weak_factory_.GetWeakPtr()));
  if (status_ == Status::CALLING_HANDLER)
    status_ = Status::CALLBACK_PENDING;
  return status_;
}

MockResourceLoader::Status MockResourceLoader::OnResponseCompleted(
    const net::URLRequestStatus& status) {
  EXPECT_FALSE(weak_factory_.HasWeakPtrs());
  // This should only happen while the ResourceLoader is idle or the request was
  // canceled.
  EXPECT_TRUE(status_ == Status::IDLE ||
              (!status.is_success() && status_ == Status::CANCELED &&
               error_code_ == status.error()));

  io_buffer_ = nullptr;
  io_buffer_size_ = 0;
  status_ = Status::CALLING_HANDLER;
  resource_handler_->OnResponseCompleted(
      status,
      base::MakeUnique<TestResourceController>(weak_factory_.GetWeakPtr()));
  if (status_ == Status::CALLING_HANDLER)
    status_ = Status::CALLBACK_PENDING;
  EXPECT_NE(Status::CANCELED, status_);
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

  resource_handler_->OnResponseCompleted(
      url_request_status,
      base::MakeUnique<TestResourceController>(weak_factory_.GetWeakPtr()));
  if (status_ == Status::CALLING_HANDLER)
    status_ = Status::CALLBACK_PENDING;
  EXPECT_NE(Status::CANCELED, status_);
  return status_;
}

void MockResourceLoader::WaitUntilIdleOrCanceled() {
  if (status_ == Status::IDLE || status_ == Status::CANCELED)
    return;
  EXPECT_FALSE(canceled_or_idle_run_loop_);
  canceled_or_idle_run_loop_.reset(new base::RunLoop());
  canceled_or_idle_run_loop_->Run();
  canceled_or_idle_run_loop_.reset();
  EXPECT_TRUE(status_ == Status::IDLE || status_ == Status::CANCELED);
}

void MockResourceLoader::OutOfBandCancel(int error_code, bool tell_renderer) {
  // Shouldn't be called in-band.
  EXPECT_NE(Status::CALLING_HANDLER, status_);

  status_ = Status::CANCELED;
  canceled_out_of_band_ = true;

  // To mimic real behavior, keep old error, in the case of double-cancel.
  if (error_code_ == net::OK)
    error_code_ = error_code;
}

void MockResourceLoader::OnCancel(int error_code) {
  // It's currently allowed to be canceled in-band after being cancelled
  // out-of-band, so do nothing, unless the status is no longer CANCELED, which
  // which case, OnResponseCompleted has already been called, and cancels aren't
  // expected then.
  // TODO(mmenke):  Make CancelOutOfBand synchronously destroy the
  // ResourceLoader.
  if (canceled_out_of_band_ && status_ == Status::CANCELED)
    return;

  EXPECT_LT(error_code, 0);
  EXPECT_TRUE(status_ == Status::CALLBACK_PENDING ||
              status_ == Status::CALLING_HANDLER);

  status_ = Status::CANCELED;
  error_code_ = error_code;
  if (canceled_or_idle_run_loop_)
    canceled_or_idle_run_loop_->Quit();
}

void MockResourceLoader::OnResume() {
  EXPECT_TRUE(status_ == Status::CALLBACK_PENDING ||
              status_ == Status::CALLING_HANDLER);

  status_ = Status::IDLE;
  if (canceled_or_idle_run_loop_)
    canceled_or_idle_run_loop_->Quit();
}

}  // namespace content
