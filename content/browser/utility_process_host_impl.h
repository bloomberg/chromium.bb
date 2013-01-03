// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_UTILITY_PROCESS_HOST_IMPL_H_
#define CONTENT_BROWSER_UTILITY_PROCESS_HOST_IMPL_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/browser_child_process_host_delegate.h"
#include "content/public/browser/utility_process_host.h"

namespace base {
class SequencedTaskRunner;
}

namespace content {
class BrowserChildProcessHostImpl;

class CONTENT_EXPORT UtilityProcessHostImpl
    : public NON_EXPORTED_BASE(UtilityProcessHost),
      public BrowserChildProcessHostDelegate {
 public:
  UtilityProcessHostImpl(UtilityProcessHostClient* client,
                         base::SequencedTaskRunner* client_task_runner);
  virtual ~UtilityProcessHostImpl();

  // UtilityProcessHost implementation:
  virtual bool Send(IPC::Message* message) OVERRIDE;
  virtual bool StartBatchMode() OVERRIDE;
  virtual void EndBatchMode() OVERRIDE;
  virtual void SetExposedDir(const FilePath& dir) OVERRIDE;
  virtual void DisableSandbox() OVERRIDE;
  virtual void EnableZygote() OVERRIDE;
  virtual const ChildProcessData& GetData() OVERRIDE;
#if defined(OS_POSIX)
  virtual void SetEnv(const base::EnvironmentVector& env) OVERRIDE;
#endif

  void set_child_flags(int flags) { child_flags_ = flags; }

 private:
  // Starts a process if necessary.  Returns true if it succeeded or a process
  // has already been started via StartBatchMode().
  bool StartProcess();

  // BrowserChildProcessHost:
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void OnProcessCrashed(int exit_code) OVERRIDE;

  // A pointer to our client interface, who will be informed of progress.
  scoped_refptr<UtilityProcessHostClient> client_;
  scoped_refptr<base::SequencedTaskRunner> client_task_runner_;
  // True when running in batch mode, i.e., StartBatchMode() has been called
  // and the utility process will run until EndBatchMode().
  bool is_batch_mode_;

  FilePath exposed_dir_;

  // Whether to pass switches::kNoSandbox to the child.
  bool no_sandbox_;

  // Flags defined in ChildProcessHost with which to start the process.
  int child_flags_;

  // Launch the utility process from the zygote. Defaults to false.
  bool use_linux_zygote_;

  base::EnvironmentVector env_;

  bool started_;

  scoped_ptr<BrowserChildProcessHostImpl> process_;

  DISALLOW_COPY_AND_ASSIGN(UtilityProcessHostImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_UTILITY_PROCESS_HOST_IMPL_H_
