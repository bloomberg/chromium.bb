// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/edk/test/mojo_test_base.h"
#include "mojo/public/c/system/buffer.h"
#include "mojo/public/c/system/data_pipe.h"
#include "mojo/public/c/system/functions.h"
#include "mojo/public/c/system/message_pipe.h"
#include "mojo/public/c/system/types.h"

namespace mojo {
namespace edk {
namespace {

using SignalsTest = test::MojoTestBase;

TEST_F(SignalsTest, QueryInvalidArguments) {
  MojoHandleSignalsState state = {0, 0};
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
            MojoQueryHandleSignalsState(MOJO_HANDLE_INVALID, &state));

  MojoHandle a, b;
  CreateMessagePipe(&a, &b);
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
            MojoQueryHandleSignalsState(a, nullptr));
}

TEST_F(SignalsTest, QueryMessagePipeSignals) {
  MojoHandleSignalsState state = {0, 0};

  MojoHandle a, b;
  CreateMessagePipe(&a, &b);

  EXPECT_EQ(MOJO_RESULT_OK, MojoQueryHandleSignalsState(a, &state));
  EXPECT_EQ(MOJO_HANDLE_SIGNAL_WRITABLE, state.satisfied_signals);
  EXPECT_EQ(MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_WRITABLE |
                MOJO_HANDLE_SIGNAL_PEER_CLOSED,
            state.satisfiable_signals);

  EXPECT_EQ(MOJO_RESULT_OK, MojoQueryHandleSignalsState(b, &state));
  EXPECT_EQ(MOJO_HANDLE_SIGNAL_WRITABLE, state.satisfied_signals);
  EXPECT_EQ(MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_WRITABLE |
                MOJO_HANDLE_SIGNAL_PEER_CLOSED,
            state.satisfiable_signals);

  WriteMessage(a, "ok");
  EXPECT_EQ(MOJO_RESULT_OK, WaitForSignals(b, MOJO_HANDLE_SIGNAL_READABLE));

  EXPECT_EQ(MOJO_RESULT_OK, MojoQueryHandleSignalsState(b, &state));
  EXPECT_EQ(MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_WRITABLE,
            state.satisfied_signals);
  EXPECT_EQ(MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_WRITABLE |
                MOJO_HANDLE_SIGNAL_PEER_CLOSED,
            state.satisfiable_signals);

  EXPECT_EQ("ok", ReadMessage(b));

  EXPECT_EQ(MOJO_RESULT_OK, MojoQueryHandleSignalsState(b, &state));
  EXPECT_EQ(MOJO_HANDLE_SIGNAL_WRITABLE, state.satisfied_signals);
  EXPECT_EQ(MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_WRITABLE |
                MOJO_HANDLE_SIGNAL_PEER_CLOSED,
            state.satisfiable_signals);

  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(a));

  EXPECT_EQ(MOJO_RESULT_OK, WaitForSignals(b, MOJO_HANDLE_SIGNAL_PEER_CLOSED));

  EXPECT_EQ(MOJO_RESULT_OK, MojoQueryHandleSignalsState(b, &state));
  EXPECT_EQ(MOJO_HANDLE_SIGNAL_PEER_CLOSED, state.satisfied_signals);
  EXPECT_EQ(MOJO_HANDLE_SIGNAL_PEER_CLOSED, state.satisfiable_signals);
}

}  // namespace
}  // namespace edk
}  // namespace mojo
