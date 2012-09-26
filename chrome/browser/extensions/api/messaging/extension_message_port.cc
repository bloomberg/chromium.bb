// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/messaging/extension_message_port.h"

#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extension_process_manager.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/extension_messages.h"
#include "content/public/browser/render_process_host.h"

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
    const std::string& tab_json,
    const std::string& source_extension_id,
    const std::string& target_extension_id) {
  process_->Send(new ExtensionMsg_DispatchOnConnect(
      routing_id_, dest_port_id, channel_name,
      tab_json, source_extension_id, target_extension_id));
}

void ExtensionMessagePort::DispatchOnDisconnect(int source_port_id,
                                                bool connection_error) {
  process_->Send(new ExtensionMsg_DispatchOnDisconnect(
      routing_id_, source_port_id, connection_error));
}

void ExtensionMessagePort::DispatchOnMessage(const std::string& message,
                                             int target_port_id) {
    process_->Send(new ExtensionMsg_DeliverMessage(
        routing_id_, target_port_id, message));
}

void ExtensionMessagePort::IncrementLazyKeepaliveCount() {
  Profile* profile =
      Profile::FromBrowserContext(process_->GetBrowserContext());
  ExtensionProcessManager* pm =
      ExtensionSystem::Get(profile)->process_manager();
  ExtensionHost* host = pm->GetBackgroundHostForExtension(extension_id_);
  if (host && host->extension()->has_lazy_background_page())
    pm->IncrementLazyKeepaliveCount(host->extension());

  // Keep track of the background host, so when we decrement, we only do so if
  // the host hasn't reloaded.
  background_host_ptr_ = host;
}

void ExtensionMessagePort::DecrementLazyKeepaliveCount() {
  Profile* profile =
      Profile::FromBrowserContext(process_->GetBrowserContext());
  ExtensionProcessManager* pm =
      ExtensionSystem::Get(profile)->process_manager();
  ExtensionHost* host = pm->GetBackgroundHostForExtension(extension_id_);
  if (host && host == background_host_ptr_)
    pm->DecrementLazyKeepaliveCount(host->extension());
}

content::RenderProcessHost* ExtensionMessagePort::GetRenderProcessHost() {
  return process_;
}

}  // namespace extensions
