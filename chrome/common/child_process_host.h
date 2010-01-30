// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_CHILD_PROCESS_HOST_H_
#define CHROME_COMMON_CHILD_PROCESS_HOST_H_

#include <list>
#include <string>

// Must be included early (e.g. before chrome/common/plugin_messages.h)
#include "ipc/ipc_logging.h"

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/child_process_launcher.h"
#include "chrome/browser/renderer_host/resource_dispatcher_host.h"
#include "ipc/ipc_channel.h"

class CommandLine;
class NotificationType;

// Plugins/workers and other child processes that live on the IO thread should
// derive from this class.
//
// [Browser]RenderProcessHost is the main exception that doesn't derive from
// this class. That project lives on the UI thread.
class ChildProcessHost : public ResourceDispatcherHost::Receiver,
                         public IPC::Channel::Listener,
                         public ChildProcessLauncher::Client {
 public:
  virtual ~ChildProcessHost();

  // Returns the pathname to be used for a child process.  If a subprocess
  // pathname was specified on the command line, that will be used.  Otherwise,
  // the default child process pathname will be returned.  On most platforms,
  // this will be the same as the currently-executing process.
  //
  // The argument allow_self is used on Linux to indicate that we allow us to
  // fork from /proc/self/exe rather than using the "real" app path. This
  // prevents autoupdate from confusing us if it changes the file out from
  // under us. You will generally want to set this to true, except when there
  // is an override to the command line (for example, we're forking a renderer
  // in gdb). In this case, you'd use GetChildPath to get the real executable
  // file name, and then prepend the GDB command to the command line.
  //
  // On failure, returns an empty FilePath.
  static FilePath GetChildPath(bool allow_self);

  // Prepares command_line for crash reporting as appropriate.  On Linux and
  // Mac, a command-line flag to enable crash reporting in the child process
  // will be appended if needed, because the child process may not have access
  // to the data that determines the status of crash reporting in the
  // currently-executing process.  This function is a no-op on Windows.
  static void SetCrashReporterCommandLine(CommandLine* command_line);

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
    ChildProcessHost* operator->() { return *iterator_; }
    ChildProcessHost* operator*() { return *iterator_; }
    ChildProcessHost* operator++();
    bool Done();

   private:
    bool all_;
    ProcessType type_;
    std::list<ChildProcessHost*>::iterator iterator_;
  };

 protected:
  // The resource_dispatcher_host may be NULL to indicate none is needed for
  // this process type.
  ChildProcessHost(ProcessType type,
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

  // Derived classes return true if it's ok to shut down the child process.
  virtual bool CanShutdown() = 0;

  // Creates the IPC channel.  Returns true iff it succeeded.
  bool CreateChannel();

  // Notifies us that an instance has been created on this child process.
  void InstanceCreated();

  // IPC::Channel::Listener implementation:
  virtual void OnMessageReceived(const IPC::Message& msg) { }
  virtual void OnChannelConnected(int32 peer_pid) { }
  virtual void OnChannelError() { }

  // ChildProcessLauncher::Client implementation.
  virtual void OnProcessLaunched() {}

  // Derived classes can override this to know if the process crashed.
  virtual void OnProcessCrashed() {}

  bool opening_channel() { return opening_channel_; }
  const std::string& channel_id() { return channel_id_; }

  virtual bool DidChildCrash();

 private:
  // Sends the given notification to the notification service on the UI thread.
  void Notify(NotificationType type);

  // Called when the child process goes away.
  void OnChildDied();

  // By using an internal class as the IPC::Channel::Listener, we can intercept
  // OnMessageReceived/OnChannelConnected and do our own processing before
  // calling the subclass' implementation.
  class ListenerHook : public IPC::Channel::Listener,
                       public ChildProcessLauncher::Client {
   public:
    explicit ListenerHook(ChildProcessHost* host);
    virtual void OnMessageReceived(const IPC::Message& msg);
    virtual void OnChannelConnected(int32 peer_pid);
    virtual void OnChannelError();
    virtual void OnProcessLaunched();
   private:
    ChildProcessHost* host_;
  };

  ListenerHook listener_;

  // May be NULL if this current process has no resource dispatcher host.
  ResourceDispatcherHost* resource_dispatcher_host_;

  bool opening_channel_;  // True while we're waiting the channel to be opened.
  scoped_ptr<IPC::Channel> channel_;
  std::string channel_id_;
  scoped_ptr<ChildProcessLauncher> child_process_;
};

#endif  // CHROME_COMMON_CHILD_PROCESS_HOST_H_
