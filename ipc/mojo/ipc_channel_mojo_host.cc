// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/mojo/ipc_channel_mojo_host.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "ipc/mojo/ipc_channel_mojo.h"

namespace IPC {

ChannelMojoHost::ChannelMojoHost(scoped_refptr<base::TaskRunner> task_runner)
    : weak_factory_(this), task_runner_(task_runner), channel_(NULL) {
}

ChannelMojoHost::~ChannelMojoHost() {
}

void ChannelMojoHost::OnClientLaunched(base::ProcessHandle process) {
  if (task_runner_ == base::MessageLoop::current()->message_loop_proxy()) {
    InvokeOnClientLaunched(process);
  } else {
    task_runner_->PostTask(FROM_HERE,
                           base::Bind(&ChannelMojoHost::InvokeOnClientLaunched,
                                      weak_factory_.GetWeakPtr(),
                                      process));
  }
}

void ChannelMojoHost::InvokeOnClientLaunched(base::ProcessHandle process) {
  if (!channel_)
    return;
  channel_->OnClientLaunched(process);
}

void ChannelMojoHost::OnChannelCreated(ChannelMojo* channel) {
  DCHECK(channel_ == NULL);
  channel_ = channel;
}

void ChannelMojoHost::OnChannelDestroyed() {
  DCHECK(channel_ != NULL);
  channel_ = NULL;
}

}  // namespace IPC
