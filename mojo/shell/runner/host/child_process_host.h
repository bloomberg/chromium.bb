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
#include "third_party/mojo/src/mojo/edk/embedder/channel_info_forward.h"
#include "third_party/mojo/src/mojo/edk/embedder/platform_channel_pair.h"
#include "third_party/mojo/src/mojo/edk/embedder/scoped_platform_handle.h"

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

  // See |ChildController|:
  void StartApp(InterfaceRequest<Application> application_request,
                const ChildController::StartAppCallback& on_app_complete);
  void ExitNow(int32_t exit_code);

 protected:
  void DidStart();

 private:
  // A thread-safe holder for the bootstrap message pipe to this child process.
  // The pipe is established on an arbitrary thread and may not be connected
  // until the host's message loop has stopped running.
  class PipeHolder : public base::RefCountedThreadSafe<PipeHolder> {
   public:
    PipeHolder();

    void Reject();
    void SetPipe(ScopedMessagePipeHandle pipe);
    ScopedMessagePipeHandle PassPipe();

   private:
    friend class base::RefCountedThreadSafe<PipeHolder>;

    ~PipeHolder();

    base::Lock lock_;
    bool reject_pipe_ = false;
    ScopedMessagePipeHandle pipe_;

    DISALLOW_COPY_AND_ASSIGN(PipeHolder);
  };

  void DoLaunch();

  void AppCompleted(int32_t result);

  // Callback for |embedder::CreateChannel()|.
  void DidCreateChannel(embedder::ChannelInfo* channel_info);

  // Called once |pipe_holder_| is bound to a pipe.
  void OnMessagePipeCreated();

  // Called when the child process is launched and when the bootstrap
  // message pipe is created. Once both things have happened (which may happen
  // in either order), |process_ready_callback_| is invoked.
  void MaybeNotifyProcessReady();

  // Callback used to receive the child message pipe from the ports EDK.
  // This may be called on any thread. It will always stash the pipe in
  // |holder|, and it will then attempt to call |callback| on
  // |callback_task_runner| (which may or may not still be running tasks.)
  static void OnParentMessagePipeCreated(
      scoped_refptr<PipeHolder> holder,
      scoped_refptr<base::TaskRunner> callback_task_runner,
      const base::Closure& callback,
      ScopedMessagePipeHandle pipe);

  scoped_refptr<base::TaskRunner> launch_process_runner_;
  bool start_sandboxed_;
  const base::FilePath app_path_;
  base::Process child_process_;
  // Used for the ChildController binding.
  embedder::PlatformChannelPair platform_channel_pair_;
  ChildControllerPtr controller_;
  embedder::ChannelInfo* channel_info_;
  ChildController::StartAppCallback on_app_complete_;
  embedder::HandlePassingInformation handle_passing_info_;

  // Used only when --use-new-edk is specified. Used to back the NodeChannel
  // between the parent and child node.
  scoped_ptr<edk::PlatformChannelPair> node_channel_;

  // Since Start() calls a method on another thread, we use an event to block
  // the main thread if it tries to destruct |this| while launching the process.
  base::WaitableEvent start_child_process_event_;

  // A token the child can use to connect a primordial pipe to the host.
  std::string primordial_pipe_token_;

  // Holds the message pipe to the child process until it is either closed or
  // bound to the controller interface.
  scoped_refptr<PipeHolder> pipe_holder_;

  // Invoked exactly once, as soon as the child process's ID is known and
  // a pipe to the child has been established.
  ProcessReadyCallback process_ready_callback_;

  base::WeakPtrFactory<ChildProcessHost> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ChildProcessHost);
};

}  // namespace shell
}  // namespace mojo

#endif  // MOJO_SHELL_RUNNER_HOST_CHILD_PROCESS_HOST_H_
