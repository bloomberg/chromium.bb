// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_CHILD_PROCESS_HOST_H_
#define CONTENT_COMMON_CHILD_PROCESS_HOST_H_
#pragma once

#include <string>
#include <vector>

#include "build/build_config.h"

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "base/shared_memory.h"
#include "base/string16.h"
#include "content/common/content_export.h"
#include "ipc/ipc_channel_proxy.h"

class FilePath;

namespace content {
class ChildProcessHostDelegate;
}

// Provides common functionality for hosting a child process and processing IPC
// messages between the host and the child process. Users are responsible
// for the actual launching and terminating of the child processes.
class CONTENT_EXPORT ChildProcessHost : public IPC::Channel::Listener,
                                        public IPC::Message::Sender {
 public:
  // These flags may be passed to GetChildPath in order to alter its behavior,
  // causing it to return a child path more suited to a specific task.
  enum {
    // No special behavior requested.
    CHILD_NORMAL = 0,

#if defined(OS_LINUX)
    // Indicates that the child execed after forking may be execced from
    // /proc/self/exe rather than using the "real" app path. This prevents
    // autoupdate from confusing us if it changes the file out from under us.
    // You will generally want to set this on Linux, except when there is an
    // override to the command line (for example, we're forking a renderer in
    // gdb). In this case, you'd use GetChildPath to get the real executable
    // file name, and then prepend the GDB command to the command line.
    CHILD_ALLOW_SELF = 1 << 0,
#elif defined(OS_MACOSX)

    // Requests that the child run in a process that does not have the
    // PIE (position-independent executable) bit set, effectively disabling
    // ASLR. For process types that need to allocate a large contiguous
    // region, ASLR may not leave a large enough "hole" for the purpose. This
    // option should be used sparingly, and only when absolutely necessary.
    // This option is currently incompatible with CHILD_ALLOW_HEAP_EXECUTION.
    CHILD_NO_PIE = 1 << 1,

    // Requests that the child run in a process that does not protect the
    // heap against execution. Normally, heap pages may be made executable
    // with mprotect, so this mode should be used sparingly. It is intended
    // for processes that may host plug-ins that expect an executable heap
    // without having to call mprotect. This option is currently incompatible
    // with CHILD_NO_PIE.
    CHILD_ALLOW_HEAP_EXECUTION = 1 << 2,
#endif
  };

  virtual ~ChildProcessHost();

  // Returns the pathname to be used for a child process.  If a subprocess
  // pathname was specified on the command line, that will be used.  Otherwise,
  // the default child process pathname will be returned.  On most platforms,
  // this will be the same as the currently-executing process.
  //
  // The |flags| argument accepts one or more flags such as CHILD_ALLOW_SELF
  // and CHILD_ALLOW_HEAP_EXECUTION as defined above. Pass only CHILD_NORMAL
  // if none of these special behaviors are required.
  //
  // On failure, returns an empty FilePath.
  static FilePath GetChildPath(int flags);

  explicit ChildProcessHost(content::ChildProcessHostDelegate* delegate);

  // IPC::Message::Sender implementation.
  virtual bool Send(IPC::Message* message) OVERRIDE;

  // Adds an IPC message filter.  A reference will be kept to the filter.
  void AddFilter(IPC::ChannelProxy::MessageFilter* filter);

  // Public and static for reuse by RenderMessageFilter.
  static void AllocateSharedMemory(
      uint32 buffer_size, base::ProcessHandle child_process,
      base::SharedMemoryHandle* handle);

  // Generates a unique channel name for a child process.
  // The "instance" pointer value is baked into the channel id.
  static std::string GenerateRandomChannelID(void* instance);

  // Returns a unique ID to identify a child process. On construction, this
  // function will be used to generate the id_, but it is also used to generate
  // IDs for the RenderProcessHost, which doesn't inherit from us, and whose IDs
  // must be unique for all child processes.
  //
  // This function is threadsafe since RenderProcessHost is on the UI thread,
  // but normally this will be used on the IO thread.
  static int GenerateChildProcessUniqueId();

  // Send the shutdown message to the child process.
  // Does not check if CanShutdown is true.
  void ForceShutdown();

  // Creates the IPC channel.  Returns true iff it succeeded.
  bool CreateChannel();

  bool opening_channel() { return opening_channel_; }
  const std::string& channel_id() { return channel_id_; }
  IPC::Channel* channel() { return channel_.get(); }

 private:
  // IPC::Channel::Listener methods:
  virtual bool OnMessageReceived(const IPC::Message& msg) OVERRIDE;
  virtual void OnChannelConnected(int32 peer_pid) OVERRIDE;
  virtual void OnChannelError() OVERRIDE;

  // Message handlers:
  void OnShutdownRequest();
  void OnAllocateSharedMemory(uint32 buffer_size,
                              base::SharedMemoryHandle* handle);

  content::ChildProcessHostDelegate* delegate_;
  base::ProcessHandle peer_handle_;
  bool opening_channel_;  // True while we're waiting the channel to be opened.
  scoped_ptr<IPC::Channel> channel_;
  std::string channel_id_;

  // Holds all the IPC message filters.  Since this object lives on the IO
  // thread, we don't have a IPC::ChannelProxy and so we manage filters
  // manually.
  std::vector<scoped_refptr<IPC::ChannelProxy::MessageFilter> > filters_;

  DISALLOW_COPY_AND_ASSIGN(ChildProcessHost);
};

#endif  // CONTENT_COMMON_CHILD_PROCESS_HOST_H_
