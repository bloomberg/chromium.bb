// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/pepper/browser_ppapi_host_impl.h"

#include "ipc/ipc_message_macros.h"

namespace content {

BrowserPpapiHostImpl::BrowserPpapiHostImpl(
    IPC::Sender* sender,
    const ppapi::PpapiPermissions& permissions)
    : host_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)),
      ppapi_host_(sender, &host_factory_, permissions),
      plugin_process_handle_(base::kNullProcessHandle) {
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

}  // namespace content
