// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BROWSER_CHILD_PROCESS_HOST_IMPL_H_
#define CONTENT_BROWSER_BROWSER_CHILD_PROCESS_HOST_IMPL_H_

#include <list>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/process.h"
#include "content/browser/child_process_launcher.h"
#include "content/public/browser/browser_child_process_host.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/common/child_process_host_delegate.h"

namespace content {

class BrowserChildProcessHostIterator;
class BrowserChildProcessObserver;

// Plugins/workers and other child processes that live on the IO thread use this
// class. RenderProcessHostImpl is the main exception that doesn't use this
/// class because it lives on the UI thread.
class CONTENT_EXPORT BrowserChildProcessHostImpl
    : public BrowserChildProcessHost,
      public NON_EXPORTED_BASE(ChildProcessHostDelegate),
      public ChildProcessLauncher::Client {
 public:
  BrowserChildProcessHostImpl(
      ProcessType type,
      BrowserChildProcessHostDelegate* delegate);
  virtual ~BrowserChildProcessHostImpl();

  // Terminates all child processes and deletes each BrowserChildProcessHost
  // instance.
  static void TerminateAll();

  // BrowserChildProcessHost implementation:
  virtual bool Send(IPC::Message* message) OVERRIDE;
  virtual void Launch(
#if defined(OS_WIN)
      const base::FilePath& exposed_dir,
#elif defined(OS_POSIX)
      bool use_zygote,
      const base::EnvironmentVector& environ,
#endif
      CommandLine* cmd_line) OVERRIDE;
  virtual const ChildProcessData& GetData() const OVERRIDE;
  virtual ChildProcessHost* GetHost() const OVERRIDE;
  virtual base::TerminationStatus GetTerminationStatus(int* exit_code) OVERRIDE;
  virtual void SetName(const string16& name) OVERRIDE;
  virtual void SetHandle(base::ProcessHandle handle) OVERRIDE;

  // Returns the handle of the child process. This can be called only after
  // OnProcessLaunched is called or it will be invalid and may crash.
  base::ProcessHandle GetHandle() const;

  // Removes this host from the host list. Calls ChildProcessHost::ForceShutdown
  void ForceShutdown();

  // Controls whether the child process should be terminated on browser
  // shutdown. Default is to always terminate.
  void SetTerminateChildOnShutdown(bool terminate_on_shutdown);

  // Called when an instance of a particular child is created in a page.
  static void NotifyProcessInstanceCreated(const ChildProcessData& data);

  BrowserChildProcessHostDelegate* delegate() const { return delegate_; }

  typedef std::list<BrowserChildProcessHostImpl*> BrowserChildProcessList;
 private:
  friend class BrowserChildProcessHostIterator;
  friend class BrowserChildProcessObserver;

  static BrowserChildProcessList* GetIterator();

  static void AddObserver(BrowserChildProcessObserver* observer);
  static void RemoveObserver(BrowserChildProcessObserver* observer);

  // ChildProcessHostDelegate implementation:
  virtual bool CanShutdown() OVERRIDE;
  virtual void OnChildDisconnected() OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void OnChannelConnected(int32 peer_pid) OVERRIDE;
  virtual void OnChannelError() OVERRIDE;

  // ChildProcessLauncher::Client implementation.
  virtual void OnProcessLaunched() OVERRIDE;

  ChildProcessData data_;
  BrowserChildProcessHostDelegate* delegate_;
  scoped_ptr<ChildProcessHost> child_process_host_;

  scoped_ptr<ChildProcessLauncher> child_process_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_BROWSER_CHILD_PROCESS_HOST_IMPL_H_
