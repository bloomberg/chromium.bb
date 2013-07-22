// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/pepper/pepper_renderer_connection.h"

#include "content/browser/browser_child_process_host_impl.h"
#include "content/browser/ppapi_plugin_process_host.h"
#include "content/browser/renderer_host/pepper/browser_ppapi_host_impl.h"
#include "content/browser/renderer_host/pepper/pepper_file_ref_host.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "ipc/ipc_message_macros.h"
#include "ppapi/host/resource_host.h"
#include "ppapi/proxy/ppapi_message_utils.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/resource_message_params.h"

#include "content/common/view_messages.h"

namespace content {

namespace {
BrowserPpapiHostImpl* GetHostForChildProcess(int child_process_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  // Find the plugin which this message refers to. Check NaCl plugins first.
  BrowserPpapiHostImpl* host = static_cast<BrowserPpapiHostImpl*>(
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
  return host;
}
}  // namespace

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
    IPC_MESSAGE_HANDLER(PpapiHostMsg_FileRef_GetInfoForRenderer,
                        OnMsgFileRefGetInfoForRenderer)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP_EX()

  return handled;
}

void PepperRendererConnection::OnMsgCreateResourceHostFromHost(
    int routing_id,
    int child_process_id,
    const ppapi::proxy::ResourceMessageCallParams& params,
    PP_Instance instance,
    const IPC::Message& nested_msg) {
  BrowserPpapiHostImpl* host = GetHostForChildProcess(child_process_id);
  int pending_resource_host_id;
  if (!host) {
    DLOG(ERROR) << "Invalid plugin process ID.";
    pending_resource_host_id = 0;
  } else {
    // FileRef_CreateExternal is only permitted from the renderer. Because of
    // this, we handle this message here and not in
    // content_browser_pepper_host_factory.cc.
    scoped_ptr<ppapi::host::ResourceHost> resource_host;
    if (host->IsValidInstance(instance)) {
      if (nested_msg.type() == PpapiHostMsg_FileRef_CreateExternal::ID) {
        base::FilePath external_path;
        if (ppapi::UnpackMessage<PpapiHostMsg_FileRef_CreateExternal>(
            nested_msg, &external_path)) {
          resource_host.reset(new PepperFileRefHost(
              host, instance, params.pp_resource(), external_path));
        }
      }
    }

    if (!resource_host.get()) {
      resource_host = host->GetPpapiHost()->CreateResourceHost(params,
                                                               instance,
                                                               nested_msg);
    }
    pending_resource_host_id =
        host->GetPpapiHost()->AddPendingResourceHost(resource_host.Pass());
  }

  Send(new PpapiHostMsg_CreateResourceHostFromHostReply(
       routing_id, params.sequence(), pending_resource_host_id));
}

void PepperRendererConnection::OnMsgFileRefGetInfoForRenderer(
    int routing_id,
    int child_process_id,
    const ppapi::proxy::ResourceMessageCallParams& params) {
  PP_FileSystemType fs_type = PP_FILESYSTEMTYPE_INVALID;
  std::string file_system_url_spec;
  base::FilePath external_path;
  BrowserPpapiHostImpl* host = GetHostForChildProcess(child_process_id);
  if (host) {
    ppapi::host::ResourceHost* resource_host =
        host->GetPpapiHost()->GetResourceHost(params.pp_resource());
    if (resource_host) {
      PepperFileRefHost* file_ref_host = resource_host->AsPepperFileRefHost();
      if (file_ref_host) {
        fs_type = file_ref_host->GetFileSystemType();
        file_system_url_spec = file_ref_host->GetFileSystemURLSpec();
        external_path = file_ref_host->GetExternalPath();
      }
    }
  }
  Send(new PpapiHostMsg_FileRef_GetInfoForRendererReply(
       routing_id,
       params.sequence(),
       fs_type,
       file_system_url_spec,
       external_path));
}

}  // namespace content
