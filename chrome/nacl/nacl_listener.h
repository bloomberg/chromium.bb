// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_NACL_NACL_LISTENER_H_
#define CHROME_NACL_NACL_LISTENER_H_

#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "chrome/common/nacl_types.h"
#include "ipc/ipc_listener.h"

namespace IPC {
class SyncChannel;
class SyncMessageFilter;
}

// The NaClListener is an IPC channel listener that waits for a
// request to start a NaCl module.
class NaClListener : public IPC::Listener {
 public:
  NaClListener();
  virtual ~NaClListener();
  // Listen for a request to launch a NaCl module.
  void Listen();

  bool Send(IPC::Message* msg);

#if defined(OS_LINUX)
  void set_prereserved_sandbox_size(size_t prereserved_sandbox_size) {
    prereserved_sandbox_size_ = prereserved_sandbox_size;
  }
#endif

 private:
  void OnStart(const nacl::NaClStartParams& params);
  virtual bool OnMessageReceived(const IPC::Message& msg) OVERRIDE;

  // A channel back to the browser.
  scoped_ptr<IPC::SyncChannel> channel_;

  // A filter that allows other threads to use the channel.
  scoped_refptr<IPC::SyncMessageFilter> filter_;

  base::WaitableEvent shutdown_event_;
  base::Thread io_thread_;

#if defined(OS_LINUX)
  size_t prereserved_sandbox_size_;
#endif

  // Used to identify what thread we're on.
  MessageLoop* main_loop_;

  DISALLOW_COPY_AND_ASSIGN(NaClListener);
};

#endif  // CHROME_NACL_NACL_LISTENER_H_
