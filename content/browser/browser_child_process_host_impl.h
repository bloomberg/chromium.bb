// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BROWSER_CHILD_PROCESS_HOST_IMPL_H_
#define CONTENT_BROWSER_BROWSER_CHILD_PROCESS_HOST_IMPL_H_
#pragma once

#include <list>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/process.h"
#include "base/synchronization/waitable_event_watcher.h"
#include "content/browser/child_process_launcher.h"
#include "content/public/browser/browser_child_process_host.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/common/child_process_host_delegate.h"

namespace content {
class BrowserChildProcessHostIterator;
}

// Plugins/workers and other child processes that live on the IO thread use this
// class. RenderProcessHostImpl is the main exception that doesn't use this
/// class because it lives on the UI thread.
class CONTENT_EXPORT BrowserChildProcessHostImpl :
    public content::BrowserChildProcessHost,
    public NON_EXPORTED_BASE(content::ChildProcessHostDelegate),
    public ChildProcessLauncher::Client,
    public base::WaitableEventWatcher::Delegate {
 public:
  BrowserChildProcessHostImpl(
      content::ProcessType type,
      content::BrowserChildProcessHostDelegate* delegate);
  virtual ~BrowserChildProcessHostImpl();

  // Terminates all child processes and deletes each BrowserChildProcessHost
  // instance.
  static void TerminateAll();

  // BrowserChildProcessHostImpl implementation:
  virtual bool Send(IPC::Message* message) OVERRIDE;
  virtual void Launch(
#if defined(OS_WIN)
      const FilePath& exposed_dir,
#elif defined(OS_POSIX)
      bool use_zygote,
      const base::EnvironmentVector& environ,
#endif
      CommandLine* cmd_line) OVERRIDE;
  virtual const content::ChildProcessData& GetData() const OVERRIDE;
  virtual content::ChildProcessHost* GetHost() const OVERRIDE;
  virtual base::TerminationStatus GetTerminationStatus(int* exit_code) OVERRIDE;
  virtual void SetName(const string16& name) OVERRIDE;
  virtual void SetHandle(base::ProcessHandle handle) OVERRIDE;

  bool disconnect_was_alive() const { return disconnect_was_alive_; }

  // Returns the handle of the child process. This can be called only after
  // OnProcessLaunched is called or it will be invalid and may crash.
  base::ProcessHandle GetHandle() const;

  // Removes this host from the host list. Calls ChildProcessHost::ForceShutdown
  void ForceShutdown();

  // Controls whether the child process should be terminated on browser
  // shutdown. Default is to always terminate.
  void SetTerminateChildOnShutdown(bool terminate_on_shutdown);

  // Sends the given notification on the UI thread.
  void Notify(int type);

  content::BrowserChildProcessHostDelegate* delegate() const {
    return delegate_;
  }

  typedef std::list<BrowserChildProcessHostImpl*> BrowserChildProcessList;
 private:
  friend class content::BrowserChildProcessHostIterator;

  static BrowserChildProcessList* GetIterator();

  // ChildProcessHostDelegate implementation:
  virtual bool CanShutdown() OVERRIDE;
  virtual void OnChildDisconnected() OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void OnChannelConnected(int32 peer_pid) OVERRIDE;
  virtual void OnChannelError() OVERRIDE;

  // ChildProcessLauncher::Client implementation.
  virtual void OnProcessLaunched() OVERRIDE;

  // public base::WaitableEventWatcher::Delegate implementation:
  virtual void OnWaitableEventSignaled(
      base::WaitableEvent* waitable_event) OVERRIDE;

  content::ChildProcessData data_;
  content::BrowserChildProcessHostDelegate* delegate_;
  scoped_ptr<content::ChildProcessHost> child_process_host_;

  scoped_ptr<ChildProcessLauncher> child_process_;
#if defined(OS_WIN)
  base::WaitableEventWatcher child_watcher_;
#else
  base::WeakPtrFactory<BrowserChildProcessHostImpl> task_factory_;
#endif
  bool disconnect_was_alive_;
};

#endif  // CONTENT_BROWSER_BROWSER_CHILD_PROCESS_HOST_IMPL_H_
