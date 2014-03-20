// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SHELL_APP_CHILD_PROCESS_HOST_H_
#define MOJO_SHELL_APP_CHILD_PROCESS_HOST_H_

#include "base/macros.h"
#include "mojo/shell/child_process_host.h"

namespace mojo {

namespace embedder {
struct ChannelInfo;
}

namespace shell {

// Note: After |Start()|, this object must remain alive until the delegate's
// |DidTerminate()| is called.
class AppChildProcessHost : public ChildProcessHost,
                            public ChildProcessHost::Delegate {
 public:
  class AppDelegate {
   public:
    virtual void DidTerminate() = 0;
  };

  AppChildProcessHost(Context* context, AppDelegate* app_delegate);
  virtual ~AppChildProcessHost();

 private:
  // |ChildProcessHost::Delegate| method:
  virtual void DidStart(bool success) OVERRIDE;

  // Callback for |embedder::CreateChannel()|.
  void DidCreateChannel(embedder::ChannelInfo* channel_info);

  AppDelegate* const app_delegate_;

  embedder::ChannelInfo* channel_info_;

  DISALLOW_COPY_AND_ASSIGN(AppChildProcessHost);
};

}  // namespace shell
}  // namespace mojo

#endif  // MOJO_SHELL_APP_CHILD_PROCESS_HOST_H_
