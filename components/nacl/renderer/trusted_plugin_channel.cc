// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/nacl/renderer/trusted_plugin_channel.h"

#include "base/debug/stack_trace.h"
#include "content/public/renderer/render_thread.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/shared_impl/ppapi_globals.h"

namespace nacl {

TrustedPluginChannel::TrustedPluginChannel(
    const IPC::ChannelHandle& handle,
    PP_CompletionCallback connected_callback,
    base::WaitableEvent* waitable_event)
    : connected_callback_(connected_callback) {
  channel_.reset(new IPC::SyncChannel(
      handle, IPC::Channel::MODE_CLIENT, this,
      content::RenderThread::Get()->GetIOMessageLoopProxy(), true,
      waitable_event));
}

TrustedPluginChannel::~TrustedPluginChannel() {
}

bool TrustedPluginChannel::Send(IPC::Message* message) {
  return channel_->Send(message);
}

bool TrustedPluginChannel::OnMessageReceived(const IPC::Message& message) {
  return false;
}

void TrustedPluginChannel::OnChannelConnected(int32 peer_pid) {
  if (connected_callback_.func)
    PP_RunAndClearCompletionCallback(&connected_callback_, PP_OK);
}

void TrustedPluginChannel::OnChannelError() {
  if (connected_callback_.func)
    PP_RunAndClearCompletionCallback(&connected_callback_, PP_ERROR_FAILED);
}

}  // namespace nacl
