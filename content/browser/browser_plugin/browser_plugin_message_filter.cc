// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browser_plugin/browser_plugin_message_filter.h"

#include "content/common/view_messages.h"

namespace content {

BrowserPluginMessageFilter::BrowserPluginMessageFilter(
    int render_process_id,
    BrowserContext* browser_context)
    : render_process_id_(render_process_id) {
}

BrowserPluginMessageFilter::~BrowserPluginMessageFilter() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
}

bool BrowserPluginMessageFilter::OnMessageReceived(
    const IPC::Message& message,
    bool* message_was_ok) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(BrowserPluginMessageFilter, message, *message_was_ok)
    IPC_MESSAGE_HANDLER(ViewHostMsg_CreateWindow, OnCreateWindow)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void BrowserPluginMessageFilter::OnCreateWindow(
    const ViewHostMsg_CreateWindow_Params& params,
    int* route_id,
    int* surface_id,
    int64* cloned_session_storage_namespace_id) {
  // TODO(fsamuel): We do not currently support window.open.
  // See http://crbug.com/140316.
  *route_id = MSG_ROUTING_NONE;
  *surface_id = 0;
}

} // namespace content
