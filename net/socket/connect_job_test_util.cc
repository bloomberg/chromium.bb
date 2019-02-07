// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/connect_job_test_util.h"

#include <utility>

#include "base/logging.h"
#include "base/run_loop.h"
#include "net/socket/stream_socket.h"
#include "net/test/gtest_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

TestConnectJobDelegate::TestConnectJobDelegate(SocketExpected socket_expected)
    : socket_expected_(socket_expected) {}

TestConnectJobDelegate::~TestConnectJobDelegate() = default;

void TestConnectJobDelegate::OnConnectJobComplete(int result, ConnectJob* job) {
  EXPECT_FALSE(has_result_);
  result_ = result;
  socket_ = job->PassSocket();
  EXPECT_EQ(socket_.get() != nullptr,
            result == OK || socket_expected_ == SocketExpected::ALWAYS);
  has_result_ = true;
  run_loop_.Quit();
}

int TestConnectJobDelegate::WaitForResult() {
  run_loop_.Run();
  DCHECK(has_result_);
  return result_;
}

void TestConnectJobDelegate::StartJobExpectingResult(ConnectJob* connect_job,
                                                     net::Error expected_result,
                                                     bool expect_sync_result) {
  int rv = connect_job->Connect();
  if (rv == ERR_IO_PENDING) {
    EXPECT_FALSE(expect_sync_result);
    EXPECT_THAT(WaitForResult(), test::IsError(expected_result));
  } else {
    EXPECT_TRUE(expect_sync_result);
    // The callback should not have been invoked.
    ASSERT_FALSE(has_result_);
    OnConnectJobComplete(rv, connect_job);
    EXPECT_THAT(result_, test::IsError(expected_result));
  }
}

std::unique_ptr<StreamSocket> TestConnectJobDelegate::ReleaseSocket() {
  return std::move(socket_);
}

}  // namespace net
