// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/nacl/loader/nacl_trusted_listener.h"

#include "base/single_thread_task_runner.h"

NaClTrustedListener::NaClTrustedListener(
    const IPC::ChannelHandle& handle,
    base::SingleThreadTaskRunner* ipc_task_runner)
    : channel_handle_(handle),
      channel_proxy_(IPC::ChannelProxy::Create(
          handle, IPC::Channel::MODE_SERVER, this, ipc_task_runner)) {
}

NaClTrustedListener::~NaClTrustedListener() {
}

IPC::ChannelHandle NaClTrustedListener::TakeClientChannelHandle() {
  IPC::ChannelHandle handle = channel_handle_;
#if defined(OS_POSIX)
  handle.socket =
      base::FileDescriptor(channel_proxy_->TakeClientFileDescriptor(), true);
#endif
  return handle;
}

bool NaClTrustedListener::OnMessageReceived(const IPC::Message& msg) {
  return false;
}

void NaClTrustedListener::OnChannelError() {
  channel_proxy_->Close();
}

bool NaClTrustedListener::Send(IPC::Message* msg) {
  return channel_proxy_->Send(msg);
}
