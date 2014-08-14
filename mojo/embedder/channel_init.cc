// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/embedder/channel_init.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "mojo/embedder/embedder.h"

namespace mojo {
namespace embedder {

ChannelInit::ChannelInit() : channel_info_(NULL), weak_factory_(this) {
}

ChannelInit::~ChannelInit() {
  if (channel_info_)
    DestroyChannel(channel_info_);
}

ScopedMessagePipeHandle ChannelInit::Init(
    base::PlatformFile file,
    scoped_refptr<base::TaskRunner> io_thread_task_runner) {
  DCHECK(!io_thread_task_runner_.get());  // Should only init once.
  io_thread_task_runner_ = io_thread_task_runner;
  ScopedMessagePipeHandle message_pipe =
      CreateChannel(ScopedPlatformHandle(PlatformHandle(file)),
                    io_thread_task_runner,
                    base::Bind(&ChannelInit::OnCreatedChannel,
                               weak_factory_.GetWeakPtr(),
                               io_thread_task_runner),
                    base::MessageLoop::current()->message_loop_proxy()).Pass();
  return message_pipe.Pass();
}

void ChannelInit::WillDestroySoon() {
  if (channel_info_)
    WillDestroyChannelSoon(channel_info_);
}

// static
void ChannelInit::OnCreatedChannel(base::WeakPtr<ChannelInit> self,
                                   scoped_refptr<base::TaskRunner> io_thread,
                                   ChannelInfo* channel) {
  // If |self| was already destroyed, shut the channel down.
  if (!self) {
    DestroyChannel(channel);
    return;
  }

  self->channel_info_ = channel;
}

}  // namespace embedder
}  // namespace mojo
