// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "base/bind_helpers.h"
#include "base/message_loop/message_loop.h"
#include "base/no_destructor.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "mojo/core/channel.h"
#include "mojo/core/connection_params.h"
#include "mojo/public/cpp/platform/platform_channel.h"

using namespace mojo::core;

// A fake delegate for each Channel endpoint. By the time an incoming message
// reaches a Delegate, all interesting message parsing at the lowest protocol
// layer has already been done by the receiving Channel implementation, so this
// doesn't need to do any work.
class FakeChannelDelegate : public Channel::Delegate {
 public:
  FakeChannelDelegate() = default;
  ~FakeChannelDelegate() override = default;

  void OnChannelMessage(const void* payload,
                        size_t payload_size,
                        std::vector<mojo::PlatformHandle> handles) override {}
  void OnChannelError(Channel::Error error) override {}
};

extern "C" int LLVMFuzzerTestOneInput(const unsigned char* data, size_t size) {
  static base::NoDestructor<base::MessageLoop> message_loop(
      base::MessageLoop::TYPE_IO);

  // Platform-specific implementation of an OS IPC primitive that is normally
  // used to carry messages between processes.
  mojo::PlatformChannel channel;

  FakeChannelDelegate receiver_delegate;
  auto receiver = Channel::Create(&receiver_delegate,
                                  ConnectionParams(channel.TakeLocalEndpoint()),
                                  message_loop->task_runner());
  receiver->Start();

  FakeChannelDelegate sender_delegate;
  auto sender = Channel::Create(&sender_delegate,
                                ConnectionParams(channel.TakeRemoteEndpoint()),
                                message_loop->task_runner());
  sender->Start();

  sender->Write(
      Channel::Message::CreateRawForFuzzing(base::make_span(data, size)));

  // Make sure |receiver| does whatever work it's gonna do in response to our
  // message. By the time the loop goes idle, all parsing will be done.
  base::RunLoop().RunUntilIdle();

  // Clean up our channels so we don't leak their underlying OS primitives.
  sender->ShutDown();
  sender.reset();
  receiver->ShutDown();
  receiver.reset();
  base::RunLoop().RunUntilIdle();

  return 0;
}
