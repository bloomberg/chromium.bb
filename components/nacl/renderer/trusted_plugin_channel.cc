// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/nacl/renderer/trusted_plugin_channel.h"

#include "base/callback_helpers.h"
#include "content/public/renderer/render_thread.h"
#include "ipc/ipc_channel_proxy.h"
#include "ppapi/c/pp_errors.h"

namespace nacl {

TrustedPluginChannel::TrustedPluginChannel(
    const IPC::ChannelHandle& handle) {
  channel_proxy_ = IPC::ChannelProxy::Create(
      handle,
      IPC::Channel::MODE_CLIENT,
      this,
      content::RenderThread::Get()->GetIOMessageLoopProxy()).Pass();
}

TrustedPluginChannel::~TrustedPluginChannel() {
}

bool TrustedPluginChannel::Send(IPC::Message* message) {
  return channel_proxy_->Send(message);
}

bool TrustedPluginChannel::OnMessageReceived(const IPC::Message& message) {
  return false;
}

}  // namespace nacl
