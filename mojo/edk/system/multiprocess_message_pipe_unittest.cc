// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/files/scoped_temp_dir.h"
#include "base/location.h"
#include "base/logging.h"
#include "build/build_config.h"
#include "mojo/edk/embedder/embedder.h"
#include "mojo/edk/embedder/platform_channel_pair.h"
#include "mojo/edk/embedder/platform_shared_buffer.h"
#include "mojo/edk/embedder/scoped_platform_handle.h"
#include "mojo/edk/system/dispatcher.h"
#include "mojo/edk/system/message_pipe_test_utils.h"
#include "mojo/edk/system/platform_handle_dispatcher.h"
#include "mojo/edk/system/raw_channel.h"
#include "mojo/edk/system/shared_buffer_dispatcher.h"
#include "mojo/edk/system/test_utils.h"
#include "mojo/edk/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"


namespace mojo {
namespace edk {
namespace {

class MultiprocessMessagePipeTest
    : public test::MultiprocessMessagePipeTestBase {};

// For each message received, sends a reply message with the same contents
// repeated twice, until the other end is closed or it receives "quitquitquit"
// (which it doesn't reply to). It'll return the number of messages received,
// not including any "quitquitquit" message, modulo 100.
MOJO_MULTIPROCESS_TEST_CHILD_MAIN(EchoEcho) {
  ScopedPlatformHandle client_platform_handle =
      std::move(test::MultiprocessTestHelper::client_platform_handle);
  CHECK(client_platform_handle.is_valid());
  ScopedMessagePipeHandle mp =
      CreateMessagePipe(std::move(client_platform_handle));

  const std::string quitquitquit("quitquitquit");
  int rv = 0;
  for (;; rv = (rv + 1) % 100) {
    // Wait for our end of the message pipe to be readable.
    HandleSignalsState hss;
    MojoResult result =
        MojoWait(mp.get().value(), MOJO_HANDLE_SIGNAL_READABLE,
                 MOJO_DEADLINE_INDEFINITE, &hss);
    if (result != MOJO_RESULT_OK) {
      // It was closed, probably.
      CHECK_EQ(result, MOJO_RESULT_FAILED_PRECONDITION);
      CHECK_EQ(hss.satisfied_signals, MOJO_HANDLE_SIGNAL_PEER_CLOSED);
      CHECK_EQ(hss.satisfiable_signals, MOJO_HANDLE_SIGNAL_PEER_CLOSED);
      break;
    } else {
      CHECK((hss.satisfied_signals & MOJO_HANDLE_SIGNAL_READABLE));
      CHECK((hss.satisfiable_signals & MOJO_HANDLE_SIGNAL_READABLE));
    }

    std::string read_buffer(1000, '\0');
    uint32_t read_buffer_size = static_cast<uint32_t>(read_buffer.size());
    CHECK_EQ(MojoReadMessage(mp.get().value(), &read_buffer[0],
                             &read_buffer_size, nullptr,
                             0, MOJO_READ_MESSAGE_FLAG_NONE),
             MOJO_RESULT_OK);
    read_buffer.resize(read_buffer_size);
    VLOG(2) << "Child got: " << read_buffer;

    if (read_buffer == quitquitquit) {
      VLOG(2) << "Child quitting.";
      break;
    }

    std::string write_buffer = read_buffer + read_buffer;
    CHECK_EQ(MojoWriteMessage(mp.get().value(), write_buffer.data(),
                              static_cast<uint32_t>(write_buffer.size()),
                              nullptr, 0u, MOJO_WRITE_MESSAGE_FLAG_NONE),
             MOJO_RESULT_OK);
  }

   return rv;
}

// Sends "hello" to child, and expects "hellohello" back.
#if defined(OS_ANDROID)
// Android multi-process tests are not executing the new process. This is flaky.
#define MAYBE_Basic DISABLED_Basic
#else
#define MAYBE_Basic Basic
#endif  // defined(OS_ANDROID)
TEST_F(MultiprocessMessagePipeTest, MAYBE_Basic) {
  helper()->StartChild("EchoEcho");

  ScopedMessagePipeHandle mp =
      CreateMessagePipe(std::move(helper()->server_platform_handle));

  std::string hello("hello");
  ASSERT_EQ(MOJO_RESULT_OK,
            MojoWriteMessage(mp.get().value(), hello.data(),
                             static_cast<uint32_t>(hello.size()), nullptr, 0u,
                             MOJO_WRITE_MESSAGE_FLAG_NONE));

  HandleSignalsState hss;
  ASSERT_EQ(MOJO_RESULT_OK,
            MojoWait(mp.get().value(), MOJO_HANDLE_SIGNAL_READABLE,
               MOJO_DEADLINE_INDEFINITE, &hss));
  // The child may or may not have closed its end of the message pipe and died
  // (and we may or may not know it yet), so our end may or may not appear as
  // writable.
  EXPECT_TRUE((hss.satisfied_signals & MOJO_HANDLE_SIGNAL_READABLE));
  EXPECT_TRUE((hss.satisfiable_signals & MOJO_HANDLE_SIGNAL_READABLE));

  std::string read_buffer(1000, '\0');
  uint32_t read_buffer_size = static_cast<uint32_t>(read_buffer.size());
  CHECK_EQ(MojoReadMessage(mp.get().value(), &read_buffer[0],
                           &read_buffer_size, nullptr, 0,
                           MOJO_READ_MESSAGE_FLAG_NONE),
           MOJO_RESULT_OK);
  read_buffer.resize(read_buffer_size);
  VLOG(2) << "Parent got: " << read_buffer;
  ASSERT_EQ(hello + hello, read_buffer);

  MojoClose(mp.release().value());

  // We sent one message.
  ASSERT_EQ(1 % 100, helper()->WaitForChildShutdown());
}

// Sends a bunch of messages to the child. Expects them "repeated" back. Waits
// for the child to close its end before quitting.
#if defined(OS_ANDROID)
// Android multi-process tests are not executing the new process. This is flaky.
#define MAYBE_QueueMessages DISABLED_QueueMessages
#else
#define MAYBE_QueueMessages QueueMessages
#endif  // defined(OS_ANDROID)
TEST_F(MultiprocessMessagePipeTest, MAYBE_QueueMessages) {
  helper()->StartChild("EchoEcho");

  ScopedMessagePipeHandle mp =
      CreateMessagePipe(std::move(helper()->server_platform_handle));

  static const size_t kNumMessages = 1001;
  for (size_t i = 0; i < kNumMessages; i++) {
    std::string write_buffer(i, 'A' + (i % 26));
    ASSERT_EQ(MOJO_RESULT_OK,
            MojoWriteMessage(mp.get().value(), write_buffer.data(),
                             static_cast<uint32_t>(write_buffer.size()),
                             nullptr, 0u, MOJO_WRITE_MESSAGE_FLAG_NONE));
  }

  for (size_t i = 0; i < kNumMessages; i++) {
    HandleSignalsState hss;
    ASSERT_EQ(MOJO_RESULT_OK,
              MojoWait(mp.get().value(), MOJO_HANDLE_SIGNAL_READABLE,
                       MOJO_DEADLINE_INDEFINITE, &hss));
    // The child may or may not have closed its end of the message pipe and died
    // (and we may or may not know it yet), so our end may or may not appear as
    // writable.
    ASSERT_TRUE((hss.satisfied_signals & MOJO_HANDLE_SIGNAL_READABLE));
    ASSERT_TRUE((hss.satisfiable_signals & MOJO_HANDLE_SIGNAL_READABLE));

    std::string read_buffer(kNumMessages * 2, '\0');
    uint32_t read_buffer_size = static_cast<uint32_t>(read_buffer.size());
    ASSERT_EQ(MojoReadMessage(mp.get().value(), &read_buffer[0],
                             &read_buffer_size, nullptr, 0,
                             MOJO_READ_MESSAGE_FLAG_NONE),
             MOJO_RESULT_OK);
    read_buffer.resize(read_buffer_size);

    ASSERT_EQ(std::string(i * 2, 'A' + (i % 26)), read_buffer);
  }

  const std::string quitquitquit("quitquitquit");
  ASSERT_EQ(MOJO_RESULT_OK,
            MojoWriteMessage(mp.get().value(), quitquitquit.data(),
                             static_cast<uint32_t>(quitquitquit.size()),
                             nullptr, 0u, MOJO_WRITE_MESSAGE_FLAG_NONE));

  // Wait for it to become readable, which should fail (since we sent
  // "quitquitquit").
  HandleSignalsState hss;
  ASSERT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
            MojoWait(mp.get().value(), MOJO_HANDLE_SIGNAL_READABLE,
                     MOJO_DEADLINE_INDEFINITE, &hss));
  ASSERT_EQ(MOJO_HANDLE_SIGNAL_PEER_CLOSED, hss.satisfied_signals);
  ASSERT_EQ(MOJO_HANDLE_SIGNAL_PEER_CLOSED, hss.satisfiable_signals);

  ASSERT_EQ(static_cast<int>(kNumMessages % 100),
            helper()->WaitForChildShutdown());
}

MOJO_MULTIPROCESS_TEST_CHILD_MAIN(CheckSharedBuffer) {
  ScopedPlatformHandle client_platform_handle =
      std::move(test::MultiprocessTestHelper::client_platform_handle);
  CHECK(client_platform_handle.is_valid());
  ScopedMessagePipeHandle mp =
      CreateMessagePipe(std::move(client_platform_handle));

  // Wait for the first message from our parent.
  HandleSignalsState hss;
  CHECK_EQ(MojoWait(mp.get().value(), MOJO_HANDLE_SIGNAL_READABLE,
               MOJO_DEADLINE_INDEFINITE, &hss),
           MOJO_RESULT_OK);
  // In this test, the parent definitely doesn't close its end of the message
  // pipe before we do.
  CHECK_EQ(hss.satisfied_signals,
           MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_WRITABLE);
  CHECK_EQ(hss.satisfiable_signals, MOJO_HANDLE_SIGNAL_READABLE |
                                    MOJO_HANDLE_SIGNAL_WRITABLE |
                                    MOJO_HANDLE_SIGNAL_PEER_CLOSED);

  // It should have a shared buffer.
  std::string read_buffer(100, '\0');
  uint32_t num_bytes = static_cast<uint32_t>(read_buffer.size());
  MojoHandle handles[10];
  uint32_t num_handlers = MOJO_ARRAYSIZE(handles);  // Maximum number to receive
  CHECK_EQ(MojoReadMessage(mp.get().value(), &read_buffer[0],
                           &num_bytes, &handles[0],
                           &num_handlers, MOJO_READ_MESSAGE_FLAG_NONE),
           MOJO_RESULT_OK);
  read_buffer.resize(num_bytes);
  CHECK_EQ(read_buffer, std::string("go 1"));
  CHECK_EQ(num_handlers, 1u);

  // Make a mapping.
  void* buffer;
  CHECK_EQ(MojoMapBuffer(handles[0], 0, 100, &buffer,
                         MOJO_CREATE_SHARED_BUFFER_OPTIONS_FLAG_NONE),
           MOJO_RESULT_OK);

  // Write some stuff to the shared buffer.
  static const char kHello[] = "hello";
  memcpy(buffer, kHello, sizeof(kHello));

  // We should be able to close the dispatcher now.
  MojoClose(handles[0]);

  // And send a message to signal that we've written stuff.
  const std::string go2("go 2");
  CHECK_EQ(MojoWriteMessage(mp.get().value(), go2.data(),
                            static_cast<uint32_t>(go2.size()), nullptr, 0u,
                            MOJO_WRITE_MESSAGE_FLAG_NONE),
           MOJO_RESULT_OK);

  // Now wait for our parent to send us a message.
  hss = HandleSignalsState();
  CHECK_EQ(MojoWait(mp.get().value(), MOJO_HANDLE_SIGNAL_READABLE,
               MOJO_DEADLINE_INDEFINITE, &hss),
           MOJO_RESULT_OK);
  CHECK_EQ(hss.satisfied_signals,
           MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_WRITABLE);
  CHECK_EQ(hss.satisfiable_signals, MOJO_HANDLE_SIGNAL_READABLE |
                                        MOJO_HANDLE_SIGNAL_WRITABLE |
                                        MOJO_HANDLE_SIGNAL_PEER_CLOSED);

  read_buffer = std::string(100, '\0');
  num_bytes = static_cast<uint32_t>(read_buffer.size());
  CHECK_EQ(MojoReadMessage(mp.get().value(), &read_buffer[0], &num_bytes,
                           nullptr, 0, MOJO_READ_MESSAGE_FLAG_NONE),
           MOJO_RESULT_OK);
  read_buffer.resize(num_bytes);
  CHECK_EQ(read_buffer, std::string("go 3"));

  // It should have written something to the shared buffer.
  static const char kWorld[] = "world!!!";
  CHECK_EQ(memcmp(buffer, kWorld, sizeof(kWorld)), 0);

  // And we're done.

  return 0;
}

#if defined(OS_ANDROID)
// Android multi-process tests are not executing the new process. This is flaky.
#define MAYBE_SharedBufferPassing DISABLED_SharedBufferPassing
#else
#define MAYBE_SharedBufferPassing SharedBufferPassing
#endif
TEST_F(MultiprocessMessagePipeTest, MAYBE_SharedBufferPassing) {
  helper()->StartChild("CheckSharedBuffer");

  ScopedMessagePipeHandle mp =
      CreateMessagePipe(std::move(helper()->server_platform_handle));

  // Make a shared buffer.
  MojoCreateSharedBufferOptions options;
  options.struct_size = sizeof(options);
  options.flags = MOJO_CREATE_SHARED_BUFFER_OPTIONS_FLAG_NONE;

  MojoHandle shared_buffer;
  ASSERT_EQ(MOJO_RESULT_OK,
            MojoCreateSharedBuffer(&options, 100, &shared_buffer));

  // Send the shared buffer.
  const std::string go1("go 1");

  MojoHandle duplicated_shared_buffer;
  ASSERT_EQ(MOJO_RESULT_OK,
            MojoDuplicateBufferHandle(
                shared_buffer,
                nullptr,
                &duplicated_shared_buffer));
  MojoHandle handles[1];
  handles[0] = duplicated_shared_buffer;
  ASSERT_EQ(MOJO_RESULT_OK,
            MojoWriteMessage(mp.get().value(), &go1[0],
                             static_cast<uint32_t>(go1.size()), &handles[0],
                             MOJO_ARRAYSIZE(handles),
                             MOJO_WRITE_MESSAGE_FLAG_NONE));

  // Wait for a message from the child.
  HandleSignalsState hss;
  ASSERT_EQ(MOJO_RESULT_OK,
            MojoWait(mp.get().value(), MOJO_HANDLE_SIGNAL_READABLE,
                     MOJO_DEADLINE_INDEFINITE, &hss));
  EXPECT_TRUE((hss.satisfied_signals & MOJO_HANDLE_SIGNAL_READABLE));
  EXPECT_TRUE((hss.satisfiable_signals & MOJO_HANDLE_SIGNAL_READABLE));

  std::string read_buffer(100, '\0');
  uint32_t num_bytes = static_cast<uint32_t>(read_buffer.size());
  ASSERT_EQ(MOJO_RESULT_OK,
            MojoReadMessage(mp.get().value(), &read_buffer[0],
                             &num_bytes, nullptr, 0,
                             MOJO_READ_MESSAGE_FLAG_NONE));
  read_buffer.resize(num_bytes);
  ASSERT_EQ(std::string("go 2"), read_buffer);

  // After we get it, the child should have written something to the shared
  // buffer.
  static const char kHello[] = "hello";
  void* buffer;
  CHECK_EQ(MojoMapBuffer(shared_buffer, 0, 100, &buffer,
                         MOJO_CREATE_SHARED_BUFFER_OPTIONS_FLAG_NONE),
           MOJO_RESULT_OK);
  ASSERT_EQ(0, memcmp(buffer, kHello, sizeof(kHello)));

  // Now we'll write some stuff to the shared buffer.
  static const char kWorld[] = "world!!!";
  memcpy(buffer, kWorld, sizeof(kWorld));

  // And send a message to signal that we've written stuff.
  const std::string go3("go 3");
  ASSERT_EQ(MOJO_RESULT_OK,
            MojoWriteMessage(mp.get().value(), &go3[0],
                             static_cast<uint32_t>(go3.size()), nullptr, 0u,
                             MOJO_WRITE_MESSAGE_FLAG_NONE));

  // Wait for |mp| to become readable, which should fail.
  hss = HandleSignalsState();
  ASSERT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
            MojoWait(mp.get().value(), MOJO_HANDLE_SIGNAL_READABLE,
                     MOJO_DEADLINE_INDEFINITE, &hss));
  ASSERT_EQ(MOJO_HANDLE_SIGNAL_PEER_CLOSED, hss.satisfied_signals);
  ASSERT_EQ(MOJO_HANDLE_SIGNAL_PEER_CLOSED, hss.satisfiable_signals);

  MojoClose(mp.release().value());

  ASSERT_EQ(0, helper()->WaitForChildShutdown());
}

MOJO_MULTIPROCESS_TEST_CHILD_MAIN(CheckPlatformHandleFile) {
  ScopedPlatformHandle client_platform_handle =
      std::move(test::MultiprocessTestHelper::client_platform_handle);
  CHECK(client_platform_handle.is_valid());
  ScopedMessagePipeHandle mp =
      CreateMessagePipe(std::move(client_platform_handle));

  HandleSignalsState hss;
  CHECK_EQ(MojoWait(mp.get().value(), MOJO_HANDLE_SIGNAL_READABLE,
               MOJO_DEADLINE_INDEFINITE, &hss),
           MOJO_RESULT_OK);
  CHECK_EQ(hss.satisfied_signals,
           MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_WRITABLE);
  CHECK_EQ(hss.satisfiable_signals, MOJO_HANDLE_SIGNAL_READABLE |
                                        MOJO_HANDLE_SIGNAL_WRITABLE |
                                        MOJO_HANDLE_SIGNAL_PEER_CLOSED);

  std::string read_buffer(100, '\0');
  uint32_t num_bytes = static_cast<uint32_t>(read_buffer.size());
  MojoHandle handles[255];  // Maximum number to receive.
  uint32_t num_handlers = MOJO_ARRAYSIZE(handles);

  CHECK_EQ(MojoReadMessage(mp.get().value(), &read_buffer[0],
                           &num_bytes, &handles[0],
                           &num_handlers, MOJO_READ_MESSAGE_FLAG_NONE),
           MOJO_RESULT_OK);
  MojoClose(mp.release().value());

  read_buffer.resize(num_bytes);
  char hello[32];
  int num_handles = 0;
  sscanf(read_buffer.c_str(), "%s %d", hello, &num_handles);
  CHECK_EQ(std::string("hello"), std::string(hello));
  CHECK_GT(num_handles, 0);

  for (int i = 0; i < num_handles; ++i) {
    ScopedPlatformHandle h;
    CHECK_EQ(PassWrappedPlatformHandle(
                handles[i], &h),
             MOJO_RESULT_OK);
    CHECK(h.is_valid());
    MojoClose(handles[i]);

    base::ScopedFILE fp(test::FILEFromPlatformHandle(std::move(h), "r"));
    CHECK(fp);
    std::string fread_buffer(100, '\0');
    size_t bytes_read =
        fread(&fread_buffer[0], 1, fread_buffer.size(), fp.get());
    fread_buffer.resize(bytes_read);
    CHECK_EQ(fread_buffer, "world");
  }

  return 0;
}

class MultiprocessMessagePipeTestWithPipeCount
    : public test::MultiprocessMessagePipeTestBase,
      public testing::WithParamInterface<size_t> {};

TEST_P(MultiprocessMessagePipeTestWithPipeCount, PlatformHandlePassing) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  helper()->StartChild("CheckPlatformHandleFile");
  ScopedMessagePipeHandle mp =
      CreateMessagePipe(std::move(helper()->server_platform_handle));

  std::vector<MojoHandle> handles;

  size_t pipe_count = GetParam();
  for (size_t i = 0; i < pipe_count; ++i) {
    base::FilePath unused;
    base::ScopedFILE fp(
        CreateAndOpenTemporaryFileInDir(temp_dir.path(), &unused));
    const std::string world("world");
    CHECK_EQ(fwrite(&world[0], 1, world.size(), fp.get()), world.size());
    fflush(fp.get());
    rewind(fp.get());
    MojoHandle handle;
    ASSERT_EQ(
        CreatePlatformHandleWrapper(
            ScopedPlatformHandle(test::PlatformHandleFromFILE(std::move(fp))),
            &handle),
        MOJO_RESULT_OK);
    handles.push_back(handle);
  }

  char message[128];
  sprintf(message, "hello %d", static_cast<int>(pipe_count));
  ASSERT_EQ(MOJO_RESULT_OK,
            MojoWriteMessage(mp.get().value(), message,
                             static_cast<uint32_t>(strlen(message)),
                             &handles[0], static_cast<uint32_t>(handles.size()),
                             MOJO_WRITE_MESSAGE_FLAG_NONE));

  // Wait for it to become readable, which should fail.
  HandleSignalsState hss;
  ASSERT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
            MojoWait(mp.get().value(), MOJO_HANDLE_SIGNAL_READABLE,
                     MOJO_DEADLINE_INDEFINITE, &hss));
  ASSERT_EQ(MOJO_HANDLE_SIGNAL_PEER_CLOSED, hss.satisfied_signals);
  ASSERT_EQ(MOJO_HANDLE_SIGNAL_PEER_CLOSED, hss.satisfiable_signals);

  MojoClose(mp.release().value());

  ASSERT_EQ(0, helper()->WaitForChildShutdown());
}

// Android multi-process tests are not executing the new process. This is flaky.
#if !defined(OS_ANDROID)
INSTANTIATE_TEST_CASE_P(PipeCount,
                        MultiprocessMessagePipeTestWithPipeCount,
                        testing::Values(1u, 128u, 140u));
#endif

MOJO_MULTIPROCESS_TEST_CHILD_MAIN(CheckMessagePipe) {
  ScopedPlatformHandle client_platform_handle =
      std::move(test::MultiprocessTestHelper::client_platform_handle);
  CHECK(client_platform_handle.is_valid());

  ScopedMessagePipeHandle mp =
      CreateMessagePipe(std::move(client_platform_handle));

  // Wait for the first message from our parent.
  HandleSignalsState hss;
  CHECK_EQ(MojoWait(mp.get().value(), MOJO_HANDLE_SIGNAL_READABLE,
               MOJO_DEADLINE_INDEFINITE, &hss),
           MOJO_RESULT_OK);
  // In this test, the parent definitely doesn't close its end of the message
  // pipe before we do.
  CHECK_EQ(hss.satisfied_signals,
           MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_WRITABLE);
  CHECK_EQ(hss.satisfiable_signals, MOJO_HANDLE_SIGNAL_READABLE |
                                    MOJO_HANDLE_SIGNAL_WRITABLE |
                                    MOJO_HANDLE_SIGNAL_PEER_CLOSED);

  // It should have a message pipe.
  MojoHandle handles[10];
  uint32_t num_handlers = MOJO_ARRAYSIZE(handles);
  CHECK_EQ(MojoReadMessage(mp.get().value(), nullptr,
                           nullptr, &handles[0],
                           &num_handlers, MOJO_READ_MESSAGE_FLAG_NONE),
           MOJO_RESULT_OK);
  CHECK_EQ(num_handlers, 1u);

  // Read data from the received message pipe.
  CHECK_EQ(MojoWait(handles[0], MOJO_HANDLE_SIGNAL_READABLE,
               MOJO_DEADLINE_INDEFINITE, &hss),
           MOJO_RESULT_OK);
  CHECK_EQ(hss.satisfied_signals,
           MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_WRITABLE);
  CHECK_EQ(hss.satisfiable_signals, MOJO_HANDLE_SIGNAL_READABLE |
                                    MOJO_HANDLE_SIGNAL_WRITABLE |
                                    MOJO_HANDLE_SIGNAL_PEER_CLOSED);

  std::string read_buffer(100, '\0');
  uint32_t read_buffer_size = static_cast<uint32_t>(read_buffer.size());
  CHECK_EQ(MojoReadMessage(handles[0], &read_buffer[0],
                           &read_buffer_size, nullptr,
                           0, MOJO_READ_MESSAGE_FLAG_NONE),
           MOJO_RESULT_OK);
  read_buffer.resize(read_buffer_size);
  CHECK_EQ(read_buffer, std::string("hello"));

  // Now write some data into the message pipe.
  std::string write_buffer = "world";
  CHECK_EQ(MojoWriteMessage(handles[0], write_buffer.data(),
                            static_cast<uint32_t>(write_buffer.size()), nullptr,
                            0u, MOJO_WRITE_MESSAGE_FLAG_NONE),
           MOJO_RESULT_OK);
  MojoClose(handles[0]);
  return 0;
}

#if defined(OS_ANDROID)
// Android multi-process tests are not executing the new process. This is flaky.
#define MAYBE_MessagePipePassing DISABLED_MessagePipePassing
#else
#define MAYBE_MessagePipePassing MessagePipePassing
#endif
TEST_F(MultiprocessMessagePipeTest, MAYBE_MessagePipePassing) {
  helper()->StartChild("CheckMessagePipe");

  ScopedMessagePipeHandle mp =
      CreateMessagePipe(std::move(helper()->server_platform_handle));
  MojoCreateSharedBufferOptions options;
  options.struct_size = sizeof(options);
  options.flags = MOJO_CREATE_SHARED_BUFFER_OPTIONS_FLAG_NONE;

  MojoHandle mp1, mp2;
  ASSERT_EQ(MOJO_RESULT_OK,
            MojoCreateMessagePipe(nullptr, &mp1, &mp2));

  // Write a string into one end of the new message pipe and send the other end.
  const std::string hello("hello");
  ASSERT_EQ(MOJO_RESULT_OK,
            MojoWriteMessage(mp1, &hello[0],
                             static_cast<uint32_t>(hello.size()), nullptr, 0,
                             MOJO_WRITE_MESSAGE_FLAG_NONE));
  ASSERT_EQ(MOJO_RESULT_OK,
            MojoWriteMessage(mp.get().value(), nullptr, 0, &mp2, 1,
                             MOJO_WRITE_MESSAGE_FLAG_NONE));

  // Wait for a message from the child.
  HandleSignalsState hss;
  ASSERT_EQ(MOJO_RESULT_OK,
            MojoWait(mp1, MOJO_HANDLE_SIGNAL_READABLE,
                     MOJO_DEADLINE_INDEFINITE, &hss));
  EXPECT_TRUE((hss.satisfied_signals & MOJO_HANDLE_SIGNAL_READABLE));
  EXPECT_TRUE((hss.satisfiable_signals & MOJO_HANDLE_SIGNAL_READABLE));

  std::string read_buffer(100, '\0');
  uint32_t read_buffer_size = static_cast<uint32_t>(read_buffer.size());
  CHECK_EQ(MojoReadMessage(mp1, &read_buffer[0],
                           &read_buffer_size, nullptr,
                           0, MOJO_READ_MESSAGE_FLAG_NONE),
           MOJO_RESULT_OK);
  read_buffer.resize(read_buffer_size);
  CHECK_EQ(read_buffer, std::string("world"));

  MojoClose(mp1);
  MojoClose(mp.release().value());

  ASSERT_EQ(0, helper()->WaitForChildShutdown());
}

// Like above test, but verifies passing the other MP handle works as well.
#if defined(OS_ANDROID)
// Android multi-process tests are not executing the new process. This is flaky.
#define MAYBE_MessagePipeTwoPassing DISABLED_MessagePipeTwoPassing
#else
#define MAYBE_MessagePipeTwoPassing MessagePipeTwoPassing
#endif
TEST_F(MultiprocessMessagePipeTest, MAYBE_MessagePipeTwoPassing) {
  helper()->StartChild("CheckMessagePipe");

  ScopedMessagePipeHandle mp =
      CreateMessagePipe(std::move(helper()->server_platform_handle));

  MojoHandle mp1, mp2;
  ASSERT_EQ(MOJO_RESULT_OK,
            MojoCreateMessagePipe(nullptr, &mp2, &mp1));

  // Write a string into one end of the new message pipe and send the other end.
  const std::string hello("hello");
  ASSERT_EQ(MOJO_RESULT_OK,
            MojoWriteMessage(mp1, &hello[0],
                             static_cast<uint32_t>(hello.size()), nullptr, 0u,
                             MOJO_WRITE_MESSAGE_FLAG_NONE));
  ASSERT_EQ(MOJO_RESULT_OK,
            MojoWriteMessage(mp.get().value(), nullptr, 0u, &mp2, 1u,
                             MOJO_WRITE_MESSAGE_FLAG_NONE));

  // Wait for a message from the child.
  HandleSignalsState hss;
  ASSERT_EQ(MOJO_RESULT_OK,
            MojoWait(mp1, MOJO_HANDLE_SIGNAL_READABLE,
                     MOJO_DEADLINE_INDEFINITE, &hss));
  EXPECT_TRUE((hss.satisfied_signals & MOJO_HANDLE_SIGNAL_READABLE));
  EXPECT_TRUE((hss.satisfiable_signals & MOJO_HANDLE_SIGNAL_READABLE));

  std::string read_buffer(100, '\0');
  uint32_t read_buffer_size = static_cast<uint32_t>(read_buffer.size());
  CHECK_EQ(MojoReadMessage(mp1, &read_buffer[0],
                           &read_buffer_size, nullptr,
                           0, MOJO_READ_MESSAGE_FLAG_NONE),
           MOJO_RESULT_OK);
  read_buffer.resize(read_buffer_size);
  CHECK_EQ(read_buffer, std::string("world"));

  MojoClose(mp.release().value());

  ASSERT_EQ(0, helper()->WaitForChildShutdown());
}

MOJO_MULTIPROCESS_TEST_CHILD_MAIN(DataPipeConsumer) {
  ScopedPlatformHandle client_platform_handle =
      std::move(test::MultiprocessTestHelper::client_platform_handle);
  CHECK(client_platform_handle.is_valid());

  ScopedMessagePipeHandle mp =
      CreateMessagePipe(std::move(client_platform_handle));

  // Wait for the first message from our parent.
  HandleSignalsState hss;
  CHECK_EQ(MojoWait(mp.get().value(), MOJO_HANDLE_SIGNAL_READABLE,
               MOJO_DEADLINE_INDEFINITE, &hss),
           MOJO_RESULT_OK);
  // In this test, the parent definitely doesn't close its end of the message
  // pipe before we do.
  CHECK_EQ(hss.satisfied_signals,
           MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_WRITABLE);
  CHECK_EQ(hss.satisfiable_signals, MOJO_HANDLE_SIGNAL_READABLE |
                                    MOJO_HANDLE_SIGNAL_WRITABLE |
                                    MOJO_HANDLE_SIGNAL_PEER_CLOSED);

  // It should have a message pipe.
  MojoHandle handles[10];
  uint32_t num_handlers = MOJO_ARRAYSIZE(handles);
  CHECK_EQ(MojoReadMessage(mp.get().value(), nullptr,
                           nullptr, &handles[0],
                           &num_handlers, MOJO_READ_MESSAGE_FLAG_NONE),
           MOJO_RESULT_OK);
  CHECK_EQ(num_handlers, 1u);

  // Read data from the received message pipe.
  CHECK_EQ(MojoWait(handles[0], MOJO_HANDLE_SIGNAL_READABLE,
               MOJO_DEADLINE_INDEFINITE, &hss),
           MOJO_RESULT_OK);
  CHECK_EQ(hss.satisfied_signals,
           MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_WRITABLE);
  CHECK_EQ(hss.satisfiable_signals, MOJO_HANDLE_SIGNAL_READABLE |
                                    MOJO_HANDLE_SIGNAL_WRITABLE |
                                    MOJO_HANDLE_SIGNAL_PEER_CLOSED);

  std::string read_buffer(100, '\0');
  uint32_t read_buffer_size = static_cast<uint32_t>(read_buffer.size());
  CHECK_EQ(MojoReadMessage(handles[0], &read_buffer[0],
                           &read_buffer_size, nullptr,
                           0, MOJO_READ_MESSAGE_FLAG_NONE),
           MOJO_RESULT_OK);
  read_buffer.resize(read_buffer_size);
  CHECK_EQ(read_buffer, std::string("hello"));

  // Now write some data into the message pipe.
  std::string write_buffer = "world";
  CHECK_EQ(MojoWriteMessage(handles[0], write_buffer.data(),
                            static_cast<uint32_t>(write_buffer.size()),
                            nullptr, 0u, MOJO_WRITE_MESSAGE_FLAG_NONE),
            MOJO_RESULT_OK);
  MojoClose(handles[0]);
  return 0;
}

#if defined(OS_ANDROID)
// Android multi-process tests are not executing the new process. This is flaky.
#define MAYBE_DataPipeConsumer DISABLED_DataPipeConsumer
#else
#define MAYBE_DataPipeConsumer DataPipeConsumer
#endif
TEST_F(MultiprocessMessagePipeTest, MAYBE_DataPipeConsumer) {
  helper()->StartChild("DataPipeConsumer");

  ScopedMessagePipeHandle mp =
      CreateMessagePipe(std::move(helper()->server_platform_handle));
  MojoCreateSharedBufferOptions options;
  options.struct_size = sizeof(options);
  options.flags = MOJO_CREATE_SHARED_BUFFER_OPTIONS_FLAG_NONE;

  MojoHandle mp1, mp2;
  ASSERT_EQ(MOJO_RESULT_OK,
            MojoCreateMessagePipe(nullptr, &mp2, &mp1));

  // Write a string into one end of the new message pipe and send the other end.
  const std::string hello("hello");
  ASSERT_EQ(MOJO_RESULT_OK,
            MojoWriteMessage(mp1, &hello[0],
                             static_cast<uint32_t>(hello.size()), nullptr, 0u,
                             MOJO_WRITE_MESSAGE_FLAG_NONE));
  ASSERT_EQ(MOJO_RESULT_OK,
            MojoWriteMessage(mp.get().value(), nullptr, 0, &mp2, 1u,
                             MOJO_WRITE_MESSAGE_FLAG_NONE));

  // Wait for a message from the child.
  HandleSignalsState hss;
  ASSERT_EQ(MOJO_RESULT_OK,
            MojoWait(mp1, MOJO_HANDLE_SIGNAL_READABLE,
                     MOJO_DEADLINE_INDEFINITE, &hss));
  EXPECT_TRUE((hss.satisfied_signals & MOJO_HANDLE_SIGNAL_READABLE));
  EXPECT_TRUE((hss.satisfiable_signals & MOJO_HANDLE_SIGNAL_READABLE));

  std::string read_buffer(100, '\0');
  uint32_t read_buffer_size = static_cast<uint32_t>(read_buffer.size());
  CHECK_EQ(MojoReadMessage(mp1, &read_buffer[0],
                           &read_buffer_size, nullptr,
                           0, MOJO_READ_MESSAGE_FLAG_NONE),
           MOJO_RESULT_OK);
  read_buffer.resize(read_buffer_size);
  CHECK_EQ(read_buffer, std::string("world"));

  MojoClose(mp1);
  MojoClose(mp.release().value());

  ASSERT_EQ(0, helper()->WaitForChildShutdown());
}

}  // namespace
}  // namespace edk
}  // namespace mojo
