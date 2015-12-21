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
#include "mojo/edk/test/multiprocess_test_helper.h"
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

typedef testing::Test EmbedderTest;

TEST_F(EmbedderTest, ChannelBasic) {
  MojoHandle server_mp, client_mp;
  ASSERT_EQ(MOJO_RESULT_OK,
            MojoCreateMessagePipe(nullptr, &server_mp, &client_mp));

  // We can write to a message pipe handle immediately.
  const char kHello[] = "hello";

  size_t write_size = sizeof(kHello);
  const char* write_buffer = kHello;
  ASSERT_EQ(MOJO_RESULT_OK,
            MojoWriteMessage(server_mp, write_buffer,
                             static_cast<uint32_t>(write_size), nullptr, 0,
                             MOJO_WRITE_MESSAGE_FLAG_NONE));

  // Now wait for the other side to become readable.
  MojoHandleSignalsState state;
  ASSERT_EQ(MOJO_RESULT_OK, MojoWait(client_mp, MOJO_HANDLE_SIGNAL_READABLE,
                                     MOJO_DEADLINE_INDEFINITE, &state));
  ASSERT_EQ(kSignalReadadableWritable, state.satisfied_signals);
  ASSERT_EQ(kSignalAll, state.satisfiable_signals);

  char read_buffer[1000] = {};
  uint32_t num_bytes = static_cast<uint32_t>(sizeof(read_buffer));
  ASSERT_EQ(MOJO_RESULT_OK,
            MojoReadMessage(client_mp, read_buffer, &num_bytes, nullptr,
                            nullptr, MOJO_READ_MESSAGE_FLAG_NONE));
  ASSERT_EQ(write_size, num_bytes);
  EXPECT_STREQ(kHello, read_buffer);

  ASSERT_EQ(MOJO_RESULT_OK, MojoClose(server_mp));
  ASSERT_EQ(MOJO_RESULT_OK, MojoClose(client_mp));
}

// Test sending a MP which has read messages out of the OS pipe but which have
// not been consumed using MojoReadMessage yet.
TEST_F(EmbedderTest, SendReadableMessagePipe) {
  MojoHandle server_mp, client_mp;
  ASSERT_EQ(MOJO_RESULT_OK,
            MojoCreateMessagePipe(nullptr, &server_mp, &client_mp));

  MojoHandle server_mp2, client_mp2;
  MojoCreateMessagePipeOptions options;
  options.struct_size = sizeof(MojoCreateMessagePipeOptions);
  options.flags = MOJO_CREATE_MESSAGE_PIPE_OPTIONS_FLAG_TRANSFERABLE;
  ASSERT_EQ(MOJO_RESULT_OK,
            MojoCreateMessagePipe(&options, &server_mp2, &client_mp2));

  // Write to server2 and wait for client2 to be readable before sending it.
  // client2's MessagePipeDispatcher will have the message below in its
  // message_queue_. For extra measures, also verify that this pending message
  // can contain a message pipe.
  MojoHandle server_mp3, client_mp3;
  ASSERT_EQ(MOJO_RESULT_OK,
            MojoCreateMessagePipe(nullptr, &server_mp3, &client_mp3));
  const char kHello[] = "hello";
  size_t write_size;
  const char* write_buffer;
  write_buffer = kHello;
  write_size = sizeof(kHello);
  ASSERT_EQ(MOJO_RESULT_OK,
            MojoWriteMessage(server_mp2, write_buffer,
                             static_cast<uint32_t>(write_size), &client_mp3, 1,
                             MOJO_WRITE_MESSAGE_FLAG_NONE));

  MojoHandleSignalsState state;
  ASSERT_EQ(MOJO_RESULT_OK, MojoWait(client_mp2, MOJO_HANDLE_SIGNAL_READABLE,
                                     MOJO_DEADLINE_INDEFINITE, &state));
  ASSERT_EQ(kSignalReadadableWritable, state.satisfied_signals);
  ASSERT_EQ(kSignalAll, state.satisfiable_signals);

  // Now send client2
  ASSERT_EQ(MOJO_RESULT_OK,
            MojoWriteMessage(server_mp, write_buffer,
                             static_cast<uint32_t>(write_size), &client_mp2, 1,
                             MOJO_WRITE_MESSAGE_FLAG_NONE));


  ASSERT_EQ(MOJO_RESULT_OK, MojoWait(client_mp, MOJO_HANDLE_SIGNAL_READABLE,
                                     MOJO_DEADLINE_INDEFINITE, &state));
  ASSERT_EQ(kSignalReadadableWritable, state.satisfied_signals);
  ASSERT_EQ(kSignalAll, state.satisfiable_signals);

  char read_buffer[20000] = {};
  uint32_t num_bytes = static_cast<uint32_t>(sizeof(read_buffer));
  MojoHandle ports[10];
  uint32_t num_ports = arraysize(ports);
  ASSERT_EQ(MOJO_RESULT_OK,
            MojoReadMessage(client_mp, read_buffer, &num_bytes, &ports[0],
                            &num_ports, MOJO_READ_MESSAGE_FLAG_NONE));
  ASSERT_EQ(write_size, num_bytes);
  EXPECT_STREQ(kHello, read_buffer);
  ASSERT_EQ(1u, num_ports);


  client_mp2 = ports[0];
  ASSERT_EQ(MOJO_RESULT_OK, MojoWait(client_mp2, MOJO_HANDLE_SIGNAL_READABLE,
                                     MOJO_DEADLINE_INDEFINITE, &state));
  ASSERT_EQ(kSignalReadadableWritable, state.satisfied_signals);
  ASSERT_EQ(kSignalAll, state.satisfiable_signals);


  ASSERT_EQ(MOJO_RESULT_OK,
            MojoReadMessage(client_mp2, read_buffer, &num_bytes, &client_mp3,
                            &num_ports, MOJO_READ_MESSAGE_FLAG_NONE));
  ASSERT_EQ(write_size, num_bytes);
  EXPECT_STREQ(kHello, read_buffer);
  ASSERT_EQ(1u, num_ports);


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
  ASSERT_EQ(MOJO_RESULT_OK,
            MojoCreateMessagePipe(nullptr, &server_mp, &client_mp));

  MojoCreateMessagePipeOptions options;
  options.struct_size = sizeof(MojoCreateMessagePipeOptions);
  options.flags = MOJO_CREATE_MESSAGE_PIPE_OPTIONS_FLAG_TRANSFERABLE;
  MojoHandle server_mp2, client_mp2;
  ASSERT_EQ(MOJO_RESULT_OK,
            MojoCreateMessagePipe(&options, &server_mp2, &client_mp2));

  static const size_t kNumMessages = 1001;
  for (size_t i = 0; i < kNumMessages; i++) {
    std::string write_buffer(i, 'A' + (i % 26));
    ASSERT_EQ(MOJO_RESULT_OK,
              MojoWriteMessage(client_mp2, write_buffer.data(),
                               static_cast<uint32_t>(write_buffer.size()),
                               nullptr, 0, MOJO_WRITE_MESSAGE_FLAG_NONE));
  }

  // Now send client2.
  ASSERT_EQ(MOJO_RESULT_OK,
            MojoWriteMessage(server_mp, nullptr, 0, &client_mp2, 1,
                             MOJO_WRITE_MESSAGE_FLAG_NONE));
  client_mp2 = MOJO_HANDLE_INVALID;

  // Read client2 just so we can close it later.
  MojoHandleSignalsState state;
  ASSERT_EQ(MOJO_RESULT_OK, MojoWait(client_mp, MOJO_HANDLE_SIGNAL_READABLE,
                                     MOJO_DEADLINE_INDEFINITE, &state));
  ASSERT_EQ(kSignalReadadableWritable, state.satisfied_signals);
  ASSERT_EQ(kSignalAll, state.satisfiable_signals);

  uint32_t num_handles = 1;
  ASSERT_EQ(MOJO_RESULT_OK, MojoReadMessage(client_mp, nullptr, 0, &client_mp2,
                                            &num_handles,
                                            MOJO_READ_MESSAGE_FLAG_NONE));
  ASSERT_EQ(1u, num_handles);

  // Now verify that all the messages that were written were sent correctly.
  for (size_t i = 0; i < kNumMessages; i++) {
    ASSERT_EQ(MOJO_RESULT_OK, MojoWait(server_mp2, MOJO_HANDLE_SIGNAL_READABLE,
                                       MOJO_DEADLINE_INDEFINITE, &state));
    ASSERT_EQ(kSignalReadadableWritable, state.satisfied_signals);
    ASSERT_EQ(kSignalAll, state.satisfiable_signals);

    std::string read_buffer(kNumMessages * 2, '\0');
    uint32_t read_buffer_size = static_cast<uint32_t>(read_buffer.size());
    ASSERT_EQ(MOJO_RESULT_OK, MojoReadMessage(server_mp2, &read_buffer[0],
                                              &read_buffer_size, nullptr, 0,
                                              MOJO_READ_MESSAGE_FLAG_NONE));
    read_buffer.resize(read_buffer_size);

    ASSERT_EQ(std::string(i, 'A' + (i % 26)), read_buffer);
  }

  ASSERT_EQ(MOJO_RESULT_OK, MojoClose(server_mp2));
  ASSERT_EQ(MOJO_RESULT_OK, MojoClose(client_mp2));
  ASSERT_EQ(MOJO_RESULT_OK, MojoClose(server_mp));
  ASSERT_EQ(MOJO_RESULT_OK, MojoClose(client_mp));
}

TEST_F(EmbedderTest, ChannelsHandlePassing) {
  MojoHandle server_mp, client_mp;
  ASSERT_EQ(MOJO_RESULT_OK,
            MojoCreateMessagePipe(nullptr, &server_mp, &client_mp));
  EXPECT_NE(server_mp, MOJO_HANDLE_INVALID);
  EXPECT_NE(client_mp, MOJO_HANDLE_INVALID);

  MojoHandle h0, h1;
  ASSERT_EQ(MOJO_RESULT_OK, MojoCreateMessagePipe(nullptr, &h0, &h1));

  // Write a message to |h0| (attaching nothing).
  const char kHello[] = "hello";
  ASSERT_EQ(MOJO_RESULT_OK,
            MojoWriteMessage(h0, kHello, static_cast<uint32_t>(sizeof(kHello)),
                             nullptr, 0, MOJO_WRITE_MESSAGE_FLAG_NONE));

  // Write one message to |server_mp|, attaching |h1|.
  const char kWorld[] = "world!!!";
  ASSERT_EQ(
      MOJO_RESULT_OK,
      MojoWriteMessage(server_mp, kWorld, static_cast<uint32_t>(sizeof(kWorld)),
                       &h1, 1, MOJO_WRITE_MESSAGE_FLAG_NONE));
  h1 = MOJO_HANDLE_INVALID;

  // Write another message to |h0|.
  const char kFoo[] = "foo";
  ASSERT_EQ(MOJO_RESULT_OK,
            MojoWriteMessage(h0, kFoo, static_cast<uint32_t>(sizeof(kFoo)),
                             nullptr, 0, MOJO_WRITE_MESSAGE_FLAG_NONE));

  // Wait for |client_mp| to become readable.
  MojoHandleSignalsState state;
  ASSERT_EQ(MOJO_RESULT_OK, MojoWait(client_mp, MOJO_HANDLE_SIGNAL_READABLE,
                                     MOJO_DEADLINE_INDEFINITE, &state));
  ASSERT_EQ(kSignalReadadableWritable, state.satisfied_signals);
  ASSERT_EQ(kSignalAll, state.satisfiable_signals);

  // Read a message from |client_mp|.
  char buffer[1000] = {};
  uint32_t num_bytes = static_cast<uint32_t>(sizeof(buffer));
  MojoHandle handles[10] = {};
  uint32_t num_handles = MOJO_ARRAYSIZE(handles);
  ASSERT_EQ(MOJO_RESULT_OK,
            MojoReadMessage(client_mp, buffer, &num_bytes, handles,
                            &num_handles, MOJO_READ_MESSAGE_FLAG_NONE));
  ASSERT_EQ(sizeof(kWorld), num_bytes);
  EXPECT_STREQ(kWorld, buffer);
  ASSERT_EQ(1u, num_handles);
  EXPECT_NE(handles[0], MOJO_HANDLE_INVALID);
  h1 = handles[0];

  // Wait for |h1| to become readable.
  ASSERT_EQ(MOJO_RESULT_OK, MojoWait(h1, MOJO_HANDLE_SIGNAL_READABLE,
                                     MOJO_DEADLINE_INDEFINITE, &state));
  ASSERT_EQ(kSignalReadadableWritable, state.satisfied_signals);
  ASSERT_EQ(kSignalAll, state.satisfiable_signals);

  // Read a message from |h1|.
  memset(buffer, 0, sizeof(buffer));
  num_bytes = static_cast<uint32_t>(sizeof(buffer));
  memset(handles, 0, sizeof(handles));
  num_handles = MOJO_ARRAYSIZE(handles);
  ASSERT_EQ(MOJO_RESULT_OK,
            MojoReadMessage(h1, buffer, &num_bytes, handles, &num_handles,
                            MOJO_READ_MESSAGE_FLAG_NONE));
  ASSERT_EQ(sizeof(kHello), num_bytes);
  EXPECT_STREQ(kHello, buffer);
  ASSERT_EQ(0u, num_handles);

  // Wait for |h1| to become readable (again).
  ASSERT_EQ(MOJO_RESULT_OK, MojoWait(h1, MOJO_HANDLE_SIGNAL_READABLE,
                                     MOJO_DEADLINE_INDEFINITE, &state));
  ASSERT_EQ(kSignalReadadableWritable, state.satisfied_signals);
  ASSERT_EQ(kSignalAll, state.satisfiable_signals);

  // Read the second message from |h1|.
  memset(buffer, 0, sizeof(buffer));
  num_bytes = static_cast<uint32_t>(sizeof(buffer));
  ASSERT_EQ(MOJO_RESULT_OK,
            MojoReadMessage(h1, buffer, &num_bytes, nullptr, nullptr,
                            MOJO_READ_MESSAGE_FLAG_NONE));
  ASSERT_EQ(sizeof(kFoo), num_bytes);
  EXPECT_STREQ(kFoo, buffer);

  // Write a message to |h1|.
  const char kBarBaz[] = "barbaz";
  ASSERT_EQ(
      MOJO_RESULT_OK,
      MojoWriteMessage(h1, kBarBaz, static_cast<uint32_t>(sizeof(kBarBaz)),
                       nullptr, 0, MOJO_WRITE_MESSAGE_FLAG_NONE));

  // Wait for |h0| to become readable.
  ASSERT_EQ(MOJO_RESULT_OK, MojoWait(h0, MOJO_HANDLE_SIGNAL_READABLE,
                                     MOJO_DEADLINE_INDEFINITE, &state));
  ASSERT_EQ(kSignalReadadableWritable, state.satisfied_signals);
  ASSERT_EQ(kSignalAll, state.satisfiable_signals);

  // Read a message from |h0|.
  memset(buffer, 0, sizeof(buffer));
  num_bytes = static_cast<uint32_t>(sizeof(buffer));
  ASSERT_EQ(MOJO_RESULT_OK,
            MojoReadMessage(h0, buffer, &num_bytes, nullptr, nullptr,
                            MOJO_READ_MESSAGE_FLAG_NONE));
  ASSERT_EQ(sizeof(kBarBaz), num_bytes);
  EXPECT_STREQ(kBarBaz, buffer);

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
  test::MultiprocessTestHelper multiprocess_test_helper;
  multiprocess_test_helper.StartChild("MultiprocessChannelsClient");

  {
    MojoHandle server_mp =
        CreateMessagePipe(
            std::move(multiprocess_test_helper.server_platform_handle))
            .release()
            .value();

    // 1. Write a message to |server_mp| (attaching nothing).
    const char kHello[] = "hello";
    ASSERT_EQ(MOJO_RESULT_OK,
              MojoWriteMessage(server_mp, kHello,
                               static_cast<uint32_t>(sizeof(kHello)), nullptr,
                               0, MOJO_WRITE_MESSAGE_FLAG_NONE));

    // TODO(vtl): If the scope were ended immediately here (maybe after closing
    // |server_mp|), we die with a fatal error in |Channel::HandleLocalError()|.

    // 2. Read a message from |server_mp|.
    MojoHandleSignalsState state;
    ASSERT_EQ(MOJO_RESULT_OK, MojoWait(server_mp, MOJO_HANDLE_SIGNAL_READABLE,
                                       MOJO_DEADLINE_INDEFINITE, &state));
    ASSERT_EQ(kSignalReadadableWritable, state.satisfied_signals);
    ASSERT_EQ(kSignalAll, state.satisfiable_signals);

    char buffer[1000] = {};
    uint32_t num_bytes = static_cast<uint32_t>(sizeof(buffer));
    ASSERT_EQ(MOJO_RESULT_OK,
              MojoReadMessage(server_mp, buffer, &num_bytes, nullptr, nullptr,
                              MOJO_READ_MESSAGE_FLAG_NONE));
    const char kWorld[] = "world!";
    ASSERT_EQ(sizeof(kWorld), num_bytes);
    EXPECT_STREQ(kWorld, buffer);

    // Create a new message pipe (endpoints |mp0| and |mp1|).
    MojoHandle mp0, mp1;
    ASSERT_EQ(MOJO_RESULT_OK, MojoCreateMessagePipe(nullptr, &mp0, &mp1));

    // 3. Write something to |mp0|.
    const char kFoo[] = "FOO";
    ASSERT_EQ(MOJO_RESULT_OK,
              MojoWriteMessage(mp0, kFoo, static_cast<uint32_t>(sizeof(kFoo)),
                               nullptr, 0, MOJO_WRITE_MESSAGE_FLAG_NONE));

    // 4. Write a message to |server_mp|, attaching |mp1|.
    const char kBar[] = "Bar";
    ASSERT_EQ(
        MOJO_RESULT_OK,
        MojoWriteMessage(server_mp, kBar, static_cast<uint32_t>(sizeof(kBar)),
                         &mp1, 1, MOJO_WRITE_MESSAGE_FLAG_NONE));
    mp1 = MOJO_HANDLE_INVALID;

    // 5. Close |server_mp|.
    ASSERT_EQ(MOJO_RESULT_OK, MojoClose(server_mp));

    // 9. Read a message from |mp0|, which should have |mp2| attached.
    ASSERT_EQ(MOJO_RESULT_OK, MojoWait(mp0, MOJO_HANDLE_SIGNAL_READABLE,
                                       MOJO_DEADLINE_INDEFINITE, &state));
    ASSERT_EQ(kSignalReadadableWritable, state.satisfied_signals);
    ASSERT_EQ(kSignalAll, state.satisfiable_signals);

    memset(buffer, 0, sizeof(buffer));
    num_bytes = static_cast<uint32_t>(sizeof(buffer));
    MojoHandle mp2 = MOJO_HANDLE_INVALID;
    uint32_t num_handles = 1;
    ASSERT_EQ(MOJO_RESULT_OK,
              MojoReadMessage(mp0, buffer, &num_bytes, &mp2, &num_handles,
                              MOJO_READ_MESSAGE_FLAG_NONE));
    const char kQuux[] = "quux";
    ASSERT_EQ(sizeof(kQuux), num_bytes);
    EXPECT_STREQ(kQuux, buffer);
    ASSERT_EQ(1u, num_handles);
    EXPECT_NE(mp2, MOJO_HANDLE_INVALID);

    // 7. Read a message from |mp2|.
    ASSERT_EQ(MOJO_RESULT_OK, MojoWait(mp2, MOJO_HANDLE_SIGNAL_PEER_CLOSED,
                                       MOJO_DEADLINE_INDEFINITE, &state));
    ASSERT_EQ(MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
              state.satisfied_signals);
    ASSERT_EQ(MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
              state.satisfiable_signals);

    memset(buffer, 0, sizeof(buffer));
    num_bytes = static_cast<uint32_t>(sizeof(buffer));
    ASSERT_EQ(MOJO_RESULT_OK,
              MojoReadMessage(mp2, buffer, &num_bytes, nullptr, nullptr,
                              MOJO_READ_MESSAGE_FLAG_NONE));
    const char kBaz[] = "baz";
    ASSERT_EQ(sizeof(kBaz), num_bytes);
    EXPECT_STREQ(kBaz, buffer);

    // 10. Close |mp0|.
    ASSERT_EQ(MOJO_RESULT_OK, MojoClose(mp0));

// 12. Wait on |mp2| (which should eventually fail) and then close it.
// TODO(vtl): crbug.com/351768
#if 0
    ASSERT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
              MojoWait(mp2, MOJO_HANDLE_SIGNAL_READABLE,
                       MOJO_DEADLINE_INDEFINITE,
                       &state));
    ASSERT_EQ(MOJO_HANDLE_SIGNAL_NONE, state.satisfied_signals);
    ASSERT_EQ(MOJO_HANDLE_SIGNAL_NONE, state.satisfiable_signals);
#endif
    ASSERT_EQ(MOJO_RESULT_OK, MojoClose(mp2));
  }

  EXPECT_TRUE(multiprocess_test_helper.WaitForChildTestShutdown());
}

MOJO_MULTIPROCESS_TEST_CHILD_TEST(MultiprocessChannelsClient) {
  ScopedPlatformHandle client_platform_handle =
      std::move(test::MultiprocessTestHelper::client_platform_handle);
  EXPECT_TRUE(client_platform_handle.is_valid());

  MojoHandle client_mp =
      CreateMessagePipe(std::move(client_platform_handle)).release().value();

  // 1. Read the first message from |client_mp|.
  MojoHandleSignalsState state;
  ASSERT_EQ(MOJO_RESULT_OK, MojoWait(client_mp, MOJO_HANDLE_SIGNAL_READABLE,
                                      MOJO_DEADLINE_INDEFINITE, &state));
  ASSERT_EQ(kSignalReadadableWritable, state.satisfied_signals);
  ASSERT_EQ(kSignalAll, state.satisfiable_signals);

  char buffer[1000] = {};
  uint32_t num_bytes = static_cast<uint32_t>(sizeof(buffer));
  ASSERT_EQ(MOJO_RESULT_OK,
            MojoReadMessage(client_mp, buffer, &num_bytes, nullptr, nullptr,
                            MOJO_READ_MESSAGE_FLAG_NONE));
  const char kHello[] = "hello";
  ASSERT_EQ(sizeof(kHello), num_bytes);
  EXPECT_STREQ(kHello, buffer);

  // 2. Write a message to |client_mp| (attaching nothing).
  const char kWorld[] = "world!";
  ASSERT_EQ(MOJO_RESULT_OK,
            MojoWriteMessage(client_mp, kWorld,
                              static_cast<uint32_t>(sizeof(kWorld)), nullptr,
                              0, MOJO_WRITE_MESSAGE_FLAG_NONE));

  // 4. Read a message from |client_mp|, which should have |mp1| attached.
  ASSERT_EQ(MOJO_RESULT_OK, MojoWait(client_mp, MOJO_HANDLE_SIGNAL_READABLE,
                                      MOJO_DEADLINE_INDEFINITE, &state));
  // The other end of the handle may or may not be closed at this point, so we
  // can't test MOJO_HANDLE_SIGNAL_WRITABLE or MOJO_HANDLE_SIGNAL_PEER_CLOSED.
  ASSERT_EQ(MOJO_HANDLE_SIGNAL_READABLE,
            state.satisfied_signals & MOJO_HANDLE_SIGNAL_READABLE);
  ASSERT_EQ(MOJO_HANDLE_SIGNAL_READABLE,
            state.satisfiable_signals & MOJO_HANDLE_SIGNAL_READABLE);
  // TODO(vtl): If the scope were to end here (and |client_mp| closed), we'd
  // die (again due to |Channel::HandleLocalError()|).
  memset(buffer, 0, sizeof(buffer));
  num_bytes = static_cast<uint32_t>(sizeof(buffer));
  MojoHandle mp1 = MOJO_HANDLE_INVALID;
  uint32_t num_handles = 1;
  ASSERT_EQ(MOJO_RESULT_OK,
            MojoReadMessage(client_mp, buffer, &num_bytes, &mp1, &num_handles,
                            MOJO_READ_MESSAGE_FLAG_NONE));
  const char kBar[] = "Bar";
  ASSERT_EQ(sizeof(kBar), num_bytes);
  EXPECT_STREQ(kBar, buffer);
  ASSERT_EQ(1u, num_handles);
  EXPECT_NE(mp1, MOJO_HANDLE_INVALID);
  // TODO(vtl): If the scope were to end here (and the two handles closed),
  // we'd die due to |Channel::RunRemoteMessagePipeEndpoint()| not handling
  // write errors (assuming the parent had closed the pipe).

  // 6. Close |client_mp|.
  ASSERT_EQ(MOJO_RESULT_OK, MojoClose(client_mp));

  // Create a new message pipe (endpoints |mp2| and |mp3|).
  MojoHandle mp2, mp3;
  ASSERT_EQ(MOJO_RESULT_OK, MojoCreateMessagePipe(nullptr, &mp2, &mp3));

  // 7. Write a message to |mp3|.
  const char kBaz[] = "baz";
  ASSERT_EQ(MOJO_RESULT_OK,
            MojoWriteMessage(mp3, kBaz, static_cast<uint32_t>(sizeof(kBaz)),
                              nullptr, 0, MOJO_WRITE_MESSAGE_FLAG_NONE));

  // 8. Close |mp3|.
  ASSERT_EQ(MOJO_RESULT_OK, MojoClose(mp3));

  // 9. Write a message to |mp1|, attaching |mp2|.
  const char kQuux[] = "quux";
  ASSERT_EQ(MOJO_RESULT_OK,
            MojoWriteMessage(mp1, kQuux, static_cast<uint32_t>(sizeof(kQuux)),
                              &mp2, 1, MOJO_WRITE_MESSAGE_FLAG_NONE));
  mp2 = MOJO_HANDLE_INVALID;

  // 3. Read a message from |mp1|.
  ASSERT_EQ(MOJO_RESULT_OK, MojoWait(mp1, MOJO_HANDLE_SIGNAL_READABLE,
                                      MOJO_DEADLINE_INDEFINITE, &state));
  ASSERT_EQ(kSignalReadadableWritable, state.satisfied_signals);
  ASSERT_EQ(kSignalAll, state.satisfiable_signals);

  memset(buffer, 0, sizeof(buffer));
  num_bytes = static_cast<uint32_t>(sizeof(buffer));
  ASSERT_EQ(MOJO_RESULT_OK,
            MojoReadMessage(mp1, buffer, &num_bytes, nullptr, nullptr,
                            MOJO_READ_MESSAGE_FLAG_NONE));
  const char kFoo[] = "FOO";
  ASSERT_EQ(sizeof(kFoo), num_bytes);
  EXPECT_STREQ(kFoo, buffer);

  // 11. Wait on |mp1| (which should eventually fail) and then close it.
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
