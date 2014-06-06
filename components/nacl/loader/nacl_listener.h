// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_NACL_NACL_LISTENER_H_
#define CHROME_NACL_NACL_LISTENER_H_

#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "components/nacl/common/nacl_types.h"
#include "components/nacl/loader/nacl_trusted_listener.h"
#include "ipc/ipc_listener.h"

namespace base {
class MessageLoopProxy;
}

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

  void set_uses_nonsfi_mode(bool uses_nonsfi_mode) {
    uses_nonsfi_mode_ = uses_nonsfi_mode;
  }
#if defined(OS_LINUX)
  void set_prereserved_sandbox_size(size_t prereserved_sandbox_size) {
    prereserved_sandbox_size_ = prereserved_sandbox_size;
  }
#endif
#if defined(OS_POSIX)
  void set_number_of_cores(int number_of_cores) {
    number_of_cores_ = number_of_cores;
  }
#endif

 private:
  virtual bool OnMessageReceived(const IPC::Message& msg) OVERRIDE;

  void OnStart(const nacl::NaClStartParams& params);

  // Non-SFI version of OnStart().
  void StartNonSfi(const nacl::NaClStartParams& params);

  IPC::ChannelHandle CreateTrustedListener(
      base::MessageLoopProxy* message_loop_proxy,
      base::WaitableEvent* shutdown_event);

  // A channel back to the browser.
  scoped_ptr<IPC::SyncChannel> channel_;

  // A filter that allows other threads to use the channel.
  scoped_refptr<IPC::SyncMessageFilter> filter_;

  base::WaitableEvent shutdown_event_;
  base::Thread io_thread_;

  bool uses_nonsfi_mode_;
#if defined(OS_LINUX)
  size_t prereserved_sandbox_size_;
#endif
#if defined(OS_POSIX)
  // The outer sandbox on Linux and OSX prevents
  // sysconf(_SC_NPROCESSORS) from working; in Windows, there are no
  // problems with invoking GetSystemInfo.  Therefore, only in
  // OS_POSIX do we need to supply the number of cores into the
  // NaClChromeMainArgs object.
  int number_of_cores_;
#endif

  scoped_refptr<NaClTrustedListener> trusted_listener_;

  // Used to identify what thread we're on.
  base::MessageLoop* main_loop_;

  DISALLOW_COPY_AND_ASSIGN(NaClListener);
};

#endif  // CHROME_NACL_NACL_LISTENER_H_
