// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/edk/embedder/embedder.h"

#include <stddef.h>
#include <stdint.h>

#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/test_timeouts.h"
#include "mojo/edk/embedder/embedder.h"
#include "mojo/edk/embedder/platform_channel_pair.h"
#include "mojo/edk/embedder/simple_platform_support.h"
#include "mojo/edk/embedder/test_embedder.h"
#include "mojo/edk/system/test_utils.h"
#include "mojo/edk/test/mojo_test_base.h"
#include "mojo/message_pump/message_pump_mojo.h"
#include "mojo/public/c/system/core.h"
#include "mojo/public/cpp/system/handle.h"
#include "mojo/public/cpp/system/macros.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace edk {
namespace {

const MojoHandleSignals kSignalReadadableWritable =
    MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_WRITABLE;

const MojoHandleSignals kSignalAll = MOJO_HANDLE_SIGNAL_READABLE |
                                     MOJO_HANDLE_SIGNAL_WRITABLE |
                                     MOJO_HANDLE_SIGNAL_PEER_CLOSED;

using EmbedderTest = test::MojoTestBase;

TEST_F(EmbedderTest, ChannelBasic) {
  MojoHandle server_mp, client_mp;
  CreateMessagePipe(&server_mp, &client_mp);

  const std::string kHello = "hello";

  // We can write to a message pipe handle immediately.
  WriteMessage(server_mp, kHello);
  EXPECT_EQ(kHello, ReadMessage(client_mp));

  ASSERT_EQ(MOJO_RESULT_OK, MojoClose(server_mp));
  ASSERT_EQ(MOJO_RESULT_OK, MojoClose(client_mp));
}

// Test sending a MP which has read messages out of the OS pipe but which have
// not been consumed using MojoReadMessage yet.
TEST_F(EmbedderTest, SendReadableMessagePipe) {
  MojoHandle server_mp, client_mp;
  CreateMessagePipe(&server_mp, &client_mp);

  MojoHandle server_mp2, client_mp2;
  CreateMessagePipe(&server_mp2, &client_mp2);

  // Write to server2 and wait for client2 to be readable before sending it.
  // client2's MessagePipeDispatcher will have the message below in its
  // message_queue_. For extra measures, also verify that this pending message
  // can contain a message pipe.
  MojoHandle server_mp3, client_mp3;
  CreateMessagePipe(&server_mp3, &client_mp3);

  const std::string kHello = "hello";
  WriteMessageWithHandles(server_mp2, kHello, &client_mp3, 1);

  MojoHandleSignalsState state;
  ASSERT_EQ(MOJO_RESULT_OK, MojoWait(client_mp2, MOJO_HANDLE_SIGNAL_READABLE,
                                     MOJO_DEADLINE_INDEFINITE, &state));
  ASSERT_EQ(kSignalReadadableWritable, state.satisfied_signals);
  ASSERT_EQ(kSignalAll, state.satisfiable_signals);

  // Now send client2
  WriteMessageWithHandles(server_mp, kHello, &client_mp2, 1);

  MojoHandle port;
  std::string message = ReadMessageWithHandles(client_mp, &port, 1);
  EXPECT_EQ(kHello, message);

  client_mp2 = port;
  message = ReadMessageWithHandles(client_mp2, &client_mp3, 1);
  EXPECT_EQ(kHello, message);

  ASSERT_EQ(MOJO_RESULT_OK, MojoClose(server_mp3));
  ASSERT_EQ(MOJO_RESULT_OK, MojoClose(client_mp3));
  ASSERT_EQ(MOJO_RESULT_OK, MojoClose(server_mp2));
  ASSERT_EQ(MOJO_RESULT_OK, MojoClose(client_mp2));
  ASSERT_EQ(MOJO_RESULT_OK, MojoClose(server_mp));
  ASSERT_EQ(MOJO_RESULT_OK, MojoClose(client_mp));
}

// Verifies that a MP with pending messages to be written can be sent and the
// pending messages aren't dropped.
TEST_F(EmbedderTest, SendMessagePipeWithWriteQueue) {
  MojoHandle server_mp, client_mp;
  CreateMessagePipe(&server_mp, &client_mp);

  MojoHandle server_mp2, client_mp2;
  CreateMessagePipe(&server_mp2, &client_mp2);

  static const size_t kNumMessages = 1001;
  for (size_t i = 1; i <= kNumMessages; i++)
    WriteMessage(client_mp2, std::string(i, 'A' + (i % 26)));

  // Now send client2.
  WriteMessageWithHandles(server_mp, "hey", &client_mp2, 1);
  client_mp2 = MOJO_HANDLE_INVALID;

  // Read client2 just so we can close it later.
  EXPECT_EQ("hey", ReadMessageWithHandles(client_mp, &client_mp2, 1));
  EXPECT_NE(MOJO_HANDLE_INVALID, client_mp2);

  // Now verify that all the messages that were written were sent correctly.
  for (size_t i = 1; i <= kNumMessages; i++)
    ASSERT_EQ(std::string(i, 'A' + (i % 26)), ReadMessage(server_mp2));

  ASSERT_EQ(MOJO_RESULT_OK, MojoClose(server_mp2));
  ASSERT_EQ(MOJO_RESULT_OK, MojoClose(client_mp2));
  ASSERT_EQ(MOJO_RESULT_OK, MojoClose(server_mp));
  ASSERT_EQ(MOJO_RESULT_OK, MojoClose(client_mp));
}

TEST_F(EmbedderTest, ChannelsHandlePassing) {
  MojoHandle server_mp, client_mp;
  CreateMessagePipe(&server_mp, &client_mp);
  EXPECT_NE(server_mp, MOJO_HANDLE_INVALID);
  EXPECT_NE(client_mp, MOJO_HANDLE_INVALID);

  MojoHandle h0, h1;
  CreateMessagePipe(&h0, &h1);

  // Write a message to |h0| (attaching nothing).
  const std::string kHello = "hello";
  WriteMessage(h0, kHello);

  // Write one message to |server_mp|, attaching |h1|.
  const std::string kWorld = "world!!!";
  WriteMessageWithHandles(server_mp, kWorld, &h1, 1);
  h1 = MOJO_HANDLE_INVALID;

  // Write another message to |h0|.
  const std::string kFoo = "foo";
  WriteMessage(h0, kFoo);

  // Wait for |client_mp| to become readable and read a message from it.
  EXPECT_EQ(kWorld, ReadMessageWithHandles(client_mp, &h1, 1));
  EXPECT_NE(h1, MOJO_HANDLE_INVALID);

  // Wait for |h1| to become readable and read a message from it.
  EXPECT_EQ(kHello, ReadMessage(h1));

  // Wait for |h1| to become readable (again) and read its second message.
  EXPECT_EQ(kFoo, ReadMessage(h1));

  // Write a message to |h1|.
  const std::string kBarBaz = "barbaz";
  WriteMessage(h1, kBarBaz);

  // Wait for |h0| to become readable and read a message from it.
  EXPECT_EQ(kBarBaz, ReadMessage(h0));

  ASSERT_EQ(MOJO_RESULT_OK, MojoClose(server_mp));
  ASSERT_EQ(MOJO_RESULT_OK, MojoClose(client_mp));
  ASSERT_EQ(MOJO_RESULT_OK, MojoClose(h0));
  ASSERT_EQ(MOJO_RESULT_OK, MojoClose(h1));
}

// The sequence of messages sent is:
//       server_mp   client_mp   mp0         mp1         mp2         mp3
//   1.  "hello"
//   2.              "world!"
//   3.                          "FOO"
//   4.  "Bar"+mp1
//   5.  (close)
//   6.              (close)
//   7.                                                              "baz"
//   8.                                                              (closed)
//   9.                                      "quux"+mp2
//  10.                          (close)
//  11.                                      (wait/cl.)
//  12.                                                  (wait/cl.)

#if defined(OS_ANDROID)
// Android multi-process tests are not executing the new process. This is flaky.
#define MAYBE_MultiprocessChannels DISABLED_MultiprocessChannels
#else
#define MAYBE_MultiprocessChannels MultiprocessChannels
#endif  // defined(OS_ANDROID)
TEST_F(EmbedderTest, MAYBE_MultiprocessChannels) {
  RUN_CHILD_ON_PIPE(MultiprocessChannelsClient, server_mp)
    // 1. Write a message to |server_mp| (attaching nothing).
    WriteMessage(server_mp, "hello");

    // 2. Read a message from |server_mp|.
    EXPECT_EQ("world!", ReadMessage(server_mp));

    // 3. Create a new message pipe (endpoints |mp0| and |mp1|).
    MojoHandle mp0, mp1;
    CreateMessagePipe(&mp0, &mp1);

    // 4. Write something to |mp0|.
    WriteMessage(mp0, "FOO");

    // 5. Write a message to |server_mp|, attaching |mp1|.
    WriteMessageWithHandles(server_mp, "Bar", &mp1, 1);
    mp1 = MOJO_HANDLE_INVALID;

    // 6. Read a message from |mp0|, which should have |mp2| attached.
    MojoHandle mp2 = MOJO_HANDLE_INVALID;
    EXPECT_EQ("quux", ReadMessageWithHandles(mp0, &mp2, 1));

    // 7. Read a message from |mp2|.
    EXPECT_EQ("baz", ReadMessage(mp2));

    // 8. Close |mp0|.
    ASSERT_EQ(MOJO_RESULT_OK, MojoClose(mp0));

    // 9. Tell the client to quit.
    WriteMessage(server_mp, "quit");

    // 10. Wait on |mp2| (which should eventually fail) and then close it.
    MojoHandleSignalsState state;
    ASSERT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
              MojoWait(mp2, MOJO_HANDLE_SIGNAL_READABLE,
                       MOJO_DEADLINE_INDEFINITE,
                       &state));
    ASSERT_EQ(MOJO_HANDLE_SIGNAL_PEER_CLOSED, state.satisfied_signals);
    ASSERT_EQ(MOJO_HANDLE_SIGNAL_PEER_CLOSED, state.satisfiable_signals);

    ASSERT_EQ(MOJO_RESULT_OK, MojoClose(mp2));
  END_CHILD()
}

DEFINE_TEST_CLIENT_TEST_WITH_PIPE(MultiprocessChannelsClient, EmbedderTest,
                                  client_mp) {
  // 1. Read the first message from |client_mp|.
  EXPECT_EQ("hello", ReadMessage(client_mp));

  // 2. Write a message to |client_mp| (attaching nothing).
  WriteMessage(client_mp, "world!");

  // 4. Read a message from |client_mp|, which should have |mp1| attached.
  MojoHandle mp1;
  EXPECT_EQ("Bar", ReadMessageWithHandles(client_mp, &mp1, 1));

  // 5. Create a new message pipe (endpoints |mp2| and |mp3|).
  MojoHandle mp2, mp3;
  CreateMessagePipe(&mp2, &mp3);

  // 6. Write a message to |mp3|.
  WriteMessage(mp3, "baz");

  // 7. Close |mp3|.
  ASSERT_EQ(MOJO_RESULT_OK, MojoClose(mp3));

  // 8. Write a message to |mp1|, attaching |mp2|.
  WriteMessageWithHandles(mp1, "quux", &mp2, 1);
  mp2 = MOJO_HANDLE_INVALID;

  // 9. Read a message from |mp1|.
  EXPECT_EQ("FOO", ReadMessage(mp1));

  EXPECT_EQ("quit", ReadMessage(client_mp));

  // 10. Wait on |mp1| (which should eventually fail) and then close it.
  MojoHandleSignalsState state;
  ASSERT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
            MojoWait(mp1, MOJO_HANDLE_SIGNAL_READABLE,
                      MOJO_DEADLINE_INDEFINITE, &state));
  ASSERT_EQ(MOJO_HANDLE_SIGNAL_PEER_CLOSED, state.satisfied_signals);
  ASSERT_EQ(MOJO_HANDLE_SIGNAL_PEER_CLOSED, state.satisfiable_signals);
  ASSERT_EQ(MOJO_RESULT_OK, MojoClose(mp1));
}

// TODO(vtl): Test immediate write & close.
// TODO(vtl): Test broken-connection cases.

}  // namespace
}  // namespace edk
}  // namespace mojo
