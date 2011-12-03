// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BROWSER_CHILD_PROCESS_HOST_H_
#define CONTENT_BROWSER_BROWSER_CHILD_PROCESS_HOST_H_
#pragma once

#include <list>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/process.h"
#include "base/synchronization/waitable_event_watcher.h"
#include "content/browser/child_process_launcher.h"
#include "content/common/content_export.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/common/child_process_host_delegate.h"
#include "ipc/ipc_message.h"

class ChildProcessHost;

namespace base {
class WaitableEvent;
}

// Plugins/workers and other child processes that live on the IO thread should
// derive from this class.
//
// [Browser]RenderProcessHost is the main exception that doesn't derive from
// this class. That project lives on the UI thread.
class CONTENT_EXPORT BrowserChildProcessHost :
    public NON_EXPORTED_BASE(content::ChildProcessHostDelegate),
    public ChildProcessLauncher::Client,
    public base::WaitableEventWatcher::Delegate,
    public IPC::Message::Sender {
 public:
  virtual ~BrowserChildProcessHost();

  virtual void OnWaitableEventSignaled(
      base::WaitableEvent* waitable_event) OVERRIDE;

  // Terminates all child processes and deletes each ChildProcessHost instance.
  static void TerminateAll();

  // The Iterator class allows iteration through either all child processes, or
  // ones of a specific type, depending on which constructor is used.  Note that
  // this should be done from the IO thread and that the iterator should not be
  // kept around as it may be invalidated on subsequent event processing in the
  // event loop.
  class CONTENT_EXPORT Iterator {
   public:
    Iterator();
    explicit Iterator(content::ProcessType type);
    BrowserChildProcessHost* operator->() { return *iterator_; }
    BrowserChildProcessHost* operator*() { return *iterator_; }
    BrowserChildProcessHost* operator++();
    bool Done();

   private:
    bool all_;
    content::ProcessType type_;
    std::list<BrowserChildProcessHost*>::iterator iterator_;
  };

  // IPC::Message::Sender override
  virtual bool Send(IPC::Message* message) OVERRIDE;

  const content::ChildProcessData& data() const { return data_; }
  content::ProcessType type() const { return data_.type; }
  const string16& name() const { return data_.name; }
  base::ProcessHandle handle() const { return data_.handle; }
  int id() const { return data_.id; }

 protected:
  explicit BrowserChildProcessHost(content::ProcessType type);

  // Derived classes call this to launch the child process asynchronously.
  void Launch(
#if defined(OS_WIN)
      const FilePath& exposed_dir,
#elif defined(OS_POSIX)
      bool use_zygote,
      const base::environment_vector& environ,
#endif
      CommandLine* cmd_line);

  // Returns the handle of the child process. This can be called only after
  // OnProcessLaunched is called or it will be invalid and may crash.
  base::ProcessHandle GetChildProcessHandle() const;

  // ChildProcessLauncher::Client implementation.
  virtual void OnProcessLaunched() OVERRIDE {}

  // Derived classes can override this to know if the process crashed.
  // |exit_code| is the status returned when the process crashed (for
  // posix, as returned from waitpid(), for Windows, as returned from
  // GetExitCodeProcess()).
  virtual void OnProcessCrashed(int exit_code) {}

  // Returns the termination status of a child.  |exit_code| is the
  // status returned when the process exited (for posix, as returned
  // from waitpid(), for Windows, as returned from
  // GetExitCodeProcess()).  |exit_code| may be NULL.
  base::TerminationStatus GetChildTerminationStatus(int* exit_code);

  // Overrides from ChildProcessHost
  virtual bool CanShutdown() OVERRIDE;
  virtual void OnChildDisconnected() OVERRIDE;
  virtual void ShutdownStarted() OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void OnChannelConnected(int32 peer_pid) OVERRIDE;

  // Removes this host from the host list. Calls ChildProcessHost::ForceShutdown
  void ForceShutdown();

  // Controls whether the child process should be terminated on browser
  // shutdown. Default is to always terminate.
  void SetTerminateChildOnShutdown(bool terminate_on_shutdown);

  // Sends the given notification on the UI thread.
  void Notify(int type);

  ChildProcessHost* child_process_host() const {
    return child_process_host_.get();
  }
  void set_name(const string16& name) { data_.name = name; }
  void set_handle(base::ProcessHandle handle) { data_.handle = handle; }

 private:
  // By using an internal class as the ChildProcessLauncher::Client, we can
  // intercept OnProcessLaunched and do our own processing before
  // calling the subclass' implementation.
  class ClientHook : public ChildProcessLauncher::Client {
   public:
    explicit ClientHook(BrowserChildProcessHost* host);
    virtual void OnProcessLaunched() OVERRIDE;
   private:
    BrowserChildProcessHost* host_;
  };

  content::ChildProcessData data_;
  scoped_ptr<ChildProcessHost> child_process_host_;

  ClientHook client_;
  scoped_ptr<ChildProcessLauncher> child_process_;
#if defined(OS_WIN)
  base::WaitableEventWatcher child_watcher_;
#else
  base::WeakPtrFactory<BrowserChildProcessHost> task_factory_;
#endif
  bool disconnect_was_alive_;
};

#endif  // CONTENT_BROWSER_BROWSER_CHILD_PROCESS_HOST_H_
