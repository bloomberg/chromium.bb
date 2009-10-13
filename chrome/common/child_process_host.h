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
                         public IPC::Channel::Listener {
 public:
  virtual ~ChildProcessHost();

  // Returns the pathname to be used for a child process.  If a subprocess
  // pathname was specified on the command line, that will be used.  Otherwise,
  // the default child process pathname will be returned.  On most platforms,
  // this will be the same as the currently-executing process.  On failure,
  // returns an empty wstring.
  static FilePath GetChildPath();

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
    Iterator(ProcessType type);
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
  ChildProcessHost(ProcessType type,
                   ResourceDispatcherHost* resource_dispatcher_host);

  // Derived classes return true if it's ok to shut down the child process.
  virtual bool CanShutdown() = 0;

  // Creates the IPC channel.  Returns true iff it succeeded.
  bool CreateChannel();

  // Once the subclass gets a handle to the process, it needs to tell
  // ChildProcessHost using this function.
  void SetHandle(base::ProcessHandle handle);

  // Notifies us that an instance has been created on this child process.
  void InstanceCreated();

  // IPC::Channel::Listener implementation:
  virtual void OnMessageReceived(const IPC::Message& msg) { }
  virtual void OnChannelConnected(int32 peer_pid) { }
  virtual void OnChannelError() { }

  bool opening_channel() { return opening_channel_; }
  const std::string& channel_id() { return channel_id_; }

  const IPC::Channel& channel() const { return *channel_; }

 private:
  // Sends the given notification to the notification service on the UI thread.
  void Notify(NotificationType type);

  // Called when the child process goes away.
  void OnChildDied();

  // By using an internal class as the IPC::Channel::Listener, we can intercept
  // OnMessageReceived/OnChannelConnected and do our own processing before
  // calling the subclass' implementation.
  class ListenerHook : public IPC::Channel::Listener {
   public:
    ListenerHook(ChildProcessHost* host);
    virtual void OnMessageReceived(const IPC::Message& msg);
    virtual void OnChannelConnected(int32 peer_pid);
    virtual void OnChannelError();
   private:
    ChildProcessHost* host_;
  };

  ListenerHook listener_;

  ResourceDispatcherHost* resource_dispatcher_host_;

  // True while we're waiting the channel to be opened.
  bool opening_channel_;

  // The IPC::Channel.
  scoped_ptr<IPC::Channel> channel_;

  // IPC Channel's id.
  std::string channel_id_;
};

#endif  // CHROME_COMMON_CHILD_PROCESS_HOST_H_
