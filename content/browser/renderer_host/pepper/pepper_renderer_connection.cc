// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/pepper/pepper_renderer_connection.h"

#include "content/browser/browser_child_process_host_impl.h"
#include "content/browser/ppapi_plugin_process_host.h"
#include "content/browser/renderer_host/pepper/browser_ppapi_host_impl.h"
#include "content/common/pepper_renderer_instance_data.h"
#include "content/common/view_messages.h"
#include "content/browser/renderer_host/pepper/pepper_file_ref_host.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "ipc/ipc_message_macros.h"
#include "ppapi/host/resource_host.h"
#include "ppapi/proxy/ppapi_message_utils.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/resource_message_params.h"

namespace content {

PepperRendererConnection::PepperRendererConnection(int render_process_id)
    : render_process_id_(render_process_id) {
  // Only give the renderer permission for stable APIs.
  in_process_host_.reset(new BrowserPpapiHostImpl(this,
                                                  ppapi::PpapiPermissions(),
                                                  "",
                                                  base::FilePath(),
                                                  base::FilePath(),
                                                  false,
                                                  NULL));
}

PepperRendererConnection::~PepperRendererConnection() {
}

BrowserPpapiHostImpl* PepperRendererConnection::GetHostForChildProcess(
    int child_process_id) const {
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

  // If the message is being sent from an in-process plugin, we own the
  // BrowserPpapiHost.
  if (!host && child_process_id == 0) {
    host = in_process_host_.get();
  }

  return host;
}

bool PepperRendererConnection::OnMessageReceived(const IPC::Message& msg,
                                                 bool* message_was_ok) {
  if (in_process_host_->GetPpapiHost()->OnMessageReceived(msg))
    return true;

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(PepperRendererConnection, msg, *message_was_ok)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_CreateResourceHostFromHost,
                        OnMsgCreateResourceHostFromHost)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_FileRef_GetInfoForRenderer,
                        OnMsgFileRefGetInfoForRenderer)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DidCreateInProcessInstance,
                        OnMsgDidCreateInProcessInstance)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DidDeleteInProcessInstance,
                        OnMsgDidDeleteInProcessInstance)
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
    int32_t sequence,
    const std::vector<PP_Resource>& resources) {
  std::vector<PP_Resource> out_resources;
  std::vector<PP_FileSystemType> fs_types;
  std::vector<std::string> file_system_url_specs;
  std::vector<base::FilePath> external_paths;

  BrowserPpapiHostImpl* host = GetHostForChildProcess(child_process_id);
  if (host) {
    for (size_t i = 0; i < resources.size(); ++i) {
      ppapi::host::ResourceHost* resource_host =
          host->GetPpapiHost()->GetResourceHost(resources[i]);
      if (resource_host && resource_host->IsFileRefHost()) {
        PepperFileRefHost* file_ref_host =
            static_cast<PepperFileRefHost*>(resource_host);
        out_resources.push_back(resources[i]);
        fs_types.push_back(file_ref_host->GetFileSystemType());
        file_system_url_specs.push_back(file_ref_host->GetFileSystemURLSpec());
        external_paths.push_back(file_ref_host->GetExternalPath());
      }
    }
  }
  Send(new PpapiHostMsg_FileRef_GetInfoForRendererReply(
       routing_id,
       sequence,
       out_resources,
       fs_types,
       file_system_url_specs,
       external_paths));
}

void PepperRendererConnection::OnMsgDidCreateInProcessInstance(
    PP_Instance instance,
    const PepperRendererInstanceData& instance_data) {
  PepperRendererInstanceData data = instance_data;
  data.render_process_id = render_process_id_;
  in_process_host_->AddInstance(instance, data);
}

void PepperRendererConnection::OnMsgDidDeleteInProcessInstance(
    PP_Instance instance) {
  in_process_host_->DeleteInstance(instance);
}

}  // namespace content
