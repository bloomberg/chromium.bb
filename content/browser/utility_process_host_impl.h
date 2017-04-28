// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_UTILITY_PROCESS_HOST_IMPL_H_
#define CONTENT_BROWSER_UTILITY_PROCESS_HOST_IMPL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "content/public/browser/browser_child_process_host_delegate.h"
#include "content/public/browser/utility_process_host.h"

namespace base {
class SequencedTaskRunner;
class Thread;
}

namespace content {
class BrowserChildProcessHostImpl;
class InProcessChildThreadParams;

typedef base::Thread* (*UtilityMainThreadFactoryFunction)(
    const InProcessChildThreadParams&);

class CONTENT_EXPORT UtilityProcessHostImpl
    : public NON_EXPORTED_BASE(UtilityProcessHost),
      public BrowserChildProcessHostDelegate {
 public:
  static void RegisterUtilityMainThreadFactory(
      UtilityMainThreadFactoryFunction create);

  UtilityProcessHostImpl(
      const scoped_refptr<UtilityProcessHostClient>& client,
      const scoped_refptr<base::SequencedTaskRunner>& client_task_runner);
  ~UtilityProcessHostImpl() override;

  // UtilityProcessHost implementation:
  base::WeakPtr<UtilityProcessHost> AsWeakPtr() override;
  bool Send(IPC::Message* message) override;
  bool StartBatchMode() override;
  void EndBatchMode() override;
  void SetExposedDir(const base::FilePath& dir) override;
  void DisableSandbox() override;
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

 private:
  // Starts a process if necessary.  Returns true if it succeeded or a process
  // has already been started via StartBatchMode().
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

  // A pointer to our client interface, who will be informed of progress.
  scoped_refptr<UtilityProcessHostClient> client_;
  scoped_refptr<base::SequencedTaskRunner> client_task_runner_;
  // True when running in batch mode, i.e., StartBatchMode() has been called
  // and the utility process will run until EndBatchMode().
  bool is_batch_mode_;

  base::FilePath exposed_dir_;

  // Whether to pass switches::kNoSandbox to the child.
  bool no_sandbox_;

  // Whether to launch the process with elevated privileges.
  bool run_elevated_;

  // Flags defined in ChildProcessHost with which to start the process.
  int child_flags_;

  base::EnvironmentMap env_;

  bool started_;

  // A user-visible name identifying this process. Used to indentify this
  // process in the task manager.
  base::string16 name_;

  std::unique_ptr<BrowserChildProcessHostImpl> process_;

  // Used in single-process mode instead of process_.
  std::unique_ptr<base::Thread> in_process_thread_;

  // Used to vend weak pointers, and should always be declared last.
  base::WeakPtrFactory<UtilityProcessHostImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(UtilityProcessHostImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_UTILITY_PROCESS_HOST_IMPL_H_
