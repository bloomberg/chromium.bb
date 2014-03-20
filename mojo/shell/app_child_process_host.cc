// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/app_child_process_host.h"

#include "base/message_loop/message_loop.h"
#include "mojo/embedder/embedder.h"
#include "mojo/public/system/core_cpp.h"
#include "mojo/shell/context.h"
#include "mojo/shell/task_runners.h"

namespace mojo {
namespace shell {

AppChildProcessHost::AppChildProcessHost(Context* context,
                                         AppDelegate* app_delegate)
    : ChildProcessHost(context, this, ChildProcess::TYPE_APP),
      app_delegate_(app_delegate),
      channel_info_(NULL) {
}

AppChildProcessHost::~AppChildProcessHost() {
}

void AppChildProcessHost::DidStart(bool success) {
  DVLOG(2) << "AppChildProcessHost::DidStart()";

  if (!success) {
    app_delegate_->DidTerminate();
    return;
  }

  mojo::ScopedMessagePipeHandle child_message_pipe(embedder::CreateChannel(
      platform_channel()->Pass(),
      context()->task_runners()->io_runner(),
      base::Bind(&AppChildProcessHost::DidCreateChannel,
                 base::Unretained(this)),
      base::MessageLoop::current()->message_loop_proxy()));

  // TODO(vtl): Hook up a RemotePtr, etc.
}

// Callback for |embedder::CreateChannel()|.
void AppChildProcessHost::DidCreateChannel(
    embedder::ChannelInfo* channel_info) {
  DVLOG(2) << "AppChildProcessHost::DidCreateChannel()";

  CHECK(channel_info);
  channel_info_ = channel_info;
}

}  // namespace shell
}  // namespace mojo
