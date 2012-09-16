// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/pepper/browser_ppapi_host_impl.h"

#include "base/logging.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_view_host.h"
#include "ipc/ipc_message_macros.h"

namespace content {

BrowserPpapiHostImpl::BrowserPpapiHostImpl(
    IPC::Sender* sender,
    const ppapi::PpapiPermissions& permissions)
    : ppapi_host_(sender, permissions),
      plugin_process_handle_(base::kNullProcessHandle) {
  ppapi_host_.AddHostFactoryFilter(scoped_ptr<ppapi::host::HostFactory>(
      new ContentBrowserPepperHostFactory(this)));

  // TODO(brettw) bug 147507: Remove this log statement when we figure out why
  // permissions aren't hooked up properly.
  LOG(INFO) << "BrowserPpapiHostImpl::BrowserPpapiHostImpl "
            << "permissions = " << permissions.GetBits();
}

BrowserPpapiHostImpl::~BrowserPpapiHostImpl() {
}

bool BrowserPpapiHostImpl::OnMessageReceived(const IPC::Message& msg) {
  /* TODO(brettw) when we add messages, here, the code should look like this:
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(BrowserPpapiHostImpl, msg)
    // Add necessary message handlers here.
    IPC_MESSAGE_UNHANDLED(handled = ppapi_host_.OnMessageReceived(msg))
  IPC_END_MESSAGE_MAP();
  return handled;
  */
  return ppapi_host_.OnMessageReceived(msg);
}

ppapi::host::PpapiHost* BrowserPpapiHostImpl::GetPpapiHost() {
  return &ppapi_host_;
}

base::ProcessHandle BrowserPpapiHostImpl::GetPluginProcessHandle() const {
  // Handle should previously have been set before use.
  DCHECK(plugin_process_handle_ != base::kNullProcessHandle);
  return plugin_process_handle_;
}

bool BrowserPpapiHostImpl::IsValidInstance(PP_Instance instance) const {
  return instance_to_view_.find(instance) != instance_to_view_.end();
}

bool BrowserPpapiHostImpl::GetRenderViewIDsForInstance(
    PP_Instance instance,
    int* render_process_id,
    int* render_view_id) const {
  InstanceToViewMap::const_iterator found = instance_to_view_.find(instance);
  if (found == instance_to_view_.end()) {
    *render_process_id = 0;
    *render_view_id = 0;
    return false;
  }

  *render_process_id = found->second.process_id;
  *render_view_id = found->second.view_id;
  return true;
}

void BrowserPpapiHostImpl::AddInstanceForView(PP_Instance instance,
                                              int render_process_id,
                                              int render_view_id) {
  DCHECK(instance_to_view_.find(instance) == instance_to_view_.end());

  RenderViewIDs ids;
  ids.process_id = render_process_id;
  ids.view_id = render_view_id;
  instance_to_view_[instance] = ids;
}

void BrowserPpapiHostImpl::DeleteInstanceForView(PP_Instance instance) {
  InstanceToViewMap::iterator found = instance_to_view_.find(instance);
  if (found == instance_to_view_.end()) {
    NOTREACHED();
    return;
  }
  instance_to_view_.erase(found);
}

}  // namespace content
