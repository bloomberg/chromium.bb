// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/extension_port.h"

#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/worker_thread.h"
#include "extensions/common/api/messaging/message.h"
#include "extensions/common/api/messaging/port_context.h"
#include "extensions/common/api/messaging/port_id.h"
#include "extensions/common/extension_messages.h"
#include "extensions/renderer/extension_bindings_system.h"
#include "extensions/renderer/ipc_message_sender.h"
#include "extensions/renderer/script_context.h"
#include "extensions/renderer/worker_thread_dispatcher.h"
#include "extensions/renderer/worker_thread_util.h"

namespace extensions {

ExtensionPort::ExtensionPort(ScriptContext* script_context,
                             const PortId& id,
                             int js_id)
    : script_context_(script_context), id_(id), js_id_(js_id) {}

ExtensionPort::~ExtensionPort() {}

void ExtensionPort::PostExtensionMessage(std::unique_ptr<Message> message) {
  if (worker_thread_util::IsWorkerThread()) {
    DCHECK(!script_context_->GetRenderFrame());
    WorkerThreadDispatcher::GetBindingsSystem()
        ->GetIPCMessageSender()
        ->SendPostMessageToPort(id_, *message);
  } else {
    content::RenderFrame* render_frame = script_context_->GetRenderFrame();
    if (!render_frame)
      return;
    render_frame->Send(new ExtensionHostMsg_PostMessage(id_, *message));
  }
}

void ExtensionPort::Close(bool close_channel) {
  if (worker_thread_util::IsWorkerThread()) {
    DCHECK(!script_context_->GetRenderFrame());
    WorkerThreadDispatcher::GetBindingsSystem()
        ->GetIPCMessageSender()
        ->SendCloseMessagePort(MSG_ROUTING_NONE, id_, close_channel);
  } else {
    content::RenderFrame* render_frame = script_context_->GetRenderFrame();
    if (!render_frame)
      return;
    render_frame->Send(new ExtensionHostMsg_CloseMessagePort(
        PortContext::ForFrame(render_frame->GetRoutingID()), id_,
        close_channel));
  }
}

}  // namespace extensions
