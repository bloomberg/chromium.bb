// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/pepper_browser_connection.h"

#include <limits>

#include "base/logging.h"
#include "content/common/view_messages.h"
#include "content/renderer/pepper/pepper_in_process_router.h"
#include "content/renderer/render_view_impl.h"
#include "ipc/ipc_message_macros.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/resource_message_params.h"

namespace content {

PepperBrowserConnection::PepperBrowserConnection(RenderView* render_view)
    : RenderViewObserver(render_view),
      RenderViewObserverTracker<PepperBrowserConnection>(render_view),
      next_sequence_number_(1) {
}

PepperBrowserConnection::~PepperBrowserConnection() {
}

bool PepperBrowserConnection::OnMessageReceived(const IPC::Message& msg) {
  // Check if the message is an in-process reply.
  if (PepperInProcessRouter::OnPluginMsgReceived(msg))
    return true;

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PepperBrowserConnection, msg)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_CreateResourceHostFromHostReply,
                        OnMsgCreateResourceHostFromHostReply)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void PepperBrowserConnection::DidCreateInProcessInstance(
    PP_Instance instance,
    int render_view_id,
    const GURL& document_url,
    const GURL& plugin_url) {
  Send(new ViewHostMsg_DidCreateInProcessInstance(
      instance,
      // Browser provides the render process id.
      PepperRendererInstanceData(0,
                                  render_view_id,
                                  document_url,
                                  plugin_url)));
}

void PepperBrowserConnection::DidDeleteInProcessInstance(PP_Instance instance) {
  Send(new ViewHostMsg_DidDeleteInProcessInstance(instance));
}

void PepperBrowserConnection::SendBrowserCreate(
    int child_process_id,
    PP_Instance instance,
    const IPC::Message& nested_msg,
    const PendingResourceIDCallback& callback) {
  int32_t sequence_number = GetNextSequence();
  pending_create_map_[sequence_number] = callback;
  ppapi::proxy::ResourceMessageCallParams params(0, sequence_number);
  Send(new PpapiHostMsg_CreateResourceHostFromHost(
      routing_id(),
      child_process_id,
      params,
      instance,
      nested_msg));
}

void PepperBrowserConnection::SendBrowserFileRefGetInfo(
    int child_process_id,
    const std::vector<PP_Resource>& resources,
    const FileRefGetInfoCallback& callback) {
  int32_t sequence_number = GetNextSequence();
  get_info_map_[sequence_number] = callback;
  Send(new PpapiHostMsg_FileRef_GetInfoForRenderer(
      routing_id(), child_process_id, sequence_number, resources));
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

void PepperBrowserConnection::OnMsgFileRefGetInfoReply(
    int32_t sequence_number,
    const std::vector<PP_Resource>& resources,
    const std::vector<PP_FileSystemType>& types,
    const std::vector<std::string>& file_system_url_specs,
    const std::vector<base::FilePath>& external_paths) {
  // Check that the message is destined for the plugin this object is associated
  // with.
  std::map<int32_t, FileRefGetInfoCallback>::iterator it =
      get_info_map_.find(sequence_number);
  if (it != get_info_map_.end()) {
    FileRefGetInfoCallback callback = it->second;
    get_info_map_.erase(it);
    callback.Run(resources, types, file_system_url_specs, external_paths);
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
