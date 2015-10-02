// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_RUNNER_CHILD_PROCESS_HOST_H_
#define MOJO_RUNNER_CHILD_PROCESS_HOST_H_

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/process/process.h"
#include "mojo/runner/child_process.mojom.h"
#include "mojo/runner/child_process_host.h"
#include "third_party/mojo/src/mojo/edk/embedder/channel_info_forward.h"
#include "third_party/mojo/src/mojo/edk/embedder/platform_channel_pair.h"
#include "third_party/mojo/src/mojo/edk/embedder/scoped_platform_handle.h"

namespace mojo {
namespace runner {

class Context;

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
  // |name| is just for debugging ease. We will spawn off a process so that it
  // can be sandboxed if |start_sandboxed| is true. |app_path| is a path to the
  // mojo application we wish to start.
  ChildProcessHost(Context* context,
                   bool start_sandboxed,
                   const base::FilePath& app_path);
  virtual ~ChildProcessHost();

  // |Start()|s the child process; calls |DidStart()| (on the thread on which
  // |Start()| was called) when the child has been started (or failed to start).
  // After calling |Start()|, this object must not be destroyed until
  // |DidStart()| has been called.
  // TODO(vtl): Consider using weak pointers and removing this requirement.
  void Start();

  // Waits for the child process to terminate, and returns its exit code.
  // Note: If |Start()| has been called, this must not be called until the
  // callback has been called.
  int Join();

  // See |ChildController|:
  void StartApp(InterfaceRequest<Application> application_request,
                const ChildController::StartAppCallback& on_app_complete);
  void ExitNow(int32_t exit_code);

 protected:
  // virtual for testing.
  virtual void DidStart(bool success);

 private:
  bool DoLaunch();

  void AppCompleted(int32_t result);

  // Callback for |embedder::CreateChannel()|.
  void DidCreateChannel(embedder::ChannelInfo* channel_info);

  Context* const context_;
  bool start_sandboxed_;
  const base::FilePath app_path_;
  base::Process child_process_;
  embedder::PlatformChannelPair platform_channel_pair_;
  ChildControllerPtr controller_;
  embedder::ChannelInfo* channel_info_;
  ChildController::StartAppCallback on_app_complete_;

  // Platform-specific "pipe" to the child process. Valid immediately after
  // creation.
  embedder::ScopedPlatformHandle platform_channel_;

  DISALLOW_COPY_AND_ASSIGN(ChildProcessHost);
};

}  // namespace runner
}  // namespace mojo

#endif  // MOJO_RUNNER_CHILD_PROCESS_HOST_H_
