// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_UTILITY_PROCESS_HOST_H_
#define CONTENT_PUBLIC_BROWSER_UTILITY_PROCESS_HOST_H_

#include "base/environment.h"
#include "base/process/launch.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "content/common/content_export.h"
#include "content/public/common/bind_interface_helpers.h"
#include "ipc/ipc_sender.h"
#include "mojo/public/cpp/bindings/interface_ptr.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "services/service_manager/sandbox/sandbox_type.h"

namespace base {
class FilePath;
class SequencedTaskRunner;
}

namespace content {
class BrowserMessageFilter;
class UtilityProcessHostClient;
struct ChildProcessData;

// This class acts as the browser-side host to a utility child process.  A
// utility process is a short-lived process that is created to run a specific
// task.  This class lives solely on the IO thread.
// If you need a single method call in the process, use StartFooBar(p).
// If you need multiple batches of work to be done in the process, use
// StartBatchMode(), then multiple calls to StartFooBar(p), then finish with
// EndBatchMode().
// If you need to bind Mojo interfaces, use Start() to start the child
// process and then call BindInterface().
//
// Note: If your class keeps a ptr to an object of this type, grab a weak ptr to
// avoid a use after free since this object is deleted synchronously but the
// client notification is asynchronous.  See http://crbug.com/108871.
class UtilityProcessHost : public IPC::Sender {
 public:
  // Used to create a utility process. |client| is optional. If supplied it will
  // be notified of incoming messages from the utility process.
  // |client_task_runner| is required if |client| is supplied and is the task
  // runner upon which |client| will be invoked.
  CONTENT_EXPORT static UtilityProcessHost* Create(
      const scoped_refptr<UtilityProcessHostClient>& client,
      const scoped_refptr<base::SequencedTaskRunner>& client_task_runner);

  ~UtilityProcessHost() override {}

  virtual base::WeakPtr<UtilityProcessHost> AsWeakPtr() = 0;

  // Allows a directory to be opened through the sandbox, in case it's needed by
  // the operation.
  virtual void SetExposedDir(const base::FilePath& dir) = 0;

  // Make the process run with a specific sandbox type, or unsandboxed if
  // SANDBOX_TYPE_NO_SANDBOX is specified.
  virtual void SetSandboxType(service_manager::SandboxType sandbox_type) = 0;

  // Returns information about the utility child process.
  virtual const ChildProcessData& GetData() = 0;

#if defined(OS_POSIX)
  virtual void SetEnv(const base::EnvironmentMap& env) = 0;
#endif

  // Starts the utility process.
  virtual bool Start() = 0;

  // Bind an interface exposed by the utility process.
  virtual void BindInterface(const std::string& interface_name,
                             mojo::ScopedMessagePipeHandle interface_pipe) = 0;

  // Set the name of the process to appear in the task manager.
  virtual void SetName(const base::string16& name) = 0;

  // Adds an IPC message filter.
  virtual void AddFilter(BrowserMessageFilter* filter) = 0;
};

};  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_UTILITY_PROCESS_HOST_H_
