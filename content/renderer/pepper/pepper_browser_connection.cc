// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/pepper_browser_connection.h"

#include <limits>

#include "base/logging.h"
#include "content/renderer/pepper/pepper_plugin_delegate_impl.h"
#include "content/renderer/render_view_impl.h"
#include "ipc/ipc_message_macros.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/resource_message_params.h"

namespace content {

PepperBrowserConnection::PepperBrowserConnection(
    PepperPluginDelegateImpl* plugin_delegate)
    : plugin_delegate_(plugin_delegate),
      next_sequence_number_(1) {
}

PepperBrowserConnection::~PepperBrowserConnection() {
}

bool PepperBrowserConnection::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PepperBrowserConnection, msg)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_CreateResourceHostFromHostReply,
                        OnMsgCreateResourceHostFromHostReply)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void PepperBrowserConnection::SendBrowserCreate(
    int child_process_id,
    PP_Instance instance,
    const IPC::Message& nested_msg,
    const PendingResourceIDCallback& callback) {
  int32_t sequence_number = GetNextSequence();
  pending_create_map_[sequence_number] = callback;
  ppapi::proxy::ResourceMessageCallParams params(0, sequence_number);
  plugin_delegate_->render_view()->Send(
      new PpapiHostMsg_CreateResourceHostFromHost(
          child_process_id, params, instance, nested_msg));
}

void PepperBrowserConnection::OnMsgCreateResourceHostFromHostReply(
    int32_t sequence_number,
    int pending_resource_host_id) {
  // Check that the message is destined for the plugin this object is associated
  // with.
  std::map<int32_t, PendingResourceIDCallback>::iterator it =
      pending_create_map_.find(sequence_number);
  if (it != pending_create_map_.end()) {
    it->second.Run(pending_resource_host_id);
    pending_create_map_.erase(it);
  } else {
    NOTREACHED();
  }
}

int32_t PepperBrowserConnection::GetNextSequence() {
  // Return the value with wraparound, making sure we don't make a sequence
  // number with a 0 ID. Note that signed wraparound is undefined in C++ so we
  // manually check.
  int32_t ret = next_sequence_number_;
  if (next_sequence_number_ == std::numeric_limits<int32_t>::max())
    next_sequence_number_ = 1;  // Skip 0 which is invalid.
  else
    next_sequence_number_++;
  return ret;
}

}  // namespace content
