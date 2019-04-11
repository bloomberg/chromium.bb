// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/bind_test_util.h"
#include "build/build_config.h"
#include "mojo/core/test/mojo_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace core {
namespace {

// Helper for tests to write a single |spliced_handle| to be spliced via a
// message sent over |sender|.
void WriteSplicedHandle(MojoHandle sender, MojoHandle spliced_handle) {
  MojoMessageHandle m;
  CHECK_EQ(MOJO_RESULT_OK, MojoCreateMessage(nullptr, &m));

  MojoAppendMessageDataHandleOptions handle_options = {0};
  handle_options.struct_size = sizeof(handle_options);
  handle_options.flags = MOJO_APPEND_MESSAGE_DATA_HANDLE_FLAG_SPLICE;

  MojoAppendMessageDataOptions append_options = {0};
  append_options.struct_size = sizeof(append_options);
  append_options.flags = MOJO_APPEND_MESSAGE_DATA_FLAG_COMMIT_SIZE;
  append_options.handle_options = &handle_options;
  CHECK_EQ(MOJO_RESULT_OK,
           MojoAppendMessageData(m, 0, &spliced_handle, 1, &append_options,
                                 nullptr, nullptr));

  CHECK_EQ(MOJO_RESULT_OK, MojoWriteMessage(sender, m, nullptr));
}

MojoHandle ReadSplicedHandle(MojoHandle receiver) {
  test::MojoTestBase::WaitForSignals(receiver, MOJO_HANDLE_SIGNAL_READABLE);

  MojoMessageHandle m;
  CHECK_EQ(MOJO_RESULT_OK, MojoReadMessage(receiver, nullptr, &m));

  uint32_t num_handles = 1;
  MojoHandle spliced_handle;
  CHECK_EQ(MOJO_RESULT_OK, MojoGetMessageData(m, nullptr, nullptr, nullptr,
                                              &spliced_handle, &num_handles));
  CHECK_EQ(1u, num_handles);
  CHECK_EQ(MOJO_RESULT_OK, MojoDestroyMessage(m));

  return spliced_handle;
}

using SplicedMessagePipeTest = test::MojoTestBase;

TEST_F(SplicedMessagePipeTest, Basic) {
  MojoHandle a, b;
  CreateMessagePipe(&a, &b);

  MojoHandle c, d;
  CreateMessagePipe(&c, &d);

  // Splice |c <=> d| with |a <=> b|.
  WriteSplicedHandle(a, d);
  d = ReadSplicedHandle(b);

  // Write a message to |a| and then to |c|.
  const std::string kTestMessage1 = "1";
  const std::string kTestMessage2 = "2";
  WriteMessage(a, kTestMessage1);
  WriteMessage(c, kTestMessage2);

  // |d| must not be readable until the first message is read from |b|.
  EXPECT_TRUE(GetSignalsState(b).satisfied_signals &
              MOJO_HANDLE_SIGNAL_READABLE);
  EXPECT_FALSE(GetSignalsState(d).satisfied_signals &
               MOJO_HANDLE_SIGNAL_READABLE);
  EXPECT_EQ(kTestMessage1, ReadMessage(b));

  EXPECT_FALSE(GetSignalsState(b).satisfied_signals &
               MOJO_HANDLE_SIGNAL_READABLE);
  EXPECT_TRUE(GetSignalsState(d).satisfied_signals &
              MOJO_HANDLE_SIGNAL_READABLE);
  EXPECT_EQ(kTestMessage2, ReadMessage(d));

  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(a));
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(b));
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(c));
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(d));
}

TEST_F(SplicedMessagePipeTest, DropUndeliverableMessages) {
  MojoHandle a, b;
  CreateMessagePipe(&a, &b);

  MojoHandle c, d;
  CreateMessagePipe(&c, &d);

  MojoHandle e, f;
  CreateMessagePipe(&e, &f);

  // Splice |c <=> d| with |a <=> b|.
  WriteSplicedHandle(a, d);
  d = ReadSplicedHandle(b);

  // Also splice |e <=> f| with the above.
  WriteSplicedHandle(a, f);
  f = ReadSplicedHandle(b);

  // Write a series of messages to each of the above pipes.
  WriteMessage(a, "msg1");
  WriteMessage(c, "msg2");
  WriteMessage(e, "msg3");
  WriteMessage(c, "msg4");

  // |d| and |f| should not be readable yet, but |b| should be.
  EXPECT_TRUE(GetSignalsState(b).satisfied_signals &
              MOJO_HANDLE_SIGNAL_READABLE);
  EXPECT_FALSE(GetSignalsState(d).satisfied_signals &
               MOJO_HANDLE_SIGNAL_READABLE);
  EXPECT_FALSE(GetSignalsState(f).satisfied_signals &
               MOJO_HANDLE_SIGNAL_READABLE);

  // Close |b| and |f| without reading their messages. This should allow both
  // messages on |d| to be read.
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(b));
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(f));

  EXPECT_TRUE(GetSignalsState(d).satisfied_signals &
              MOJO_HANDLE_SIGNAL_READABLE);
  EXPECT_EQ("msg2", ReadMessage(d));
  EXPECT_EQ("msg4", ReadMessage(d));

  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(a));
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(c));
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(d));
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(e));
}

TEST_F(SplicedMessagePipeTest, CloseOriginalPipe) {
  MojoHandle a, b;
  CreateMessagePipe(&a, &b);

  MojoHandle c, d;
  CreateMessagePipe(&c, &d);

  // Splice |c <=> d| with |a <=> b|.
  WriteSplicedHandle(a, d);
  d = ReadSplicedHandle(b);

  // Close |a| and |b| immediately. |c <=> d| should still be operational.
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(a));
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(b));

  WriteMessage(c, "x");
  EXPECT_EQ("x", ReadMessage(d));

  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(c));
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(d));
}

TEST_F(SplicedMessagePipeTest, PeerClosure) {
  MojoHandle a, b;
  CreateMessagePipe(&a, &b);

  MojoHandle c, d;
  CreateMessagePipe(&c, &d);

  WriteSplicedHandle(a, d);
  d = ReadSplicedHandle(b);

  WriteMessage(c, "x");
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(c));

  WaitForSignals(d, MOJO_HANDLE_SIGNAL_PEER_CLOSED);
  EXPECT_TRUE(GetSignalsState(d).satisfied_signals &
              MOJO_HANDLE_SIGNAL_READABLE);
  EXPECT_EQ("x", ReadMessage(d));
  EXPECT_FALSE(GetSignalsState(d).satisfiable_signals &
               MOJO_HANDLE_SIGNAL_READABLE);

  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(a));
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(b));
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(d));
}

void EventHandler(const MojoTrapEvent* event) {
  auto* callback =
      reinterpret_cast<base::RepeatingClosure*>(event->trigger_context);
  if (event->result == MOJO_RESULT_OK)
    callback->Run();
}

uintptr_t MakeContext(base::RepeatingClosure* callback) {
  return reinterpret_cast<uintptr_t>(callback);
}

TEST_F(SplicedMessagePipeTest, SignalHandlerOrdering) {
  MojoHandle a, b;
  CreateMessagePipe(&a, &b);

  MojoHandle c, d;
  CreateMessagePipe(&c, &d);

  MojoHandle e, f;
  CreateMessagePipe(&e, &f);

  WriteSplicedHandle(a, d);
  d = ReadSplicedHandle(b);

  WriteSplicedHandle(a, f);
  f = ReadSplicedHandle(b);

  MojoHandle b_trap;
  EXPECT_EQ(MOJO_RESULT_OK, MojoCreateTrap(&EventHandler, nullptr, &b_trap));
  MojoHandle d_trap;
  EXPECT_EQ(MOJO_RESULT_OK, MojoCreateTrap(&EventHandler, nullptr, &d_trap));
  MojoHandle f_trap;
  EXPECT_EQ(MOJO_RESULT_OK, MojoCreateTrap(&EventHandler, nullptr, &f_trap));

  int messages_read = 0;

  auto on_b_readable = base::BindLambdaForTesting([&]() {
    EXPECT_EQ(0, messages_read);
    EXPECT_EQ("x", ReadMessage(b));
    ++messages_read;
  });

  auto on_d_readable = base::BindLambdaForTesting([&]() {
    EXPECT_EQ(1, messages_read);
    EXPECT_EQ("y", ReadMessage(d));
    ++messages_read;
  });

  auto on_f_readable = base::BindLambdaForTesting([&]() {
    EXPECT_EQ(2, messages_read);
    EXPECT_EQ("z", ReadMessage(f));
    ++messages_read;
  });

  EXPECT_EQ(MOJO_RESULT_OK,
            MojoAddTrigger(b_trap, b, MOJO_HANDLE_SIGNAL_READABLE,
                           MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
                           MakeContext(&on_b_readable), nullptr));
  EXPECT_EQ(MOJO_RESULT_OK,
            MojoAddTrigger(d_trap, d, MOJO_HANDLE_SIGNAL_READABLE,
                           MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
                           MakeContext(&on_d_readable), nullptr));
  EXPECT_EQ(MOJO_RESULT_OK,
            MojoAddTrigger(f_trap, f, MOJO_HANDLE_SIGNAL_READABLE,
                           MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
                           MakeContext(&on_f_readable), nullptr));
  EXPECT_EQ(MOJO_RESULT_OK, MojoArmTrap(b_trap, nullptr, nullptr, nullptr));
  EXPECT_EQ(MOJO_RESULT_OK, MojoArmTrap(d_trap, nullptr, nullptr, nullptr));
  EXPECT_EQ(MOJO_RESULT_OK, MojoArmTrap(f_trap, nullptr, nullptr, nullptr));

  WriteMessage(a, "x");
  WriteMessage(c, "y");
  WriteMessage(e, "z");
  EXPECT_EQ(3, messages_read);

  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(a));
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(b));
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(c));
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(d));
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(e));
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(f));
}

// No multi-process support for iOS.
#if !defined(OS_IOS)

#if defined(OS_FUCHSIA)
// Flaky: https://crbug.com/950983
#define MAYBE_Multiprocess DISABLED_Multiprocess
#else
#define MAYBE_Multiprocess Multiprocess
#endif

TEST_F(SplicedMessagePipeTest, MAYBE_Multiprocess) {
  MojoHandle a, b;
  CreateMessagePipe(&a, &b);
  RunTestClient("Client1", [&](MojoHandle h) {
    WriteMessageWithHandles(h, "hi", &a, 1);
    RunTestClient("Client2", [&](MojoHandle h) {
      WriteMessageWithHandles(h, "hi", &b, 1);

      EXPECT_EQ("ok", ReadMessage(h));
      WriteMessage(h, "bye");
    });

    EXPECT_EQ("ok", ReadMessage(h));
    WriteMessage(h, "bye");
  });
}

DEFINE_TEST_CLIENT_TEST_WITH_PIPE(Client1, SplicedMessagePipeTest, h) {
  MojoHandle a;
  EXPECT_EQ("hi", ReadMessageWithHandles(h, &a, 1));

  MojoHandle c, d;
  CreateMessagePipe(&c, &d);
  WriteSplicedHandle(a, d);

  WriteMessage(a, "1");
  WriteMessage(c, "2");
  WriteMessage(a, "3");
  WriteMessage(c, "4");

  // Test the other direction for good measure.
  EXPECT_EQ("5", ReadMessage(c));

  WriteMessage(h, "ok");
  EXPECT_EQ("bye", ReadMessage(h));
}

DEFINE_TEST_CLIENT_TEST_WITH_PIPE(Client2, SplicedMessagePipeTest, h) {
  MojoHandle b;
  EXPECT_EQ("hi", ReadMessageWithHandles(h, &b, 1));

  MojoHandle d = ReadSplicedHandle(b);

  WaitForSignals(b, MOJO_HANDLE_SIGNAL_READABLE);
  EXPECT_FALSE(GetSignalsState(d).satisfied_signals &
               MOJO_HANDLE_SIGNAL_READABLE);
  EXPECT_EQ("1", ReadMessage(b));

  WaitForSignals(d, MOJO_HANDLE_SIGNAL_READABLE);
  EXPECT_FALSE(GetSignalsState(b).satisfied_signals &
               MOJO_HANDLE_SIGNAL_READABLE);
  EXPECT_EQ("2", ReadMessage(d));

  WaitForSignals(b, MOJO_HANDLE_SIGNAL_READABLE);
  EXPECT_FALSE(GetSignalsState(d).satisfied_signals &
               MOJO_HANDLE_SIGNAL_READABLE);
  EXPECT_EQ("3", ReadMessage(b));

  WaitForSignals(d, MOJO_HANDLE_SIGNAL_READABLE);
  EXPECT_FALSE(GetSignalsState(b).satisfied_signals &
               MOJO_HANDLE_SIGNAL_READABLE);
  EXPECT_EQ("4", ReadMessage(d));

  WriteMessage(d, "5");

  WriteMessage(h, "ok");
  EXPECT_EQ("bye", ReadMessage(h));
}

#endif  // !defined(OS_IOS)

}  // namespace
}  // namespace core
}  // namespace mojo
