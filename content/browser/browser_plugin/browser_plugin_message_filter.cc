// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browser_plugin/browser_plugin_message_filter.h"

#include "base/supports_user_data.h"
#include "content/browser/browser_plugin/browser_plugin_guest.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/browser_plugin/browser_plugin_constants.h"
#include "content/common/browser_plugin/browser_plugin_messages.h"
#include "content/common/view_messages.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_plugin_guest_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_view_host.h"

namespace content {

BrowserPluginMessageFilter::BrowserPluginMessageFilter(int render_process_id)
    : BrowserMessageFilter(BrowserPluginMsgStart),
      render_process_id_(render_process_id) {
}

BrowserPluginMessageFilter::~BrowserPluginMessageFilter() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
}

bool BrowserPluginMessageFilter::OnMessageReceived(
    const IPC::Message& message) {
  // Any message requested by a BrowserPluginGuest should be routed through
  // a BrowserPluginGuestManager.
  if (BrowserPluginGuest::ShouldForwardToBrowserPluginGuest(message)) {
    ForwardMessageToGuest(message);
    // We always swallow messages destined for BrowserPluginGuestManager because
    // we're on the UI thread and fallback code is expected to be run on the IO
    // thread.
    return true;
  }
  return false;
}

void BrowserPluginMessageFilter::OnDestruct() const {
  BrowserThread::DeleteOnIOThread::Destruct(this);
}

void BrowserPluginMessageFilter::OverrideThreadForMessage(
    const IPC::Message& message, BrowserThread::ID* thread) {
  if (BrowserPluginGuest::ShouldForwardToBrowserPluginGuest(message))
    *thread = BrowserThread::UI;
}

void BrowserPluginMessageFilter::ForwardMessageToGuest(
    const IPC::Message& message) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  RenderViewHost* rvh = RenderViewHost::FromID(render_process_id_,
                                               message.routing_id());
  if (!rvh)
    return;

  WebContents* embedder_web_contents = WebContents::FromRenderViewHost(rvh);

  int browser_plugin_instance_id = 0;
  // All allowed messages must have instance_id as their first parameter.
  PickleIterator iter(message);
  bool success = iter.ReadInt(&browser_plugin_instance_id);
  DCHECK(success);
  WebContents* guest_web_contents =
      embedder_web_contents->GetBrowserContext()
          ->GetGuestManager()
          ->GetGuestByInstanceID(embedder_web_contents,
                                 browser_plugin_instance_id);
  if (!guest_web_contents)
    return;

  static_cast<WebContentsImpl*>(guest_web_contents)
      ->GetBrowserPluginGuest()
      ->OnMessageReceivedFromEmbedder(message);
}

} // namespace content
