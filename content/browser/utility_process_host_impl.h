// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_UTILITY_PROCESS_HOST_IMPL_H_
#define CONTENT_BROWSER_UTILITY_PROCESS_HOST_IMPL_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "build/build_config.h"
#include "content/public/browser/browser_child_process_host_delegate.h"
#include "content/public/browser/utility_process_host.h"
#include "services/service_manager/public/cpp/identity.h"

namespace base {
class FilePath;
class SequencedTaskRunner;
class Thread;
}

namespace content {
class BrowserChildProcessHostImpl;
class InProcessChildThreadParams;

typedef base::Thread* (*UtilityMainThreadFactoryFunction)(
    const InProcessChildThreadParams&);

class CONTENT_EXPORT UtilityProcessHostImpl
    : public UtilityProcessHost,
      public BrowserChildProcessHostDelegate {
 public:
  static void RegisterUtilityMainThreadFactory(
      UtilityMainThreadFactoryFunction create);

  UtilityProcessHostImpl(
      const scoped_refptr<UtilityProcessHostClient>& client,
      const scoped_refptr<base::SequencedTaskRunner>& client_task_runner);
  ~UtilityProcessHostImpl() override;

  // UtilityProcessHost:
  base::WeakPtr<UtilityProcessHost> AsWeakPtr() override;
  bool Send(IPC::Message* message) override;
  void SetExposedDir(const base::FilePath& dir) override;
  void SetSandboxType(SandboxType sandbox_type) override;
#if defined(OS_WIN)
  void ElevatePrivileges() override;
#endif
  const ChildProcessData& GetData() override;
#if defined(OS_POSIX)
  void SetEnv(const base::EnvironmentMap& env) override;
#endif
  bool Start() override;
  void BindInterface(const std::string& interface_name,
                     mojo::ScopedMessagePipeHandle interface_pipe) override;
  void SetName(const base::string16& name) override;

  void set_child_flags(int flags) { child_flags_ = flags; }

  // Used when the utility process is going to host a service. |identity| is
  // the identity of the service being launched.
  void SetServiceIdentity(const service_manager::Identity& identity);

 private:
  // Starts the child process if needed, returns true on success.
  bool StartProcess();

  // BrowserChildProcessHost:
  bool OnMessageReceived(const IPC::Message& message) override;
  void OnProcessLaunchFailed(int error_code) override;
  void OnProcessCrashed(int exit_code) override;

  // Cleans up |this| as a result of a failed Start().
  void NotifyAndDelete(int error_code);

  // Notifies the client that the launch failed and deletes |host|.
  static void NotifyLaunchFailedAndDelete(
      base::WeakPtr<UtilityProcessHostImpl> host,
      int error_code);

  // Pointer to our client interface used for progress notifications.
  scoped_refptr<UtilityProcessHostClient> client_;

  // Task runner used for posting progess notifications to |client_|.
  scoped_refptr<base::SequencedTaskRunner> client_task_runner_;

  // Directory opened through the child process sandbox if needed.
  base::FilePath exposed_dir_;

  // Launch the child process with switches that will setup this sandbox type.
  SandboxType sandbox_type_;

  // Whether to launch the child process with elevated privileges.
  bool run_elevated_;

  // ChildProcessHost flags to use when starting the child process.
  int child_flags_;

  // Map of environment variables to values.
  base::EnvironmentMap env_;

  // True if StartProcess() has been called.
  bool started_;

  // The process name used to identify the process in task manager.
  base::string16 name_;

  // Child process host implementation.
  std::unique_ptr<BrowserChildProcessHostImpl> process_;

  // Used in single-process mode instead of |process_|.
  std::unique_ptr<base::Thread> in_process_thread_;

  // If this has a value it indicates the process is going to host a mojo
  // service.
  base::Optional<service_manager::Identity> service_identity_;

  // Used to vend weak pointers, and should always be declared last.
  base::WeakPtrFactory<UtilityProcessHostImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(UtilityProcessHostImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_UTILITY_PROCESS_HOST_IMPL_H_
