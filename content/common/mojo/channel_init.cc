// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/mojo/channel_init.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/lazy_instance.h"
#include "base/message_loop/message_loop.h"
#include "base/thread_task_runner_handle.h"
#include "third_party/mojo/src/mojo/edk/embedder/embedder.h"

namespace content {

ChannelInit::ChannelInit() : channel_info_(nullptr), weak_factory_(this) {}

ChannelInit::~ChannelInit() {
  if (channel_info_)
    mojo::embedder::DestroyChannel(channel_info_,
                                   base::Bind(&base::DoNothing), nullptr);
}

mojo::ScopedMessagePipeHandle ChannelInit::Init(
    base::PlatformFile file,
    scoped_refptr<base::TaskRunner> io_thread_task_runner) {
  scoped_ptr<IPC::ScopedIPCSupport> ipc_support(
      new IPC::ScopedIPCSupport(io_thread_task_runner));
  mojo::ScopedMessagePipeHandle message_pipe = mojo::embedder::CreateChannel(
      mojo::embedder::ScopedPlatformHandle(
          mojo::embedder::PlatformHandle(file)),
      base::Bind(&ChannelInit::OnCreatedChannel, weak_factory_.GetWeakPtr(),
                 base::Passed(&ipc_support)),
      base::ThreadTaskRunnerHandle::Get());
  return message_pipe;
}

void ChannelInit::WillDestroySoon() {
  if (channel_info_)
    mojo::embedder::WillDestroyChannelSoon(channel_info_);
}

// static
void ChannelInit::OnCreatedChannel(
    base::WeakPtr<ChannelInit> self,
    scoped_ptr<IPC::ScopedIPCSupport> ipc_support,
    mojo::embedder::ChannelInfo* channel) {
  // If |self| was already destroyed, shut the channel down.
  if (!self) {
    mojo::embedder::DestroyChannel(channel,
                                   base::Bind(&base::DoNothing), nullptr);
    return;
  }

  DCHECK(!self->channel_info_);
  self->channel_info_ = channel;
  self->ipc_support_ = std::move(ipc_support);
}

}  // namespace content
