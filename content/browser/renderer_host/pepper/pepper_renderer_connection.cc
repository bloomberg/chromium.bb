// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/pepper/pepper_renderer_connection.h"

#include "content/browser/browser_child_process_host_impl.h"
#include "content/browser/ppapi_plugin_process_host.h"
#include "content/browser/renderer_host/pepper/browser_ppapi_host_impl.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "ipc/ipc_message_macros.h"
#include "ppapi/host/resource_host.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/resource_message_params.h"

namespace content {

PepperRendererConnection::PepperRendererConnection() {
}

PepperRendererConnection::~PepperRendererConnection() {
}

bool PepperRendererConnection::OnMessageReceived(const IPC::Message& msg,
                                                 bool* message_was_ok) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(PepperRendererConnection, msg, *message_was_ok)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_CreateResourceHostFromHost,
                        OnMsgCreateResourceHostFromHost)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP_EX()

  return handled;
}

void PepperRendererConnection::OnMsgCreateResourceHostFromHost(
    int child_process_id,
    const ppapi::proxy::ResourceMessageCallParams& params,
    PP_Instance instance,
    const IPC::Message& nested_msg) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  BrowserPpapiHostImpl* host = NULL;

  // Find the plugin which this message refers to. Check NaCl plugins first.
  host = static_cast<BrowserPpapiHostImpl*>(
      GetContentClient()->browser()->GetExternalBrowserPpapiHost(
          child_process_id));

  if (!host) {
    // Check trusted pepper plugins.
    for (PpapiPluginProcessHostIterator iter; !iter.Done(); ++iter) {
      if (iter->process() &&
          iter->process()->GetData().id == child_process_id) {
        // Found the plugin.
        host = iter->host_impl();
        break;
      }
    }
  }

  int pending_resource_host_id;
  if (!host) {
    DLOG(ERROR) << "Invalid plugin process ID.";
    pending_resource_host_id = 0;
  } else {
    scoped_ptr<ppapi::host::ResourceHost> resource_host =
        host->GetPpapiHost()->CreateResourceHost(params,
                                                 instance,
                                                 nested_msg);
    pending_resource_host_id =
        host->GetPpapiHost()->AddPendingResourceHost(resource_host.Pass());
  }

  Send(new PpapiHostMsg_CreateResourceHostFromHostReply(
      params.sequence(), pending_resource_host_id));
}

}  // namespace content
