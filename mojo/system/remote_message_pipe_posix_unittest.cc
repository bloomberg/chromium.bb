// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(vtl): The POSIX-specific bits have been factored out. Apply this test to
// non-POSIX once we have a non-POSIX implementation.

#include <stdint.h>
#include <string.h>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/threading/thread.h"
#include "mojo/system/channel.h"
#include "mojo/system/local_message_pipe_endpoint.h"
#include "mojo/system/message_pipe.h"
#include "mojo/system/platform_channel.h"
#include "mojo/system/proxy_message_pipe_endpoint.h"
#include "mojo/system/test_utils.h"
#include "mojo/system/waiter.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace system {
namespace {

class RemoteMessagePipeTest : public testing::Test {
 public:
  RemoteMessagePipeTest() : io_thread_("io_thread") {
  }

  virtual ~RemoteMessagePipeTest() {
  }

  virtual void SetUp() OVERRIDE {
    io_thread_.StartWithOptions(
        base::Thread::Options(base::MessageLoop::TYPE_IO, 0));

    test::PostTaskAndWait(io_thread_task_runner(),
                          FROM_HERE,
                          base::Bind(&RemoteMessagePipeTest::SetUpOnIOThread,
                                     base::Unretained(this)));
  }

  virtual void TearDown() OVERRIDE {
    test::PostTaskAndWait(io_thread_task_runner(),
                          FROM_HERE,
                          base::Bind(&RemoteMessagePipeTest::TearDownOnIOThread,
                                     base::Unretained(this)));
    io_thread_.Stop();
  }

  // This connects MP 0, port 1 and MP 1, port 0 (leaving MP 0, port 0 and MP 1,
  // port 1 as the user-visible endpoints) to channel 0 and 1, respectively. MP
  // 0, port 1 and MP 1, port 0 must have |ProxyMessagePipeEndpoint|s.
  void ConnectMessagePipes(scoped_refptr<MessagePipe> mp_0,
                           scoped_refptr<MessagePipe> mp_1) {
    test::PostTaskAndWait(
        io_thread_task_runner(),
        FROM_HERE,
        base::Bind(&RemoteMessagePipeTest::ConnectMessagePipesOnIOThread,
                   base::Unretained(this), mp_0, mp_1));
  }

  // This connects |mp|'s port |channel_index ^ 1| to channel |channel_index|.
  // It assumes/requires that this is the bootstrap case, i.e., that the
  // endpoint IDs are both/will both be |Channel::kBootstrapEndpointId|. This
  // returns *without* waiting for it to finish connecting.
  void BootstrapMessagePipeNoWait(unsigned channel_index,
                                  scoped_refptr<MessagePipe> mp) {
    io_thread_task_runner()->PostTask(
        FROM_HERE,
        base::Bind(&RemoteMessagePipeTest::BootstrapMessagePipeOnIOThread,
                   base::Unretained(this), channel_index, mp));
  }

 protected:
  base::MessageLoop* io_thread_message_loop() {
    return io_thread_.message_loop();
  }

  scoped_refptr<base::TaskRunner> io_thread_task_runner() {
    return io_thread_message_loop()->message_loop_proxy();
  }

 private:
  void SetUpOnIOThread() {
    CHECK_EQ(base::MessageLoop::current(), io_thread_message_loop());

    scoped_ptr<PlatformServerChannel> server_channel(
        PlatformServerChannel::Create("channel"));
    CHECK(server_channel.get());
    CHECK(server_channel->is_valid());
    scoped_ptr<PlatformClientChannel> client_channel(
        server_channel->CreateClientChannel());
    CHECK(client_channel.get());
    CHECK(client_channel->is_valid());

    platform_channels_[0] = server_channel.PassAs<PlatformChannel>();
    platform_channels_[1] = client_channel.PassAs<PlatformChannel>();
  }

  void CreateAndInitChannel(unsigned channel_index) {
    CHECK_EQ(base::MessageLoop::current(), io_thread_message_loop());
    CHECK(channel_index == 0 || channel_index == 1);
    CHECK(!channels_[channel_index].get());

    channels_[channel_index] = new Channel();
    CHECK(channels_[channel_index]->Init(
        platform_channels_[channel_index]->PassHandle()));
  }

  void ConnectMessagePipesOnIOThread(scoped_refptr<MessagePipe> mp_0,
                                     scoped_refptr<MessagePipe> mp_1) {
    CHECK_EQ(base::MessageLoop::current(), io_thread_message_loop());

    if (!channels_[0].get())
      CreateAndInitChannel(0);
    if (!channels_[1].get())
      CreateAndInitChannel(1);

    MessageInTransit::EndpointId local_id_0 =
        channels_[0]->AttachMessagePipeEndpoint(mp_0, 1);
    MessageInTransit::EndpointId local_id_1 =
        channels_[1]->AttachMessagePipeEndpoint(mp_1, 0);

    channels_[0]->RunMessagePipeEndpoint(local_id_0, local_id_1);
    channels_[1]->RunMessagePipeEndpoint(local_id_1, local_id_0);
  }

  void BootstrapMessagePipeOnIOThread(unsigned channel_index,
                                      scoped_refptr<MessagePipe> mp) {
    CHECK_EQ(base::MessageLoop::current(), io_thread_message_loop());
    CHECK(channel_index == 0 || channel_index == 1);

    unsigned port = channel_index ^ 1u;

    // Important: If we don't boot
    CreateAndInitChannel(channel_index);
    CHECK_EQ(channels_[channel_index]->AttachMessagePipeEndpoint(mp, port),
             Channel::kBootstrapEndpointId);
    channels_[channel_index]->RunMessagePipeEndpoint(
        Channel::kBootstrapEndpointId, Channel::kBootstrapEndpointId);
  }

  void TearDownOnIOThread() {
    if (channels_[0].get()) {
      channels_[0]->Shutdown();
      channels_[0] = NULL;
    }
    if (channels_[1].get()) {
      channels_[1]->Shutdown();
      channels_[1] = NULL;
    }
  }

  base::Thread io_thread_;
  scoped_ptr<PlatformChannel> platform_channels_[2];
  scoped_refptr<Channel> channels_[2];

  DISALLOW_COPY_AND_ASSIGN(RemoteMessagePipeTest);
};

TEST_F(RemoteMessagePipeTest, Basic) {
  const char hello[] = "hello";
  const char world[] = "world!!!1!!!1!";
  char buffer[100] = { 0 };
  uint32_t buffer_size = static_cast<uint32_t>(sizeof(buffer));
  Waiter waiter;

  // Connect message pipes. MP 0, port 1 will be attached to channel 0 and
  // connected to MP 1, port 0, which will be attached to channel 1. This leaves
  // MP 0, port 0 and MP 1, port 1 as the "user-facing" endpoints.

  scoped_refptr<MessagePipe> mp_0(new MessagePipe(
      scoped_ptr<MessagePipeEndpoint>(new LocalMessagePipeEndpoint()),
      scoped_ptr<MessagePipeEndpoint>(new ProxyMessagePipeEndpoint())));
  scoped_refptr<MessagePipe> mp_1(new MessagePipe(
      scoped_ptr<MessagePipeEndpoint>(new ProxyMessagePipeEndpoint()),
      scoped_ptr<MessagePipeEndpoint>(new LocalMessagePipeEndpoint())));
  ConnectMessagePipes(mp_0, mp_1);

  // Write in one direction: MP 0, port 0 -> ... -> MP 1, port 1.

  // Prepare to wait on MP 1, port 1. (Add the waiter now. Otherwise, if we do
  // it later, it might already be readable.)
  waiter.Init();
  EXPECT_EQ(MOJO_RESULT_OK,
            mp_1->AddWaiter(1, &waiter, MOJO_WAIT_FLAG_READABLE, 123));

  // Write to MP 0, port 0.
  EXPECT_EQ(MOJO_RESULT_OK,
            mp_0->WriteMessage(0,
                               hello, sizeof(hello),
                               NULL,
                               MOJO_WRITE_MESSAGE_FLAG_NONE));

  // Wait.
  EXPECT_EQ(123, waiter.Wait(MOJO_DEADLINE_INDEFINITE));
  mp_1->RemoveWaiter(1, &waiter);

  // Read from MP 1, port 1.
  EXPECT_EQ(MOJO_RESULT_OK,
            mp_1->ReadMessage(1,
                              buffer, &buffer_size,
                              NULL, NULL,
                              MOJO_READ_MESSAGE_FLAG_NONE));
  EXPECT_EQ(sizeof(hello), static_cast<size_t>(buffer_size));
  EXPECT_EQ(0, strcmp(buffer, hello));

  // Write in the other direction: MP 1, port 1 -> ... -> MP 0, port 0.

  waiter.Init();
  EXPECT_EQ(MOJO_RESULT_OK,
            mp_0->AddWaiter(0, &waiter, MOJO_WAIT_FLAG_READABLE, 456));

  EXPECT_EQ(MOJO_RESULT_OK,
            mp_1->WriteMessage(1,
                               world, sizeof(world),
                               NULL,
                               MOJO_WRITE_MESSAGE_FLAG_NONE));

  EXPECT_EQ(456, waiter.Wait(MOJO_DEADLINE_INDEFINITE));
  mp_0->RemoveWaiter(0, &waiter);

  buffer_size = static_cast<uint32_t>(sizeof(buffer));
  EXPECT_EQ(MOJO_RESULT_OK,
            mp_0->ReadMessage(0,
                              buffer, &buffer_size,
                              NULL, NULL,
                              MOJO_READ_MESSAGE_FLAG_NONE));
  EXPECT_EQ(sizeof(world), static_cast<size_t>(buffer_size));
  EXPECT_EQ(0, strcmp(buffer, world));

  // Close MP 0, port 0.
  mp_0->Close(0);

  // Try to wait for MP 1, port 1 to become readable. This will eventually fail
  // when it realizes that MP 0, port 0 has been closed. (It may also fail
  // immediately.)
  waiter.Init();
  MojoResult result = mp_1->AddWaiter(1, &waiter, MOJO_WAIT_FLAG_READABLE, 789);
  if (result == MOJO_RESULT_OK) {
    EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
              waiter.Wait(MOJO_DEADLINE_INDEFINITE));
    mp_1->RemoveWaiter(1, &waiter);
  } else {
    EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION, result);
  }

  // And MP 1, port 1.
  mp_1->Close(1);
}

TEST_F(RemoteMessagePipeTest, Multiplex) {
  const char hello[] = "hello";
  const char world[] = "world!!!1!!!1!";
  char buffer[100] = { 0 };
  uint32_t buffer_size = static_cast<uint32_t>(sizeof(buffer));
  Waiter waiter;

  // Connect message pipes as in the |Basic| test.

  scoped_refptr<MessagePipe> mp_0(new MessagePipe(
      scoped_ptr<MessagePipeEndpoint>(new LocalMessagePipeEndpoint()),
      scoped_ptr<MessagePipeEndpoint>(new ProxyMessagePipeEndpoint())));
  scoped_refptr<MessagePipe> mp_1(new MessagePipe(
      scoped_ptr<MessagePipeEndpoint>(new ProxyMessagePipeEndpoint()),
      scoped_ptr<MessagePipeEndpoint>(new LocalMessagePipeEndpoint())));
  ConnectMessagePipes(mp_0, mp_1);

  // Now put another message pipe on the channel.

  scoped_refptr<MessagePipe> mp_2(new MessagePipe(
      scoped_ptr<MessagePipeEndpoint>(new LocalMessagePipeEndpoint()),
      scoped_ptr<MessagePipeEndpoint>(new ProxyMessagePipeEndpoint())));
  scoped_refptr<MessagePipe> mp_3(new MessagePipe(
      scoped_ptr<MessagePipeEndpoint>(new ProxyMessagePipeEndpoint()),
      scoped_ptr<MessagePipeEndpoint>(new LocalMessagePipeEndpoint())));
  ConnectMessagePipes(mp_2, mp_3);

  // Write: MP 2, port 0 -> MP 3, port 1.

  waiter.Init();
  EXPECT_EQ(MOJO_RESULT_OK,
            mp_3->AddWaiter(1, &waiter, MOJO_WAIT_FLAG_READABLE, 789));

  EXPECT_EQ(MOJO_RESULT_OK,
            mp_2->WriteMessage(0,
                               hello, sizeof(hello),
                               NULL,
                               MOJO_WRITE_MESSAGE_FLAG_NONE));

  EXPECT_EQ(789, waiter.Wait(MOJO_DEADLINE_INDEFINITE));
  mp_3->RemoveWaiter(1, &waiter);

  // Make sure there's nothing on MP 0, port 0 or MP 1, port 1 or MP 2, port 0.
  buffer_size = static_cast<uint32_t>(sizeof(buffer));
  EXPECT_EQ(MOJO_RESULT_NOT_FOUND,
            mp_0->ReadMessage(0,
                              buffer, &buffer_size,
                              NULL, NULL,
                              MOJO_READ_MESSAGE_FLAG_NONE));
  buffer_size = static_cast<uint32_t>(sizeof(buffer));
  EXPECT_EQ(MOJO_RESULT_NOT_FOUND,
            mp_1->ReadMessage(1,
                              buffer, &buffer_size,
                              NULL, NULL,
                              MOJO_READ_MESSAGE_FLAG_NONE));
  buffer_size = static_cast<uint32_t>(sizeof(buffer));
  EXPECT_EQ(MOJO_RESULT_NOT_FOUND,
            mp_2->ReadMessage(0,
                              buffer, &buffer_size,
                              NULL, NULL,
                              MOJO_READ_MESSAGE_FLAG_NONE));

  // Read from MP 3, port 1.
  buffer_size = static_cast<uint32_t>(sizeof(buffer));
  EXPECT_EQ(MOJO_RESULT_OK,
            mp_3->ReadMessage(1,
                              buffer, &buffer_size,
                              NULL, NULL,
                              MOJO_READ_MESSAGE_FLAG_NONE));
  EXPECT_EQ(sizeof(hello), static_cast<size_t>(buffer_size));
  EXPECT_EQ(0, strcmp(buffer, hello));

  // Write: MP 0, port 0 -> MP 1, port 1 again.

  waiter.Init();
  EXPECT_EQ(MOJO_RESULT_OK,
            mp_1->AddWaiter(1, &waiter, MOJO_WAIT_FLAG_READABLE, 123));

  EXPECT_EQ(MOJO_RESULT_OK,
            mp_0->WriteMessage(0,
                               world, sizeof(world),
                               NULL,
                               MOJO_WRITE_MESSAGE_FLAG_NONE));

  EXPECT_EQ(123, waiter.Wait(MOJO_DEADLINE_INDEFINITE));
  mp_1->RemoveWaiter(1, &waiter);

  // Make sure there's nothing on the other ports.
  buffer_size = static_cast<uint32_t>(sizeof(buffer));
  EXPECT_EQ(MOJO_RESULT_NOT_FOUND,
            mp_0->ReadMessage(0,
                              buffer, &buffer_size,
                              NULL, NULL,
                              MOJO_READ_MESSAGE_FLAG_NONE));
  buffer_size = static_cast<uint32_t>(sizeof(buffer));
  EXPECT_EQ(MOJO_RESULT_NOT_FOUND,
            mp_2->ReadMessage(0,
                              buffer, &buffer_size,
                              NULL, NULL,
                              MOJO_READ_MESSAGE_FLAG_NONE));
  buffer_size = static_cast<uint32_t>(sizeof(buffer));
  EXPECT_EQ(MOJO_RESULT_NOT_FOUND,
            mp_3->ReadMessage(1,
                              buffer, &buffer_size,
                              NULL, NULL,
                              MOJO_READ_MESSAGE_FLAG_NONE));

  buffer_size = static_cast<uint32_t>(sizeof(buffer));
  EXPECT_EQ(MOJO_RESULT_OK,
            mp_1->ReadMessage(1,
                              buffer, &buffer_size,
                              NULL, NULL,
                              MOJO_READ_MESSAGE_FLAG_NONE));
  EXPECT_EQ(sizeof(world), static_cast<size_t>(buffer_size));
  EXPECT_EQ(0, strcmp(buffer, world));
}

TEST_F(RemoteMessagePipeTest, CloseBeforeConnect) {
  const char hello[] = "hello";
  char buffer[100] = { 0 };
  uint32_t buffer_size = static_cast<uint32_t>(sizeof(buffer));
  Waiter waiter;

  // Connect message pipes. MP 0, port 1 will be attached to channel 0 and
  // connected to MP 1, port 0, which will be attached to channel 1. This leaves
  // MP 0, port 0 and MP 1, port 1 as the "user-facing" endpoints.

  scoped_refptr<MessagePipe> mp_0(new MessagePipe(
      scoped_ptr<MessagePipeEndpoint>(new LocalMessagePipeEndpoint()),
      scoped_ptr<MessagePipeEndpoint>(new ProxyMessagePipeEndpoint())));

  // Write to MP 0, port 0.
  EXPECT_EQ(MOJO_RESULT_OK,
            mp_0->WriteMessage(0,
                               hello, sizeof(hello),
                               NULL,
                               MOJO_WRITE_MESSAGE_FLAG_NONE));

  BootstrapMessagePipeNoWait(0, mp_0);


  // Close MP 0, port 0 before channel 1 is even connected.
  mp_0->Close(0);

  scoped_refptr<MessagePipe> mp_1(new MessagePipe(
      scoped_ptr<MessagePipeEndpoint>(new ProxyMessagePipeEndpoint()),
      scoped_ptr<MessagePipeEndpoint>(new LocalMessagePipeEndpoint())));

  // Prepare to wait on MP 1, port 1. (Add the waiter now. Otherwise, if we do
  // it later, it might already be readable.)
  waiter.Init();
  EXPECT_EQ(MOJO_RESULT_OK,
            mp_1->AddWaiter(1, &waiter, MOJO_WAIT_FLAG_READABLE, 123));

  BootstrapMessagePipeNoWait(1, mp_1);

  // Wait.
  EXPECT_EQ(123, waiter.Wait(MOJO_DEADLINE_INDEFINITE));
  mp_1->RemoveWaiter(1, &waiter);

  // Read from MP 1, port 1.
  EXPECT_EQ(MOJO_RESULT_OK,
            mp_1->ReadMessage(1,
                              buffer, &buffer_size,
                              NULL, NULL,
                              MOJO_READ_MESSAGE_FLAG_NONE));
  EXPECT_EQ(sizeof(hello), static_cast<size_t>(buffer_size));
  EXPECT_EQ(0, strcmp(buffer, hello));

  // And MP 1, port 1.
  mp_1->Close(1);
}
}  // namespace
}  // namespace system
}  // namespace mojo
