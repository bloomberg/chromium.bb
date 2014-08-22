// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NACL_RENDERER_TRUSTED_PLUGIN_CHANNEL_H_
#define COMPONENTS_NACL_RENDERER_TRUSTED_PLUGIN_CHANNEL_H_

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "ipc/ipc_listener.h"
#include "ppapi/c/pp_instance.h"

namespace base {
class WaitableEvent;
}  // namespace base

namespace IPC {
struct ChannelHandle;
class Message;
class SyncChannel;
}  // namespace IPC

namespace nacl {
class NexeLoadManager;

class TrustedPluginChannel : public IPC::Listener {
 public:
  TrustedPluginChannel(NexeLoadManager* nexe_load_manager,
                       const IPC::ChannelHandle& handle,
                       base::WaitableEvent* shutdown_event,
                       bool report_exit_status);
  virtual ~TrustedPluginChannel();

  bool Send(IPC::Message* message);

  // Listener implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  void OnReportExitStatus(int exit_status);

 private:
  // Non-owning pointer. This is safe because the TrustedPluginChannel is owned
  // by the NexeLoadManager pointed to here.
  NexeLoadManager* nexe_load_manager_;
  scoped_ptr<IPC::SyncChannel> channel_;
  bool report_exit_status_;

  DISALLOW_COPY_AND_ASSIGN(TrustedPluginChannel);
};

}  // namespace nacl

#endif  // COMPONENTS_NACL_RENDERER_TRUSTED_PLUGIN_CHANNEL_H_
