// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SHELL_RUNNER_HOST_CHILD_PROCESS_HOST_H_
#define MOJO_SHELL_RUNNER_HOST_CHILD_PROCESS_HOST_H_

#include <stdint.h>

#include <string>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/process/process.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "mojo/edk/embedder/platform_channel_pair.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "mojo/shell/runner/child/child_controller.mojom.h"
#include "mojo/shell/runner/host/child_process_host.h"

namespace base {
class TaskRunner;
}

namespace mojo {
namespace shell {

// This class represents a "child process host". Handles launching and
// connecting a platform-specific "pipe" to the child, and supports joining the
// child process. Currently runs a single app (loaded from the file system).
//
// This class is not thread-safe. It should be created/used/destroyed on a
// single thread.
//
// Note: Does not currently work on Windows before Vista.
// Note: After |Start()|, |StartApp| must be called and this object must
// remained alive until the |on_app_complete| callback is called.
class ChildProcessHost {
 public:
  using ProcessReadyCallback = base::Callback<void(base::ProcessId)>;

  // |name| is just for debugging ease. We will spawn off a process so that it
  // can be sandboxed if |start_sandboxed| is true. |app_path| is a path to the
  // mojo application we wish to start.
  ChildProcessHost(base::TaskRunner* launch_process_runner,
                   bool start_sandboxed,
                   const base::FilePath& app_path);
  // Allows a ChildProcessHost to be instantiated for an existing channel
  // created by someone else (e.g. an app that launched its own process).
  explicit ChildProcessHost(ScopedHandle channel);
  virtual ~ChildProcessHost();

  // |Start()|s the child process; calls |DidStart()| (on the thread on which
  // |Start()| was called) when the child has been started (or failed to start).
  void Start(const ProcessReadyCallback& callback);

  // Waits for the child process to terminate, and returns its exit code.
  int Join();

  // See |mojom::ChildController|:
  void StartApp(
      InterfaceRequest<mojom::ShellClient> request,
      const mojom::ChildController::StartAppCallback& on_app_complete);
  void ExitNow(int32_t exit_code);

 protected:
  void DidStart(const ProcessReadyCallback& callback);

 private:
  void DoLaunch();

  void AppCompleted(int32_t result);

  scoped_refptr<base::TaskRunner> launch_process_runner_;
  bool start_sandboxed_;
  const base::FilePath app_path_;
  base::Process child_process_;
  // Used for the ChildController binding.
  edk::PlatformChannelPair platform_channel_pair_;
  mojom::ChildControllerPtr controller_;
  mojom::ChildController::StartAppCallback on_app_complete_;
  edk::HandlePassingInformation handle_passing_info_;

  // Used to back the NodeChannel between the parent and child node.
  scoped_ptr<edk::PlatformChannelPair> node_channel_;

  // Since Start() calls a method on another thread, we use an event to block
  // the main thread if it tries to destruct |this| while launching the process.
  base::WaitableEvent start_child_process_event_;

  // A token the child can use to connect a primordial pipe to the host.
  std::string primordial_pipe_token_;

  base::WeakPtrFactory<ChildProcessHost> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ChildProcessHost);
};

}  // namespace shell
}  // namespace mojo

#endif  // MOJO_SHELL_RUNNER_HOST_CHILD_PROCESS_HOST_H_
