// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This tests the performance of the C API.

#include "mojo/public/system/core.h"

#include <assert.h>

#include "mojo/public/system/macros.h"
#include "mojo/public/tests/test_support.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class CorePerftest : public testing::Test {
 public:
  CorePerftest() : buffer_(NULL), num_bytes_(0) {}
  virtual ~CorePerftest() {}

  static void NoOp(void* /*closure*/) {
  }

  static void MessagePipe_CreateAndClose(void* closure) {
    CorePerftest* self = static_cast<CorePerftest*>(closure);
    MojoResult result MOJO_ALLOW_UNUSED;
    result = MojoCreateMessagePipe(&self->h_0_, &self->h_1_);
    assert(result == MOJO_RESULT_OK);
    result = MojoClose(self->h_0_);
    assert(result == MOJO_RESULT_OK);
    result = MojoClose(self->h_1_);
    assert(result == MOJO_RESULT_OK);
  }

  static void MessagePipe_WriteAndRead(void* closure) {
    CorePerftest* self = static_cast<CorePerftest*>(closure);
    MojoResult result MOJO_ALLOW_UNUSED;
    result = MojoWriteMessage(self->h_0_,
                              self->buffer_, self->num_bytes_,
                              NULL, 0,
                              MOJO_WRITE_MESSAGE_FLAG_NONE);
    assert(result == MOJO_RESULT_OK);
    uint32_t read_bytes = self->num_bytes_;
    result = MojoReadMessage(self->h_1_,
                             self->buffer_, &read_bytes,
                             NULL, NULL,
                             MOJO_READ_MESSAGE_FLAG_NONE);
    assert(result == MOJO_RESULT_OK);
  }

  static void MessagePipe_EmptyRead(void* closure) {
    CorePerftest* self = static_cast<CorePerftest*>(closure);
    MojoResult result MOJO_ALLOW_UNUSED;
    result = MojoReadMessage(self->h_0_,
                             NULL, NULL,
                             NULL, NULL,
                             MOJO_READ_MESSAGE_FLAG_MAY_DISCARD);
    assert(result == MOJO_RESULT_SHOULD_WAIT);
  }

 protected:
  MojoHandle h_0_;
  MojoHandle h_1_;

  void* buffer_;
  uint32_t num_bytes_;

 private:
  MOJO_DISALLOW_COPY_AND_ASSIGN(CorePerftest);
};

// A no-op test so we can compare performance.
TEST_F(CorePerftest, NoOp) {
  mojo::test::IterateAndReportPerf("NoOp", &CorePerftest::NoOp, this);
}

TEST_F(CorePerftest, MessagePipe_CreateAndClose) {
  mojo::test::IterateAndReportPerf("MessagePipe_CreateAndClose",
                                   &CorePerftest::MessagePipe_CreateAndClose,
                                   this);
}

TEST_F(CorePerftest, MessagePipe_WriteAndRead) {
  MojoResult result MOJO_ALLOW_UNUSED;
  result = MojoCreateMessagePipe(&h_0_, &h_1_);
  assert(result == MOJO_RESULT_OK);
  char buffer[10000] = { 0 };
  buffer_ = buffer;
  num_bytes_ = 10u;
  mojo::test::IterateAndReportPerf("MessagePipe_WriteAndRead_10bytes",
                                   &CorePerftest::MessagePipe_WriteAndRead,
                                   this);
  num_bytes_ = 100u;
  mojo::test::IterateAndReportPerf("MessagePipe_WriteAndRead_100bytes",
                                   &CorePerftest::MessagePipe_WriteAndRead,
                                   this);
  num_bytes_ = 1000u;
  mojo::test::IterateAndReportPerf("MessagePipe_WriteAndRead_1000bytes",
                                   &CorePerftest::MessagePipe_WriteAndRead,
                                   this);
  num_bytes_ = 10000u;
  mojo::test::IterateAndReportPerf("MessagePipe_WriteAndRead_10000bytes",
                                   &CorePerftest::MessagePipe_WriteAndRead,
                                   this);
  result = MojoClose(h_0_);
  assert(result == MOJO_RESULT_OK);
  result = MojoClose(h_1_);
  assert(result == MOJO_RESULT_OK);
}

TEST_F(CorePerftest, MessagePipe_EmptyRead) {
  MojoResult result MOJO_ALLOW_UNUSED;
  result = MojoCreateMessagePipe(&h_0_, &h_1_);
  assert(result == MOJO_RESULT_OK);
  mojo::test::IterateAndReportPerf("MessagePipe_EmptyRead",
                                   &CorePerftest::MessagePipe_EmptyRead,
                                   this);
  result = MojoClose(h_0_);
  assert(result == MOJO_RESULT_OK);
  result = MojoClose(h_1_);
  assert(result == MOJO_RESULT_OK);
}

}  // namespace
