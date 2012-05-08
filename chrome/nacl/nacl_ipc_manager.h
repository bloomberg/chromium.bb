// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_NACL_NACL_IPC_MANAGER_H_
#define CHROME_NACL_NACL_IPC_MANAGER_H_
#pragma once

#include <map>

#include "base/memory/ref_counted.h"
#include "base/message_loop_proxy.h"
#include "base/synchronization/lock.h"

namespace IPC {
struct ChannelHandle;
}

class NaClIPCAdapter;

// This class manages all IPC channels exposed to NaCl. We give NaCl void*
// "handles" to identify channels. When the untrusted nacl code does operations
// on these handles, this class maps them to the corresponding adapter object.
//
// This class must be threadsafe since nacl send/recvmsg functions can be
// called on any thread.
class NaClIPCManager {
 public:
  NaClIPCManager();
  ~NaClIPCManager();

  // Init must be called before creating any channels.
  void Init(scoped_refptr<base::MessageLoopProxy> io_thread_proxy);

  // Creates a nacl channel associated with the given channel handle (normally
  // this will come from the browser process). Returns the handle that should
  // be given to nacl to associated with this IPC channel.
  void* CreateChannel(const IPC::ChannelHandle& handle);

  // Destroys the channel with the given handle.
  void DestroyChannel(void* handle);

  // Implementation of sendmsg on the given channel. The return value is the
  // number of bytes written or -1 on failure.
  int SendMessage(void* handle, const char* data, int data_length);

  // Implementation of recvmsg on the given channel. The return value is the
  // number of bytes received or -1 on failure.
  int ReceiveMessage(void* handle, char* buffer, int buffer_length);

 private:
  // Looks up the adapter if given a handle. The pointer wil be null on
  // failures.
  scoped_refptr<NaClIPCAdapter> GetAdapter(void* handle);

  scoped_refptr<base::MessageLoopProxy> io_thread_proxy_;

  // Lock around all data below.
  base::Lock lock_;

  // All active IPC channels. Locked by lock_ above.
  typedef std::map<void*, scoped_refptr<NaClIPCAdapter> > AdapterMap;
  AdapterMap adapters_;

  DISALLOW_COPY_AND_ASSIGN(NaClIPCManager);
};

#endif  // CHROME_NACL_NACL_IPC_MANAGER_H_
