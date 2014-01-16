// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(vtl): Enable this on non-POSIX once we have a non-POSIX implementation.
#include "build/build_config.h"
#if defined(OS_POSIX)

#include <stdint.h>

#include <string>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/threading/thread.h"
#include "mojo/common/test/multiprocess_test_base.h"
#include "mojo/system/channel.h"
#include "mojo/system/embedder/scoped_platform_handle.h"
#include "mojo/system/local_message_pipe_endpoint.h"
#include "mojo/system/message_pipe.h"
#include "mojo/system/proxy_message_pipe_endpoint.h"
#include "mojo/system/test_utils.h"
#include "mojo/system/waiter.h"

namespace mojo {
namespace system {
namespace {

class IOThreadWrapper {
 public:
  IOThreadWrapper() : io_thread_("io_thread") {}
  ~IOThreadWrapper() {
    CHECK(!channel_.get());
    CHECK(!io_thread_.IsRunning());
  }

  void PostTask(const tracked_objects::Location& from_here,
                const base::Closure& task) {
    task_runner()->PostTask(from_here, task);
  }

  void PostTaskAndWait(const tracked_objects::Location& from_here,
                       const base::Closure& task) {
    test::PostTaskAndWait(task_runner(), from_here, task);
  }

  void Init(embedder::ScopedPlatformHandle platform_handle,
            scoped_refptr<MessagePipe> mp) {
    io_thread_.StartWithOptions(
        base::Thread::Options(base::MessageLoop::TYPE_IO, 0));
    PostTask(FROM_HERE,
             base::Bind(&IOThreadWrapper::InitOnIOThread,
                        base::Unretained(this),
                        base::Passed(&platform_handle), mp));
  }

  void Shutdown() {
    PostTaskAndWait(FROM_HERE,
                    base::Bind(&IOThreadWrapper::ShutdownOnIOThread,
                               base::Unretained(this)));
    io_thread_.Stop();
  }

  bool is_initialized() const { return !!channel_.get(); }

  base::MessageLoop* message_loop() {
    return io_thread_.message_loop();
  }

  scoped_refptr<base::TaskRunner> task_runner() {
    return message_loop()->message_loop_proxy();
  }

 private:
  void InitOnIOThread(embedder::ScopedPlatformHandle platform_handle,
                      scoped_refptr<MessagePipe> mp) {
    CHECK_EQ(base::MessageLoop::current(), message_loop());
    CHECK(platform_handle.is_valid());

    // Create and initialize |Channel|.
    channel_ = new Channel();
    CHECK(channel_->Init(platform_handle.Pass()));

    // Attach the message pipe endpoint.
    // Note: On the "server" (parent process) side, we need not attach the
    // message pipe endpoint immediately. However, on the "client" (child
    // process) side, this *must* be done here -- otherwise, the |Channel| may
    // receive/process messages (which it can do as soon as it's hooked up to
    // the IO thread message loop, and that message loop runs) before the
    // message pipe endpoint is attached.
    CHECK_EQ(channel_->AttachMessagePipeEndpoint(mp, 1),
             Channel::kBootstrapEndpointId);
    channel_->RunMessagePipeEndpoint(Channel::kBootstrapEndpointId,
                                     Channel::kBootstrapEndpointId);
  }

  void ShutdownOnIOThread() {
    CHECK(channel_.get());
    channel_->Shutdown();
    channel_ = NULL;
  }

  base::Thread io_thread_;
  scoped_refptr<Channel> channel_;

  DISALLOW_COPY_AND_ASSIGN(IOThreadWrapper);
};

class MultiprocessMessagePipeTest : public mojo::test::MultiprocessTestBase {
 public:
  MultiprocessMessagePipeTest() {}
  virtual ~MultiprocessMessagePipeTest() {}

  virtual void TearDown() OVERRIDE {
    if (io_thread_wrapper_.is_initialized())
      io_thread_wrapper_.Shutdown();
    mojo::test::MultiprocessTestBase::TearDown();
  }

  void Init(scoped_refptr<MessagePipe> mp) {
    io_thread_wrapper_.Init(server_platform_handle.Pass(), mp);
  }

 private:
  IOThreadWrapper io_thread_wrapper_;

  DISALLOW_COPY_AND_ASSIGN(MultiprocessMessagePipeTest);
};

MojoResult WaitIfNecessary(scoped_refptr<MessagePipe> mp, MojoWaitFlags flags) {
  Waiter waiter;
  waiter.Init();

  MojoResult add_result = mp->AddWaiter(0, &waiter, flags, MOJO_RESULT_OK);
  if (add_result != MOJO_RESULT_OK) {
    return (add_result == MOJO_RESULT_ALREADY_EXISTS) ? MOJO_RESULT_OK :
                                                        add_result;
  }

  MojoResult wait_result = waiter.Wait(MOJO_DEADLINE_INDEFINITE);
  mp->RemoveWaiter(0, &waiter);
  return wait_result;
}

// For each message received, sends a reply message with the same contents
// repeated twice, until the other end is closed or it receives "quitquitquit"
// (which it doesn't reply to). It'll return the number of messages received,
// not including any "quitquitquit" message, modulo 100.
MOJO_MULTIPROCESS_TEST_CHILD_MAIN(EchoEcho) {
  IOThreadWrapper io_thread_wrapper;
  embedder::ScopedPlatformHandle client_platform_handle =
      MultiprocessMessagePipeTest::client_platform_handle.Pass();
  CHECK(client_platform_handle.is_valid());
  scoped_refptr<MessagePipe> mp(new MessagePipe(
      scoped_ptr<MessagePipeEndpoint>(new LocalMessagePipeEndpoint()),
      scoped_ptr<MessagePipeEndpoint>(new ProxyMessagePipeEndpoint())));
  io_thread_wrapper.Init(client_platform_handle.Pass(), mp);

  const std::string quitquitquit("quitquitquit");
  int rv = 0;
  for (;; rv = (rv + 1) % 100) {
    // Wait for our end of the message pipe to be readable.
    MojoResult result = WaitIfNecessary(mp, MOJO_WAIT_FLAG_READABLE);
    if (result != MOJO_RESULT_OK) {
      // It was closed, probably.
      CHECK_EQ(result, MOJO_RESULT_FAILED_PRECONDITION);
      break;
    }

    std::string read_buffer(1000, '\0');
    uint32_t read_buffer_size = static_cast<uint32_t>(read_buffer.size());
    CHECK_EQ(mp->ReadMessage(0,
                             &read_buffer[0], &read_buffer_size,
                             NULL, NULL,
                             MOJO_READ_MESSAGE_FLAG_NONE),
             MOJO_RESULT_OK);
    read_buffer.resize(read_buffer_size);
    VLOG(2) << "Child got: " << read_buffer;

    if (read_buffer == quitquitquit) {
      VLOG(2) << "Child quitting.";
      break;
    }

    std::string write_buffer = read_buffer + read_buffer;
    CHECK_EQ(mp->WriteMessage(0,
                              write_buffer.data(),
                              static_cast<uint32_t>(write_buffer.size()),
                              NULL,
                              MOJO_WRITE_MESSAGE_FLAG_NONE),
             MOJO_RESULT_OK);
  }


  mp->Close(0);
  io_thread_wrapper.Shutdown();
  return rv;
}

// Sends "hello" to child, and expects "hellohello" back.
TEST_F(MultiprocessMessagePipeTest, Basic) {
  StartChild("EchoEcho");

  scoped_refptr<MessagePipe> mp(new MessagePipe(
      scoped_ptr<MessagePipeEndpoint>(new LocalMessagePipeEndpoint()),
      scoped_ptr<MessagePipeEndpoint>(new ProxyMessagePipeEndpoint())));
  Init(mp);

  std::string hello("hello");
  EXPECT_EQ(MOJO_RESULT_OK,
            mp->WriteMessage(0,
                             hello.data(), static_cast<uint32_t>(hello.size()),
                             NULL,
                             MOJO_WRITE_MESSAGE_FLAG_NONE));

  EXPECT_EQ(MOJO_RESULT_OK, WaitIfNecessary(mp, MOJO_WAIT_FLAG_READABLE));

  std::string read_buffer(1000, '\0');
  uint32_t read_buffer_size = static_cast<uint32_t>(read_buffer.size());
  CHECK_EQ(mp->ReadMessage(0,
                           &read_buffer[0], &read_buffer_size,
                           NULL, NULL,
                           MOJO_READ_MESSAGE_FLAG_NONE),
           MOJO_RESULT_OK);
  read_buffer.resize(read_buffer_size);
  VLOG(2) << "Parent got: " << read_buffer;
  EXPECT_EQ(hello + hello, read_buffer);

  mp->Close(0);

  // We sent one message.
  EXPECT_EQ(1 % 100, WaitForChildShutdown());
}

// Sends a bunch of messages to the child. Expects them "repeated" back. Waits
// for the child to close its end before quitting.
TEST_F(MultiprocessMessagePipeTest, QueueMessages) {
  StartChild("EchoEcho");

  scoped_refptr<MessagePipe> mp(new MessagePipe(
      scoped_ptr<MessagePipeEndpoint>(new LocalMessagePipeEndpoint()),
      scoped_ptr<MessagePipeEndpoint>(new ProxyMessagePipeEndpoint())));
  Init(mp);

  static const size_t kNumMessages = 1001;
  for (size_t i = 0; i < kNumMessages; i++) {
    std::string write_buffer(i, 'A' + (i % 26));
    EXPECT_EQ(MOJO_RESULT_OK,
              mp->WriteMessage(0,
                               write_buffer.data(),
                               static_cast<uint32_t>(write_buffer.size()),
                               NULL,
                               MOJO_WRITE_MESSAGE_FLAG_NONE));
  }

  const std::string quitquitquit("quitquitquit");
  EXPECT_EQ(MOJO_RESULT_OK,
            mp->WriteMessage(0,
                             quitquitquit.data(),
                             static_cast<uint32_t>(quitquitquit.size()),
                             NULL,
                             MOJO_WRITE_MESSAGE_FLAG_NONE));

  for (size_t i = 0; i < kNumMessages; i++) {
    EXPECT_EQ(MOJO_RESULT_OK, WaitIfNecessary(mp, MOJO_WAIT_FLAG_READABLE));

    std::string read_buffer(kNumMessages * 2, '\0');
    uint32_t read_buffer_size = static_cast<uint32_t>(read_buffer.size());
    CHECK_EQ(mp->ReadMessage(0,
                             &read_buffer[0], &read_buffer_size,
                             NULL, NULL,
                             MOJO_READ_MESSAGE_FLAG_NONE),
             MOJO_RESULT_OK);
    read_buffer.resize(read_buffer_size);

    EXPECT_EQ(std::string(i * 2, 'A' + (i % 26)), read_buffer);
  }

  // Wait for it to become readable, which should fail (since we sent
  // "quitquitquit").
  EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
            WaitIfNecessary(mp, MOJO_WAIT_FLAG_READABLE));

  mp->Close(0);

  EXPECT_EQ(static_cast<int>(kNumMessages % 100), WaitForChildShutdown());
}

}  // namespace
}  // namespace system
}  // namespace mojo

#endif  // defined(OS_POSIX)
