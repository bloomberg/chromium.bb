// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NACL_RENDERER_TRUSTED_PLUGIN_CHANNEL_H_
#define COMPONENTS_NACL_RENDERER_TRUSTED_PLUGIN_CHANNEL_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "ipc/ipc_listener.h"
#include "native_client/src/trusted/service_runtime/nacl_error_code.h"
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
                       bool is_helper_nexe);
  ~TrustedPluginChannel() override;

  bool Send(IPC::Message* message);

  // Listener implementation.
  bool OnMessageReceived(const IPC::Message& message) override;
  void OnChannelError() override;

  void OnReportExitStatus(int exit_status);
  void OnReportLoadStatus(NaClErrorCode load_status);

 private:
  // Non-owning pointer. This is safe because the TrustedPluginChannel is owned
  // by the NexeLoadManager pointed to here.
  NexeLoadManager* nexe_load_manager_;
  std::unique_ptr<IPC::SyncChannel> channel_;
  bool is_helper_nexe_;

  DISALLOW_COPY_AND_ASSIGN(TrustedPluginChannel);
};

}  // namespace nacl

#endif  // COMPONENTS_NACL_RENDERER_TRUSTED_PLUGIN_CHANNEL_H_
