// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/messaging/extension_message_port.h"

#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/render_process_host.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/process_manager.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/manifest_handlers/background_info.h"

namespace extensions {

ExtensionMessagePort::ExtensionMessagePort(content::RenderProcessHost* process,
                                           int routing_id,
                                           const std::string& extension_id)
     : process_(process),
       routing_id_(routing_id),
       extension_id_(extension_id),
       background_host_ptr_(NULL) {
}

void ExtensionMessagePort::DispatchOnConnect(
    int dest_port_id,
    const std::string& channel_name,
    scoped_ptr<base::DictionaryValue> source_tab,
    int source_frame_id,
    int target_frame_id,
    int guest_process_id,
    const std::string& source_extension_id,
    const std::string& target_extension_id,
    const GURL& source_url,
    const std::string& tls_channel_id) {
  ExtensionMsg_TabConnectionInfo source;
  if (source_tab)
    source.tab.Swap(source_tab.get());
  source.frame_id = source_frame_id;

  ExtensionMsg_ExternalConnectionInfo info;
  info.target_id = target_extension_id;
  info.source_id = source_extension_id;
  info.source_url = source_url;
  info.target_frame_id = target_frame_id;
  info.guest_process_id = guest_process_id;

  process_->Send(new ExtensionMsg_DispatchOnConnect(
      routing_id_, dest_port_id, channel_name, source, info, tls_channel_id));
}

void ExtensionMessagePort::DispatchOnDisconnect(
    int source_port_id,
    const std::string& error_message) {
  process_->Send(new ExtensionMsg_DispatchOnDisconnect(
      routing_id_, source_port_id, error_message));
}

void ExtensionMessagePort::DispatchOnMessage(const Message& message,
                                             int target_port_id) {
  process_->Send(new ExtensionMsg_DeliverMessage(
      routing_id_, target_port_id, message));
}

void ExtensionMessagePort::IncrementLazyKeepaliveCount() {
  Profile* profile =
      Profile::FromBrowserContext(process_->GetBrowserContext());
  extensions::ProcessManager* pm = ProcessManager::Get(profile);
  ExtensionHost* host = pm->GetBackgroundHostForExtension(extension_id_);
  if (host && BackgroundInfo::HasLazyBackgroundPage(host->extension()))
    pm->IncrementLazyKeepaliveCount(host->extension());

  // Keep track of the background host, so when we decrement, we only do so if
  // the host hasn't reloaded.
  background_host_ptr_ = host;
}

void ExtensionMessagePort::DecrementLazyKeepaliveCount() {
  Profile* profile =
      Profile::FromBrowserContext(process_->GetBrowserContext());
  extensions::ProcessManager* pm = ProcessManager::Get(profile);
  ExtensionHost* host = pm->GetBackgroundHostForExtension(extension_id_);
  if (host && host == background_host_ptr_)
    pm->DecrementLazyKeepaliveCount(host->extension());
}

content::RenderProcessHost* ExtensionMessagePort::GetRenderProcessHost() {
  return process_;
}

}  // namespace extensions
