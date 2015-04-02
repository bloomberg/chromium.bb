// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/app_child_process_host.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "mojo/edk/embedder/embedder.h"
#include "mojo/public/cpp/system/core.h"
#include "mojo/shell/context.h"
#include "mojo/shell/task_runners.h"

namespace mojo {
namespace shell {

AppChildProcessHost::AppChildProcessHost(Context* context)
    : ChildProcessHost(context), channel_info_(nullptr) {
}

AppChildProcessHost::~AppChildProcessHost() {
}

void AppChildProcessHost::StartApp(
    const String& app_path,
    bool clean_app_path,
    InterfaceRequest<Application> application_request,
    const AppChildController::StartAppCallback& on_app_complete) {
  DCHECK(controller_);

  on_app_complete_ = on_app_complete;
  controller_->StartApp(
      app_path, clean_app_path, application_request.Pass(),
      base::Bind(&AppChildProcessHost::AppCompleted, base::Unretained(this)));
}

void AppChildProcessHost::ExitNow(int32_t exit_code) {
  DCHECK(controller_);

  controller_->ExitNow(exit_code);
}

void AppChildProcessHost::WillStart() {
  DCHECK(platform_channel()->is_valid());

  ScopedMessagePipeHandle handle(embedder::CreateChannel(
      platform_channel()->Pass(), context()->task_runners()->io_runner(),
      base::Bind(&AppChildProcessHost::DidCreateChannel,
                 base::Unretained(this)),
      base::MessageLoop::current()->message_loop_proxy()));

  controller_.Bind(handle.Pass());
}

void AppChildProcessHost::DidStart(bool success) {
  DVLOG(2) << "AppChildProcessHost::DidStart()";

  if (!success) {
    LOG(ERROR) << "Failed to start app child process";
    AppCompleted(MOJO_RESULT_UNKNOWN);
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

void AppChildProcessHost::AppCompleted(int32_t result) {
  if (!on_app_complete_.is_null()) {
    auto on_app_complete = on_app_complete_;
    on_app_complete_.reset();
    on_app_complete.Run(result);
  }
}

}  // namespace shell
}  // namespace mojo
