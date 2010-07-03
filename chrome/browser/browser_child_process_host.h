// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSER_CHILD_PROCESS_HOST_H_
#define CHROME_BROWSER_BROWSER_CHILD_PROCESS_HOST_H_

#include <list>

#include "chrome/browser/child_process_launcher.h"
#include "chrome/browser/renderer_host/resource_dispatcher_host.h"
#include "chrome/common/child_process_host.h"


// Plugins/workers and other child processes that live on the IO thread should
// derive from this class.
//
// [Browser]RenderProcessHost is the main exception that doesn't derive from
// this class. That project lives on the UI thread.
class BrowserChildProcessHost : public ResourceDispatcherHost::Receiver,
                                public ChildProcessHost,
                                public ChildProcessLauncher::Client {
 public:
  virtual ~BrowserChildProcessHost();

  // Prepares command_line for crash reporting as appropriate.  On Linux and
  // Mac, a command-line flag to enable crash reporting in the child process
  // will be appended if needed, because the child process may not have access
  // to the data that determines the status of crash reporting in the
  // currently-executing process.  This function is a no-op on Windows.
  static void SetCrashReporterCommandLine(CommandLine* command_line);

  // Terminates all child processes and deletes each ChildProcessHost instance.
  static void TerminateAll();

  // ResourceDispatcherHost::Receiver implementation:
  virtual bool Send(IPC::Message* msg);

  // The Iterator class allows iteration through either all child processes, or
  // ones of a specific type, depending on which constructor is used.  Note that
  // this should be done from the IO thread and that the iterator should not be
  // kept around as it may be invalidated on subsequent event processing in the
  // event loop.
  class Iterator {
   public:
    Iterator();
    explicit Iterator(ProcessType type);
    BrowserChildProcessHost* operator->() { return *iterator_; }
    BrowserChildProcessHost* operator*() { return *iterator_; }
    BrowserChildProcessHost* operator++();
    bool Done();

   private:
    bool all_;
    ProcessType type_;
    std::list<BrowserChildProcessHost*>::iterator iterator_;
  };

 protected:
  // The resource_dispatcher_host may be NULL to indicate none is needed for
  // this process type.
  BrowserChildProcessHost(ProcessType type,
                          ResourceDispatcherHost* resource_dispatcher_host);

  // Derived classes call this to launch the child process asynchronously.
  void Launch(
#if defined(OS_WIN)
      const FilePath& exposed_dir,
#elif defined(OS_POSIX)
      bool use_zygote,
      const base::environment_vector& environ,
#endif
      CommandLine* cmd_line);

  // ChildProcessLauncher::Client implementation.
  virtual void OnProcessLaunched() { }

  // Derived classes can override this to know if the process crashed.
  virtual void OnProcessCrashed() {}

  virtual bool DidChildCrash();

  // Overrides from ChildProcessHost
  virtual void OnChildDied();
  virtual bool InterceptMessageFromChild(const IPC::Message& msg);
  virtual void Notify(NotificationType type);
  // Extends the base class implementation and removes this host from
  // the host list. Calls ChildProcessHost::ForceShutdown
  virtual void ForceShutdown();

 private:
  // By using an internal class as the ChildProcessLauncher::Client, we can
  // intercept OnProcessLaunched and do our own processing before
  // calling the subclass' implementation.
  class ClientHook : public ChildProcessLauncher::Client {
   public:
    explicit ClientHook(BrowserChildProcessHost* host);
    virtual void OnProcessLaunched();
   private:
    BrowserChildProcessHost* host_;
  };
  ClientHook client_;
  // May be NULL if this current process has no resource dispatcher host.
  ResourceDispatcherHost* resource_dispatcher_host_;
  scoped_ptr<ChildProcessLauncher> child_process_;
};

#endif  // CHROME_BROWSER_BROWSER_CHILD_PROCESS_HOST_H_

