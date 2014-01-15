// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This tests the performance of the C API.

#include "mojo/public/system/core.h"

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/logging.h"
#include "mojo/public/tests/test_support.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class CorePerftest : public testing::Test {
 public:
  CorePerftest() {}
  virtual ~CorePerftest() {}

  void NoOp() {
  }

  void MessagePipe_CreateAndClose() {
    MojoResult result;
    result = MojoCreateMessagePipe(&h_0_, &h_1_);
    DCHECK_EQ(result, MOJO_RESULT_OK);
    result = MojoClose(h_0_);
    DCHECK_EQ(result, MOJO_RESULT_OK);
    result = MojoClose(h_1_);
    DCHECK_EQ(result, MOJO_RESULT_OK);
  }

  void MessagePipe_WriteAndRead(void* buffer, uint32_t bytes) {
    MojoResult result;
    result = MojoWriteMessage(h_0_,
                              buffer, bytes,
                              NULL, 0,
                              MOJO_WRITE_MESSAGE_FLAG_NONE);
    DCHECK_EQ(result, MOJO_RESULT_OK);
    uint32_t read_bytes = bytes;
    result = MojoReadMessage(h_1_,
                             buffer, &read_bytes,
                             NULL, NULL,
                             MOJO_READ_MESSAGE_FLAG_NONE);
    DCHECK_EQ(result, MOJO_RESULT_OK);
  }

  void MessagePipe_EmptyRead() {
    MojoResult result;
    result = MojoReadMessage(h_0_,
                             NULL, NULL,
                             NULL, NULL,
                             MOJO_READ_MESSAGE_FLAG_MAY_DISCARD);
    DCHECK_EQ(result, MOJO_RESULT_SHOULD_WAIT);
  }

 protected:
  MojoHandle h_0_;
  MojoHandle h_1_;

 private:
  DISALLOW_COPY_AND_ASSIGN(CorePerftest);
};

// A no-op test so we can compare performance.
TEST_F(CorePerftest, NoOp) {
  mojo::test::IterateAndReportPerf(
      "NoOp",
      base::Bind(&CorePerftest::NoOp,
                 base::Unretained(this)));
}

TEST_F(CorePerftest, MessagePipe_CreateAndClose) {
  mojo::test::IterateAndReportPerf(
      "MessagePipe_CreateAndClose",
      base::Bind(&CorePerftest::MessagePipe_CreateAndClose,
                 base::Unretained(this)));
}

TEST_F(CorePerftest, MessagePipe_WriteAndRead) {
  CHECK_EQ(MojoCreateMessagePipe(&h_0_, &h_1_), MOJO_RESULT_OK);
  char buffer[10000] = { 0 };
  mojo::test::IterateAndReportPerf(
      "MessagePipe_WriteAndRead_10bytes",
      base::Bind(&CorePerftest::MessagePipe_WriteAndRead,
                 base::Unretained(this),
                 static_cast<void*>(buffer), static_cast<uint32_t>(10)));
  mojo::test::IterateAndReportPerf(
      "MessagePipe_WriteAndRead_100bytes",
      base::Bind(&CorePerftest::MessagePipe_WriteAndRead,
                 base::Unretained(this),
                 static_cast<void*>(buffer), static_cast<uint32_t>(100)));
  mojo::test::IterateAndReportPerf(
      "MessagePipe_WriteAndRead_1000bytes",
      base::Bind(&CorePerftest::MessagePipe_WriteAndRead,
                 base::Unretained(this),
                 static_cast<void*>(buffer), static_cast<uint32_t>(1000)));
  mojo::test::IterateAndReportPerf(
      "MessagePipe_WriteAndRead_10000bytes",
      base::Bind(&CorePerftest::MessagePipe_WriteAndRead,
                 base::Unretained(this),
                 static_cast<void*>(buffer), static_cast<uint32_t>(10000)));
  CHECK_EQ(MojoClose(h_0_), MOJO_RESULT_OK);
  CHECK_EQ(MojoClose(h_1_), MOJO_RESULT_OK);
}

TEST_F(CorePerftest, MessagePipe_EmptyRead) {
  CHECK_EQ(MojoCreateMessagePipe(&h_0_, &h_1_), MOJO_RESULT_OK);
  mojo::test::IterateAndReportPerf(
      "MessagePipe_EmptyRead",
      base::Bind(&CorePerftest::MessagePipe_EmptyRead,
                 base::Unretained(this)));
  CHECK_EQ(MojoClose(h_0_), MOJO_RESULT_OK);
  CHECK_EQ(MojoClose(h_1_), MOJO_RESULT_OK);
}

}  // namespace
