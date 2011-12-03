// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_CHILD_PROCESS_HOST_IMPL_H_
#define CONTENT_COMMON_CHILD_PROCESS_HOST_IMPL_H_
#pragma once

#include <string>
#include <vector>

#include "build/build_config.h"

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "base/shared_memory.h"
#include "base/string16.h"
#include "content/public/common/child_process_host.h"

class FilePath;

namespace content {
class ChildProcessHostDelegate;

// Provides common functionality for hosting a child process and processing IPC
// messages between the host and the child process. Users are responsible
// for the actual launching and terminating of the child processes.
class CONTENT_EXPORT ChildProcessHostImpl : public ChildProcessHost,
                                            public IPC::Channel::Listener {
 public:
  virtual ~ChildProcessHostImpl();

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

  // ChildProcessHost implementation
  virtual bool Send(IPC::Message* message) OVERRIDE;
  virtual void ForceShutdown() OVERRIDE;
  virtual std::string CreateChannel() OVERRIDE;
  virtual bool IsChannelOpening() OVERRIDE;
  virtual void AddFilter(IPC::ChannelProxy::MessageFilter* filter) OVERRIDE;
#if defined(OS_POSIX)
  virtual int TakeClientFileDescriptor() OVERRIDE;
#endif

 private:
  friend class ChildProcessHost;

  explicit ChildProcessHostImpl(ChildProcessHostDelegate* delegate);

  // IPC::Channel::Listener methods:
  virtual bool OnMessageReceived(const IPC::Message& msg) OVERRIDE;
  virtual void OnChannelConnected(int32 peer_pid) OVERRIDE;
  virtual void OnChannelError() OVERRIDE;

  // Message handlers:
  void OnShutdownRequest();
  void OnAllocateSharedMemory(uint32 buffer_size,
                              base::SharedMemoryHandle* handle);

  ChildProcessHostDelegate* delegate_;
  base::ProcessHandle peer_handle_;
  bool opening_channel_;  // True while we're waiting the channel to be opened.
  scoped_ptr<IPC::Channel> channel_;
  std::string channel_id_;

  // Holds all the IPC message filters.  Since this object lives on the IO
  // thread, we don't have a IPC::ChannelProxy and so we manage filters
  // manually.
  std::vector<scoped_refptr<IPC::ChannelProxy::MessageFilter> > filters_;

  DISALLOW_COPY_AND_ASSIGN(ChildProcessHostImpl);
};

}  // namespace content

#endif  // CONTENT_COMMON_CHILD_PROCESS_HOST_IMPL_H_
