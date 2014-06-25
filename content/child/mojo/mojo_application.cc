// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/mojo/mojo_application.h"

#include "content/child/child_process.h"
#include "content/common/mojo/mojo_messages.h"
#include "ipc/ipc_message.h"

namespace content {

MojoApplication::MojoApplication() {
}

MojoApplication::~MojoApplication() {
}

bool MojoApplication::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(MojoApplication, msg)
    IPC_MESSAGE_HANDLER(MojoMsg_Activate, OnActivate)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void MojoApplication::OnActivate(
    const IPC::PlatformFileForTransit& file) {
#if defined(OS_POSIX)
  base::PlatformFile handle = file.fd;
#elif defined(OS_WIN)
  base::PlatformFile handle = file;
#endif
  mojo::ScopedMessagePipeHandle message_pipe =
      channel_init_.Init(handle,
                         ChildProcess::current()->io_message_loop_proxy());
  DCHECK(message_pipe.is_valid());
  service_registry_.BindRemoteServiceProvider(message_pipe.Pass());
}

}  // namespace content
