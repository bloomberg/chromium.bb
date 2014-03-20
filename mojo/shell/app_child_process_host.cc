// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/app_child_process_host.h"

#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "mojo/embedder/embedder.h"
#include "mojo/public/system/core_cpp.h"
#include "mojo/shell/context.h"
#include "mojo/shell/task_runners.h"

namespace mojo {
namespace shell {

AppChildProcessHost::AppChildProcessHost(
    Context* context,
    mojo_shell::AppChildControllerClient* controller_client)
    : ChildProcessHost(context, this, ChildProcess::TYPE_APP),
      controller_client_(controller_client),
      channel_info_(NULL) {
}

AppChildProcessHost::~AppChildProcessHost() {
}

void AppChildProcessHost::WillStart() {
  DCHECK(platform_channel()->is_valid());

  mojo::ScopedMessagePipeHandle child_message_pipe(embedder::CreateChannel(
      platform_channel()->Pass(),
      context()->task_runners()->io_runner(),
      base::Bind(&AppChildProcessHost::DidCreateChannel,
                 base::Unretained(this)),
      base::MessageLoop::current()->message_loop_proxy()));
  controller_.reset(
      mojo_shell::ScopedAppChildControllerHandle(
          mojo_shell::AppChildControllerHandle(
              child_message_pipe.release().value())), controller_client_);
}

void AppChildProcessHost::DidStart(bool success) {
  DVLOG(2) << "AppChildProcessHost::DidStart()";

  if (!success) {
    LOG(ERROR) << "Failed to start app child process";
    controller_client_->AppCompleted(MOJO_RESULT_UNKNOWN);
    return;
  }
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
