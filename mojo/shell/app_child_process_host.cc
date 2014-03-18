// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/app_child_process_host.h"

namespace mojo {
namespace shell {

AppChildProcessHost::AppChildProcessHost(Context* context,
                                         AppDelegate* app_delegate)
    : ChildProcessHost(context, this, ChildProcess::TYPE_APP),
      app_delegate_(app_delegate) {
}

AppChildProcessHost::~AppChildProcessHost() {
}

void AppChildProcessHost::DidStart(bool success) {
  if (!success) {
    app_delegate_->DidTerminate();
    return;
  }

  // TODO(vtl): What else?
}

}  // namespace shell
}  // namespace mojo
