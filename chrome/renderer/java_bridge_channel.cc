// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/java_bridge_channel.h"

#include "content/common/child_process.h"

JavaBridgeChannel* JavaBridgeChannel::GetJavaBridgeChannel(
    const IPC::ChannelHandle& channel_handle,
    base::MessageLoopProxy* ipc_message_loop) {
  return static_cast<JavaBridgeChannel*>(NPChannelBase::GetChannel(
      channel_handle,
      IPC::Channel::MODE_CLIENT,
      ClassFactory,
      ipc_message_loop,
      true,
      ChildProcess::current()->GetShutDownEvent()));
}

int JavaBridgeChannel::GenerateRouteID() {
  NOTREACHED() << "Java Bridge only creates object stubs in the browser.";
  return -1;
}

bool JavaBridgeChannel::OnMessageReceived(const IPC::Message& msg) {
  NOTREACHED() << "Java Bridge only sends messages from renderer to browser.";
  return false;
}
