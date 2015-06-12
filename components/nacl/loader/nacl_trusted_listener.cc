// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/nacl/loader/nacl_trusted_listener.h"

#include "base/single_thread_task_runner.h"
#include "ipc/message_filter.h"
#include "native_client/src/public/chrome_main.h"

namespace {

// The OnChannelError() event must be processed in a MessageFilter so it can
// be handled on the IO thread.  The main thread used by NaClListener is busy
// in NaClChromeMainAppStart(), so it can't be used for servicing messages.
class EOFMessageFilter : public IPC::MessageFilter {
 public:
  void OnChannelError() override {
    // The renderer process dropped its connection to this process (the NaCl
    // loader process), either because the <embed> element was removed
    // (perhaps implicitly if the tab was closed) or because the renderer
    // crashed.  The NaCl loader process should therefore exit.
    //
    // For SFI NaCl, trusted code does this exit voluntarily, but untrusted
    // code cannot disable it.  However, for Non-SFI NaCl, the following exit
    // call could be disabled by untrusted code.
    NaClExit(0);
  }
 private:
  ~EOFMessageFilter() override {}
};

}

NaClTrustedListener::NaClTrustedListener(
    const IPC::ChannelHandle& handle,
    base::SingleThreadTaskRunner* ipc_task_runner,
    base::WaitableEvent* shutdown_event)
    : channel_handle_(handle) {
  channel_ = IPC::SyncChannel::Create(handle,
                                      IPC::Channel::MODE_SERVER,
                                      this,
                                      ipc_task_runner,
                                      true,  /* create_channel_now */
                                      shutdown_event).Pass();
  channel_->AddFilter(new EOFMessageFilter());
}

NaClTrustedListener::~NaClTrustedListener() {
}

IPC::ChannelHandle NaClTrustedListener::TakeClientChannelHandle() {
  IPC::ChannelHandle handle = channel_handle_;
#if defined(OS_POSIX)
  handle.socket =
      base::FileDescriptor(channel_->TakeClientFileDescriptor());
#endif
  return handle;
}

bool NaClTrustedListener::OnMessageReceived(const IPC::Message& msg) {
  return false;
}

bool NaClTrustedListener::Send(IPC::Message* msg) {
  return channel_->Send(msg);
}
