// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/system/core.h"

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/logging.h"
#include "mojo/public/tests/test_support.h"

namespace mojo {
namespace {

class SystemPerftest : public test::TestBase {
 public:
  SystemPerftest() {}
  virtual ~SystemPerftest() {}

  void NoOp() {
  }

  void MessagePipe_CreateAndClose() {
    MojoResult result;
    result = CreateMessagePipe(&h_0_, &h_1_);
    DCHECK_EQ(result, MOJO_RESULT_OK);
    result = Close(h_0_);
    DCHECK_EQ(result, MOJO_RESULT_OK);
    result = Close(h_1_);
    DCHECK_EQ(result, MOJO_RESULT_OK);
  }

  void MessagePipe_WriteAndRead(void* buffer, uint32_t bytes) {
    MojoResult result;
    result = WriteMessage(h_0_,
                          buffer, bytes,
                          NULL, 0,
                          MOJO_WRITE_MESSAGE_FLAG_NONE);
    DCHECK_EQ(result, MOJO_RESULT_OK);
    uint32_t read_bytes = bytes;
    result = ReadMessage(h_1_,
                         buffer, &read_bytes,
                         NULL, NULL,
                         MOJO_READ_MESSAGE_FLAG_NONE);
    DCHECK_EQ(result, MOJO_RESULT_OK);
  }

  void MessagePipe_EmptyRead() {
    MojoResult result;
    result = ReadMessage(h_0_,
                         NULL, NULL,
                         NULL, NULL,
                         MOJO_READ_MESSAGE_FLAG_MAY_DISCARD);
    DCHECK_EQ(result, MOJO_RESULT_NOT_FOUND);
  }

 protected:
  Handle h_0_;
  Handle h_1_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SystemPerftest);
};

// A no-op test so we can compare performance.
TEST_F(SystemPerftest, NoOp) {
  test::IterateAndReportPerf(
      "NoOp",
      base::Bind(&SystemPerftest::NoOp,
                 base::Unretained(this)));
}

TEST_F(SystemPerftest, MessagePipe_CreateAndClose) {
  test::IterateAndReportPerf(
      "MessagePipe_CreateAndClose",
      base::Bind(&SystemPerftest::MessagePipe_CreateAndClose,
                 base::Unretained(this)));
}

TEST_F(SystemPerftest, MessagePipe_WriteAndRead) {
  CHECK_EQ(CreateMessagePipe(&h_0_, &h_1_), MOJO_RESULT_OK);
  char buffer[10000] = { 0 };
  test::IterateAndReportPerf(
      "MessagePipe_WriteAndRead_10bytes",
      base::Bind(&SystemPerftest::MessagePipe_WriteAndRead,
                 base::Unretained(this),
                 static_cast<void*>(buffer), static_cast<uint32_t>(10)));
  test::IterateAndReportPerf(
      "MessagePipe_WriteAndRead_100bytes",
      base::Bind(&SystemPerftest::MessagePipe_WriteAndRead,
                 base::Unretained(this),
                 static_cast<void*>(buffer), static_cast<uint32_t>(100)));
  test::IterateAndReportPerf(
      "MessagePipe_WriteAndRead_1000bytes",
      base::Bind(&SystemPerftest::MessagePipe_WriteAndRead,
                 base::Unretained(this),
                 static_cast<void*>(buffer), static_cast<uint32_t>(1000)));
  test::IterateAndReportPerf(
      "MessagePipe_WriteAndRead_10000bytes",
      base::Bind(&SystemPerftest::MessagePipe_WriteAndRead,
                 base::Unretained(this),
                 static_cast<void*>(buffer), static_cast<uint32_t>(10000)));
  CHECK_EQ(Close(h_0_), MOJO_RESULT_OK);
  CHECK_EQ(Close(h_1_), MOJO_RESULT_OK);
}

TEST_F(SystemPerftest, MessagePipe_EmptyRead) {
  CHECK_EQ(CreateMessagePipe(&h_0_, &h_1_), MOJO_RESULT_OK);
  test::IterateAndReportPerf(
      "MessagePipe_EmptyRead",
      base::Bind(&SystemPerftest::MessagePipe_EmptyRead,
                 base::Unretained(this)));
  CHECK_EQ(Close(h_0_), MOJO_RESULT_OK);
  CHECK_EQ(Close(h_1_), MOJO_RESULT_OK);
}

}  // namespace
}  // namespace mojo
