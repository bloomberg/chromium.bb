// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/nacl/loader/nacl_trusted_listener.h"

NaClTrustedListener::NaClTrustedListener(
    const IPC::ChannelHandle& handle,
    base::MessageLoopProxy* message_loop_proxy,
    base::WaitableEvent* shutdown_event) {
  channel_.reset(new IPC::SyncChannel(handle, IPC::Channel::MODE_SERVER, this,
                                      message_loop_proxy, true,
                                      shutdown_event));
}

NaClTrustedListener::~NaClTrustedListener() {
}

#if defined(OS_POSIX)
int NaClTrustedListener::TakeClientFileDescriptor() {
  return channel_->TakeClientFileDescriptor();
}
#endif

bool NaClTrustedListener::OnMessageReceived(const IPC::Message& msg) {
  return false;
}

void NaClTrustedListener::OnChannelConnected(int32 peer_pid) {
}

void NaClTrustedListener::OnChannelError() {
  channel_->Close();
}

bool NaClTrustedListener::Send(IPC::Message* msg) {
  return channel_->Send(msg);
}
