// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NACL_LOADER_NACL_TRUSTED_LISTENER_H_
#define COMPONENTS_NACL_LOADER_NACL_TRUSTED_LISTENER_H_

#include "base/memory/ref_counted.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/ipc_channel_proxy.h"
#include "ipc/ipc_listener.h"

namespace base {
class SingleThreadTaskRunner;
}

class NaClTrustedListener : public base::RefCounted<NaClTrustedListener>,
                            public IPC::Listener {
 public:
  NaClTrustedListener(const IPC::ChannelHandle& handle,
                      base::SingleThreadTaskRunner* ipc_task_runner);

#if defined(OS_POSIX)
  int TakeClientFileDescriptor();
#endif

  // Listener implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void OnChannelError() OVERRIDE;

  bool Send(IPC::Message* msg);

 private:
  friend class base::RefCounted<NaClTrustedListener>;
  virtual ~NaClTrustedListener();
  scoped_ptr<IPC::ChannelProxy> channel_proxy_;

  DISALLOW_COPY_AND_ASSIGN(NaClTrustedListener);
};

#endif  // COMPONENTS_NACL_LOADER_NACL_TRUSTED_LISTENER_H_
