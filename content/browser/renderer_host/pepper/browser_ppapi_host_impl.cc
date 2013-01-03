// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/pepper/browser_ppapi_host_impl.h"

#include "content/browser/renderer_host/pepper/pepper_message_filter.h"
#include "content/browser/trace_message_filter.h"
#include "content/common/pepper_renderer_instance_data.h"
#include "content/public/browser/render_view_host.h"
#include "ipc/ipc_message_macros.h"

namespace content {

// static
BrowserPpapiHost* BrowserPpapiHost::CreateExternalPluginProcess(
    IPC::Sender* sender,
    ppapi::PpapiPermissions permissions,
    base::ProcessHandle plugin_child_process,
    IPC::ChannelProxy* channel,
    net::HostResolver* host_resolver,
    int render_process_id,
    int render_view_id) {
  // TODO(raymes): Figure out how to plumb plugin_name and
  // profile_data_directory through for NaCl. They are currently only needed for
  // PPB_Flash_File interfaces so it doesn't matter.
  std::string plugin_name;
  FilePath profile_data_directory;
  BrowserPpapiHostImpl* browser_ppapi_host =
      new BrowserPpapiHostImpl(sender, permissions, plugin_name,
                               profile_data_directory);
  browser_ppapi_host->set_plugin_process_handle(plugin_child_process);

  channel->AddFilter(
      new PepperMessageFilter(PepperMessageFilter::NACL,
                              permissions,
                              host_resolver,
                              render_process_id,
                              render_view_id));
  channel->AddFilter(browser_ppapi_host->message_filter());
  channel->AddFilter(new TraceMessageFilter());

  return browser_ppapi_host;
}

BrowserPpapiHostImpl::BrowserPpapiHostImpl(
    IPC::Sender* sender,
    const ppapi::PpapiPermissions& permissions,
    const std::string& plugin_name,
    const FilePath& profile_data_directory)
    : ppapi_host_(new ppapi::host::PpapiHost(sender, permissions)),
      plugin_process_handle_(base::kNullProcessHandle),
      plugin_name_(plugin_name),
      profile_data_directory_(profile_data_directory) {
  message_filter_ = new HostMessageFilter(ppapi_host_.get());
  ppapi_host_->AddHostFactoryFilter(scoped_ptr<ppapi::host::HostFactory>(
      new ContentBrowserPepperHostFactory(this)));
}

BrowserPpapiHostImpl::~BrowserPpapiHostImpl() {
  // Notify the filter so it won't foward messages to us.
  message_filter_->OnHostDestroyed();

  // Delete the host explicitly first. This shutdown will destroy the
  // resources, which may want to do cleanup in their destructors and expect
  // their pointers to us to be valid.
  ppapi_host_.reset();
}

ppapi::host::PpapiHost* BrowserPpapiHostImpl::GetPpapiHost() {
  return ppapi_host_.get();
}

base::ProcessHandle BrowserPpapiHostImpl::GetPluginProcessHandle() const {
  // Handle should previously have been set before use.
  DCHECK(plugin_process_handle_ != base::kNullProcessHandle);
  return plugin_process_handle_;
}

bool BrowserPpapiHostImpl::IsValidInstance(PP_Instance instance) const {
  return instance_map_.find(instance) != instance_map_.end();
}

bool BrowserPpapiHostImpl::GetRenderViewIDsForInstance(
    PP_Instance instance,
    int* render_process_id,
    int* render_view_id) const {
  InstanceMap::const_iterator found = instance_map_.find(instance);
  if (found == instance_map_.end()) {
    *render_process_id = 0;
    *render_view_id = 0;
    return false;
  }

  *render_process_id = found->second.render_process_id;
  *render_view_id = found->second.render_view_id;
  return true;
}

const std::string& BrowserPpapiHostImpl::GetPluginName() {
  return plugin_name_;
}

const FilePath& BrowserPpapiHostImpl::GetProfileDataDirectory() {
  return profile_data_directory_;
}

GURL BrowserPpapiHostImpl::GetDocumentURLForInstance(PP_Instance instance) {
  InstanceMap::const_iterator found = instance_map_.find(instance);
  if (found == instance_map_.end())
    return GURL();
  return found->second.document_url;
}

GURL BrowserPpapiHostImpl::GetPluginURLForInstance(PP_Instance instance) {
  InstanceMap::const_iterator found = instance_map_.find(instance);
  if (found == instance_map_.end())
    return GURL();
  return found->second.plugin_url;
}

void BrowserPpapiHostImpl::AddInstance(
    PP_Instance instance,
    const PepperRendererInstanceData& instance_data) {
  DCHECK(instance_map_.find(instance) == instance_map_.end());
  instance_map_[instance] = instance_data;
}

void BrowserPpapiHostImpl::DeleteInstance(PP_Instance instance) {
  InstanceMap::iterator found = instance_map_.find(instance);
  if (found == instance_map_.end()) {
    NOTREACHED();
    return;
  }
  instance_map_.erase(found);
}

bool BrowserPpapiHostImpl::HostMessageFilter::OnMessageReceived(
    const IPC::Message& msg) {
  // Don't forward messages if our owner object has been destroyed.
  if (!ppapi_host_)
    return false;

  /* TODO(brettw) when we add messages, here, the code should look like this:
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(BrowserPpapiHostImpl, msg)
    // Add necessary message handlers here.
    IPC_MESSAGE_UNHANDLED(handled = ppapi_host_->OnMessageReceived(msg))
  IPC_END_MESSAGE_MAP();
  return handled;
  */
  return ppapi_host_->OnMessageReceived(msg);
}

void BrowserPpapiHostImpl::HostMessageFilter::OnHostDestroyed() {
  DCHECK(ppapi_host_);
  ppapi_host_ = NULL;
}

}  // namespace content
