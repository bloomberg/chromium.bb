// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/fileapi/mock_url_request_delegate.h"

#include "base/profiler/scoped_tracker.h"
#include "base/run_loop.h"
#include "net/base/io_buffer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
const int kBufferSize = 1024;
}

namespace content {

MockURLRequestDelegate::MockURLRequestDelegate()
    : io_buffer_(new net::IOBuffer(kBufferSize)) {
}

MockURLRequestDelegate::~MockURLRequestDelegate() {
}

void MockURLRequestDelegate::OnResponseStarted(net::URLRequest* request) {
  // TODO(vadimt): Remove ScopedTracker below once crbug.com/422516 is fixed.
  tracked_objects::ScopedTracker tracking_profile(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "422516 MockURLRequestDelegate::OnResponseStarted"));

  if (request->status().is_success()) {
    EXPECT_TRUE(request->response_headers());
    ReadSome(request);
  } else {
    RequestComplete();
  }
}

void MockURLRequestDelegate::OnReadCompleted(net::URLRequest* request,
                                             int bytes_read) {
  // TODO(vadimt): Remove ScopedTracker below once crbug.com/422516 is fixed.
  tracked_objects::ScopedTracker tracking_profile(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "422516 MockURLRequestDelegate::OnReadCompleted"));

  if (bytes_read > 0)
    ReceiveData(request, bytes_read);
  else
    RequestComplete();
}

void MockURLRequestDelegate::ReadSome(net::URLRequest* request) {
  if (!request->is_pending()) {
    RequestComplete();
    return;
  }

  int bytes_read = 0;
  if (!request->Read(io_buffer_.get(), kBufferSize, &bytes_read)) {
    if (!request->status().is_io_pending())
      RequestComplete();
    return;
  }

  ReceiveData(request, bytes_read);
}

void MockURLRequestDelegate::ReceiveData(net::URLRequest* request,
                                         int bytes_read) {
  if (bytes_read) {
    response_data_.append(io_buffer_->data(),
                          static_cast<size_t>(bytes_read));
    ReadSome(request);
  } else {
    RequestComplete();
  }
}

void MockURLRequestDelegate::RequestComplete() {
  base::MessageLoop::current()->Quit();
}

}  // namespace
