// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/nacl/renderer/trusted_plugin_channel.h"

#include "base/callback_helpers.h"
#include "components/nacl/common/nacl_renderer_messages.h"
#include "components/nacl/renderer/nexe_load_manager.h"
#include "content/public/renderer/render_thread.h"
#include "ipc/ipc_sync_channel.h"
#include "ipc/ipc_message_macros.h"
#include "ppapi/c/pp_errors.h"

namespace nacl {

TrustedPluginChannel::TrustedPluginChannel(
    NexeLoadManager* nexe_load_manager,
    const IPC::ChannelHandle& handle,
    base::WaitableEvent* shutdown_event,
    bool report_exit_status)
    : nexe_load_manager_(nexe_load_manager),
      report_exit_status_(report_exit_status) {
  channel_ = IPC::SyncChannel::Create(
      handle,
      IPC::Channel::MODE_CLIENT,
      this,
      content::RenderThread::Get()->GetIOMessageLoopProxy(),
      true,
      shutdown_event).Pass();
}

TrustedPluginChannel::~TrustedPluginChannel() {
}

bool TrustedPluginChannel::Send(IPC::Message* message) {
  return channel_->Send(message);
}

bool TrustedPluginChannel::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(TrustedPluginChannel, msg)
    IPC_MESSAGE_HANDLER(NaClRendererMsg_ReportExitStatus, OnReportExitStatus);
    IPC_MESSAGE_UNHANDLED(handled = false);
  IPC_END_MESSAGE_MAP()
  return handled;
}

void TrustedPluginChannel::OnReportExitStatus(int exit_status) {
  if (report_exit_status_)
    nexe_load_manager_->set_exit_status(exit_status);
}

}  // namespace nacl
