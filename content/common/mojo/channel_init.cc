// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/mojo/channel_init.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "mojo/edk/embedder/embedder.h"

namespace content {

ChannelInit::ChannelInit() {}

ChannelInit::~ChannelInit() {}

mojo::ScopedMessagePipeHandle ChannelInit::Init(
    base::PlatformFile file,
    scoped_refptr<base::TaskRunner> io_thread_task_runner) {
  ipc_support_.reset(new IPC::ScopedIPCSupport(io_thread_task_runner));
  return mojo::edk::CreateMessagePipe(
      mojo::edk::ScopedPlatformHandle(mojo::edk::PlatformHandle(file)));
}

}  // namespace content
