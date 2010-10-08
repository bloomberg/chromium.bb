// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_CHILD_PROCESS_HOST_H_
#define CHROME_COMMON_CHILD_PROCESS_HOST_H_
#pragma once

#include <string>

#include "build/build_config.h"

#if defined(OS_WIN)
#include <windows.h>
#endif  // defined(OS_WIN)

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "chrome/common/notification_type.h"
#include "ipc/ipc_channel.h"

class CommandLine;
class FilePath;

namespace IPC {
class Message;
}

// Provides common functionality for hosting a child process and processing IPC
// messages between the host and the child process. Subclasses are responsible
// for the actual launching and terminating of the child processes.
//
class ChildProcessHost : public IPC::Channel::Listener {
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

#if defined(OS_WIN)
  // See comments in the cc file. This is a common hack needed for a process
  // hosting a sandboxed child process. Hence it lives in this file.
  static void PreCacheFont(LOGFONT font);
#endif  // defined(OS_WIN)

 protected:
  ChildProcessHost();

  // A helper method to send an IPC message to the child on the channel.
  // It behavies just like IPC::Message::Sender::Send. The implementor takes
  // ownership of the given Message regardless of whether or not this method
  // succeeds. This class does not implement IPC::Message::Sender to prevent
  // conflicts with subclasses which indirectly could inherit from
  // IPC::Message::Sender.
  bool SendOnChannel(IPC::Message* msg);

  // Derived classes return true if it's ok to shut down the child process.
  virtual bool CanShutdown() = 0;

  // Send the shutdown message to the child process.
  // Does not check if CanShutdown is true.
  virtual void ForceShutdown();

  // Creates the IPC channel.  Returns true iff it succeeded.
  virtual bool CreateChannel();

  // Notifies us that an instance has been created on this child process.
  virtual void InstanceCreated();

  // IPC::Channel::Listener implementation:
  virtual void OnMessageReceived(const IPC::Message& msg) { }
  virtual void OnChannelConnected(int32 peer_pid) { }
  virtual void OnChannelError() { }

  bool opening_channel() { return opening_channel_; }
  const std::string& channel_id() { return channel_id_; }
  IPC::Channel* channel() { return channel_.get(); }

  // Called when the child process goes away.
  virtual void OnChildDied();
  // Allows the derived implementation to intercept a message before it is
  // handed to the IPC::Channel::Listener::OnMessageReceived implementation.
  virtual bool InterceptMessageFromChild(const IPC::Message& msg);
  // Subclasses can implement specific notification methods.
  virtual void Notify(NotificationType type) { }

 private:
  // By using an internal class as the IPC::Channel::Listener, we can intercept
  // OnMessageReceived/OnChannelConnected and do our own processing before
  // calling the subclass' implementation.
  class ListenerHook : public IPC::Channel::Listener {
   public:
    explicit ListenerHook(ChildProcessHost* host);
    virtual void OnMessageReceived(const IPC::Message& msg);
    virtual void OnChannelConnected(int32 peer_pid);
    virtual void OnChannelError();
   private:
    ChildProcessHost* host_;
  };

  ListenerHook listener_;

  bool opening_channel_;  // True while we're waiting the channel to be opened.
  scoped_ptr<IPC::Channel> channel_;
  std::string channel_id_;

  DISALLOW_COPY_AND_ASSIGN(ChildProcessHost);
};

#endif  // CHROME_COMMON_CHILD_PROCESS_HOST_H_
