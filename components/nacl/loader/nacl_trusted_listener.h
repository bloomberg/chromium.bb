// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NACL_LOADER_NACL_TRUSTED_LISTENER_H_
#define COMPONENTS_NACL_LOADER_NACL_TRUSTED_LISTENER_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_sync_channel.h"

namespace base {
class SingleThreadTaskRunner;
}

class NaClTrustedListener : public base::RefCounted<NaClTrustedListener>,
                            public IPC::Listener {
 public:
  NaClTrustedListener(const IPC::ChannelHandle& handle,
                      base::SingleThreadTaskRunner* ipc_task_runner,
                      base::WaitableEvent* shutdown_event);

  IPC::ChannelHandle TakeClientChannelHandle();

  // Listener implementation.
  bool OnMessageReceived(const IPC::Message& message) override;

  bool Send(IPC::Message* msg);

 private:
  friend class base::RefCounted<NaClTrustedListener>;
  ~NaClTrustedListener() override;
  IPC::ChannelHandle channel_handle_;
  std::unique_ptr<IPC::SyncChannel> channel_;

  DISALLOW_COPY_AND_ASSIGN(NaClTrustedListener);
};

#endif  // COMPONENTS_NACL_LOADER_NACL_TRUSTED_LISTENER_H_
