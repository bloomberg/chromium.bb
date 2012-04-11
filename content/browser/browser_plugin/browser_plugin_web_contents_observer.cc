// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browser_plugin/browser_plugin_web_contents_observer.h"

#include "base/lazy_instance.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/browser_plugin_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "ppapi/proxy/ppapi_messages.h"

namespace content {

BrowserPluginWebContentsObserver::BrowserPluginWebContentsObserver(
    TabContents* tab_contents)
    : WebContentsObserver(tab_contents),
      host_(NULL),
      instance_id_(0) {
  registrar_.Add(this,
                 NOTIFICATION_RENDER_WIDGET_VISIBILITY_CHANGED,
                 Source<RenderViewHost>(
                    tab_contents->GetRenderViewHost()));
}

BrowserPluginWebContentsObserver::~BrowserPluginWebContentsObserver() {
}

bool BrowserPluginWebContentsObserver::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(BrowserPluginWebContentsObserver, message)
    IPC_MESSAGE_HANDLER(BrowserPluginHostMsg_ChannelCreated,
                        OnRendererPluginChannelCreated)
    IPC_MESSAGE_HANDLER(BrowserPluginHostMsg_OpenChannel,
                        OnOpenChannelToBrowserPlugin)
    IPC_MESSAGE_HANDLER(BrowserPluginHostMsg_GuestReady, OnGuestReady)
    IPC_MESSAGE_HANDLER(BrowserPluginHostMsg_ResizeGuest, OnResizeGuest)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void BrowserPluginWebContentsObserver::OnGuestReady() {
  // We only care about handling this message on guests.
  if (!host())
    return;

  // The renderer is now ready to receive ppapi messages.
  // Let's tell it create a channel with the embedder/host.
  BrowserPluginMsg_CreateChannel* msg =
      new BrowserPluginMsg_CreateChannel(
          host()->GetRenderProcessHost()->GetHandle(),
          host()->GetRenderProcessHost()->GetID());
  msg->set_routing_id(web_contents()->GetRenderViewHost()->GetRoutingID());
  // TODO(fsamuel): If we aren't able to successfully send this message
  // to the guest then that probably means it crashed. Is there anything
  // we can do that's cleaner than failing a check?
  CHECK(Send(msg));
}

void BrowserPluginWebContentsObserver::OnResizeGuest(
    int width, int height) {
  // Tell the RenderWidgetHostView to adjust its size.
  web_contents()->GetRenderViewHost()->GetView()->SetSize(
      gfx::Size(width, height));
}

void BrowserPluginWebContentsObserver::OnOpenChannelToBrowserPlugin(
    int instance_id,
    long long frame_id,
    const std::string& src,
    const gfx::Size& size) {
  BrowserContext* browser_context =
      web_contents()->GetRenderViewHost()->GetProcess()->GetBrowserContext();
  DCHECK(browser_context);

  GURL url(src);
  SiteInstance* site_instance =
      SiteInstance::CreateForURL(
          browser_context, url);
  TabContents* guest_tab_contents =
      static_cast<TabContents*>(
          WebContents::Create(
              browser_context,
              site_instance,
              MSG_ROUTING_NONE,
              NULL, // base tab contents
              NULL  // session storage namespace
          ));
  // TODO(fsamuel): Set the WebContentsDelegate here.
  RenderViewHostImpl* guest_render_view_host =
      static_cast<RenderViewHostImpl*>(
          guest_tab_contents->GetRenderViewHost());

  // We need to make sure that the RenderViewHost knows that it's
  // hosting a guest RenderView so that it informs the RenderView
  // on a ViewMsg_New. Guest RenderViews will avoid compositing
  // until a guest-to-host channel has been initialized.
  guest_render_view_host->set_guest(true);

  guest_tab_contents->GetController().LoadURL(
      url,
      Referrer(),
      PAGE_TRANSITION_HOME_PAGE,
      std::string());

  guest_render_view_host->GetView()->SetSize(size);
  BrowserPluginWebContentsObserver* guest_observer =
      guest_tab_contents->browser_plugin_web_contents_observer();
  guest_observer->set_host(static_cast<TabContents*>(web_contents()));
  guest_observer->set_instance_id(instance_id);

  AddGuest(guest_tab_contents, frame_id);
}

void BrowserPluginWebContentsObserver::OnRendererPluginChannelCreated(
    const IPC::ChannelHandle& channel_handle) {
  DCHECK(host());
  // Prepare the handle to send to the renderer.
  base::ProcessHandle plugin_process =
      web_contents()->GetRenderProcessHost()->GetHandle();
#if defined(OS_WIN)
  base::ProcessHandle renderer_process =
      host()->GetRenderProcessHost()->GetHandle();
  int renderer_id = host()->GetRenderProcessHost()->GetID();

  base::ProcessHandle renderers_plugin_handle = NULL;
  ::DuplicateHandle(::GetCurrentProcess(), plugin_process,
                    renderer_process, &renderers_plugin_handle,
                    0, FALSE, DUPLICATE_SAME_ACCESS);
#elif defined(OS_POSIX)
  // Don't need to duplicate anything on POSIX since it's just a PID.
  base::ProcessHandle renderers_plugin_handle = plugin_process;
#endif
  // Tell the BrowserPLuginPlaceholder in the host that we're done
  // and that it can begin using the guest renderer.
  host()->GetRenderProcessHost()->Send(
      new BrowserPluginMsg_GuestReady_ACK(
          host()->GetRenderViewHost()->GetRoutingID(),
          instance_id(),
          renderers_plugin_handle,
          channel_handle));
}

void BrowserPluginWebContentsObserver::AddGuest(
    TabContents* guest,
    int64 frame_id) {
  guests_[guest] = frame_id;
}

void BrowserPluginWebContentsObserver::RemoveGuest(TabContents* guest) {
  guests_.erase(guest);
}

void BrowserPluginWebContentsObserver::DestroyGuests() {
  for (GuestMap::const_iterator it = guests_.begin();
       it != guests_.end(); ++it) {
    const TabContents* contents = it->first;
    delete contents;
  }
  guests_.clear();
}

void BrowserPluginWebContentsObserver::DidCommitProvisionalLoadForFrame(
    int64 frame_id,
    bool is_main_frame,
    const GURL& url,
    PageTransition transition_type) {
  typedef std::set<TabContents*> GuestSet;
  GuestSet guests_to_delete;
  for (GuestMap::const_iterator it = guests_.begin();
       it != guests_.end(); ++it) {
    TabContents* contents = it->first;
    if (it->second == frame_id) {
      guests_to_delete.insert(contents);
    }
  }
  for (GuestSet::const_iterator it = guests_to_delete.begin();
       it != guests_to_delete.end(); ++it) {
    delete *it;
    guests_.erase(*it);
  }
}

void BrowserPluginWebContentsObserver::RenderViewDeleted(
    RenderViewHost* render_view_host) {
  DestroyGuests();
}

void BrowserPluginWebContentsObserver::RenderViewGone(
    base::TerminationStatus status) {
  DestroyGuests();
}

void BrowserPluginWebContentsObserver::WebContentsDestroyed(
    WebContents* web_contents) {
  DestroyGuests();
}

void BrowserPluginWebContentsObserver::Observe(
    int type,
    const NotificationSource& source,
    const NotificationDetails& details) {
  DCHECK(type == NOTIFICATION_RENDER_WIDGET_VISIBILITY_CHANGED);
  bool visible = *Details<bool>(details).ptr();

  // If the host is hidden we need to hide the guests as well.
  for (GuestMap::const_iterator it = guests_.begin();
       it != guests_.end(); ++it) {
    TabContents* contents = it->first;
    if (visible)
      contents->ShowContents();
    else
      contents->HideContents();
  }
}

}  // namespace content
