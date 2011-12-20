// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/java/java_bridge_channel.h"

#include "content/common/child_process.h"
#include "content/common/java_bridge_messages.h"

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
  // Use a control message as this going to the JavaBridgeChannelHost, not an
  // injected object.
  int route_id = MSG_ROUTING_NONE;
  Send(new JavaBridgeMsg_GenerateRouteID(&route_id));
  // This should never fail, as the JavaBridgeChannelHost should always outlive
  // us.
  DCHECK_NE(MSG_ROUTING_NONE, route_id);
  return route_id;
}
