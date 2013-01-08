// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browser_plugin/browser_plugin_message_filter.h"

#include "content/browser/browser_plugin/browser_plugin_guest.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/view_messages.h"
#include "content/public/browser/render_view_host.h"

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
    IPC_MESSAGE_HANDLER_GENERIC(ViewHostMsg_CreateWindow,
                                OnCreateWindow(message))
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void BrowserPluginMessageFilter::OverrideThreadForMessage(
    const IPC::Message& message, BrowserThread::ID* thread) {
  if (message.type() == ViewHostMsg_CreateWindow::ID)
    *thread = BrowserThread::UI;
}

void BrowserPluginMessageFilter::OnCreateWindow(const IPC::Message& message) {
  // Special case: For ViewHostMsg_CreateWindow, we route based on the contents
  // of the message.
  PickleIterator iter = IPC::SyncMessage::GetDataIterator(&message);
  ViewHostMsg_CreateWindow_Params params;
  if (!IPC::ReadParam(&message, &iter, &params)) {
    NOTREACHED();
    return;
  }
  RenderViewHost* rvh = RenderViewHost::FromID(render_process_id_,
                                               params.opener_id);
  WebContentsImpl* web_contents = static_cast<WebContentsImpl*>(
      WebContents::FromRenderViewHost(rvh));
  BrowserPluginGuest* guest = web_contents->GetBrowserPluginGuest();
  guest->OnMessageReceived(message);
}

} // namespace content
