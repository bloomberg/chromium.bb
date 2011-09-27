// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/java_bridge_channel_host.h"

#include "base/lazy_instance.h"
#include "base/stringprintf.h"
#include "base/synchronization/waitable_event.h"

using base::WaitableEvent;

namespace {
struct WaitableEventLazyInstanceTraits
    : public base::DefaultLazyInstanceTraits<WaitableEvent> {
  static WaitableEvent* New(void* instance) {
    // Use placement new to initialize our instance in our preallocated space.
    // The parenthesis is very important here to force POD type initialization.
    return new (instance) WaitableEvent(false, false);
  }
};
base::LazyInstance<WaitableEvent, WaitableEventLazyInstanceTraits> dummy_event(
    base::LINKER_INITIALIZED);
}

JavaBridgeChannelHost* JavaBridgeChannelHost::GetJavaBridgeChannelHost(
    int renderer_id, base::MessageLoopProxy* ipc_message_loop) {
  std::string channel_name(StringPrintf("r%d.javabridge", renderer_id));
  // We don't use a shutdown event because the Java Bridge only sends messages
  // from renderer to browser, so we'll never be waiting for a sync IPC to
  // complete. So we use an event which is never signaled.
  return static_cast<JavaBridgeChannelHost*>(NPChannelBase::GetChannel(
      channel_name,
      IPC::Channel::MODE_SERVER,
      ClassFactory,
      ipc_message_loop,
      false,
      dummy_event.Pointer()));
}

int JavaBridgeChannelHost::GenerateRouteID() {
  static int last_id = 0;
  return ++last_id;
}

bool JavaBridgeChannelHost::Init(base::MessageLoopProxy* ipc_message_loop,
                                 bool create_pipe_now,
                                 WaitableEvent* shutdown_event) {
  if (!NPChannelBase::Init(ipc_message_loop, create_pipe_now, shutdown_event)) {
    return false;
  }

  // Finish populating our ChannelHandle.
#if defined(OS_POSIX)
  // Leave the auto-close property at its default value.
  channel_handle_.socket.fd = channel_->GetClientFileDescriptor();
#endif

  return true;
}

bool JavaBridgeChannelHost::Send(IPC::Message* msg) {
  CHECK(!msg->is_sync() && msg->is_reply()) <<
      "Java Bridge only sends messages from renderer to browser.";
  return NPChannelBase::Send(msg);
}
