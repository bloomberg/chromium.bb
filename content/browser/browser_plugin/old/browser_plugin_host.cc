// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browser_plugin/old/browser_plugin_host.h"

#include "content/browser/browser_plugin/old/browser_plugin_host_helper.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/browser_plugin_messages.h"
#include "content/public/common/url_constants.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents_view.h"
#include "ppapi/proxy/ppapi_messages.h"

namespace content {

BrowserPluginHost::BrowserPluginHost(
    WebContentsImpl* web_contents)
    : WebContentsObserver(web_contents),
      embedder_render_process_host_(NULL),
      instance_id_(0) {
  // Listen to visibility changes so that an embedder hides its guests
  // as well.
  registrar_.Add(this,
                 NOTIFICATION_WEB_CONTENTS_VISIBILITY_CHANGED,
                 Source<WebContents>(web_contents));
  // Construct plumbing helpers when a new RenderViewHost is created for
  // this BrowserPluginHost's WebContentsImpl.
  registrar_.Add(this,
                 NOTIFICATION_RENDER_VIEW_HOST_CREATED_FOR_TAB,
                 Source<WebContents>(web_contents));
}

BrowserPluginHost::~BrowserPluginHost() {
}

BrowserPluginHost* BrowserPluginHost::GetGuestByContainerID(int container_id) {
  ContainerInstanceMap::const_iterator it =
      guests_by_container_id_.find(container_id);
  if (it != guests_by_container_id_.end())
    return it->second;
  return NULL;
}

void BrowserPluginHost::RegisterContainerInstance(
    int container_id,
    BrowserPluginHost* observer) {
  DCHECK(guests_by_container_id_.find(container_id) ==
      guests_by_container_id_.end());
  guests_by_container_id_[container_id] = observer;
}

bool BrowserPluginHost::TakeFocus(bool reverse) {
  embedder_render_process_host()->Send(
      new BrowserPluginMsg_AdvanceFocus(instance_id(), reverse));
  return true;
}

bool BrowserPluginHost::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(BrowserPluginHost, message)
    IPC_MESSAGE_HANDLER(BrowserPluginHostMsg_NavigateFromGuest,
                        OnNavigateFromGuest)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void BrowserPluginHost::NavigateGuestFromEmbedder(
    RenderViewHost* render_view_host,
    int container_instance_id,
    long long frame_id,
    const std::string& src) {
  BrowserPluginHost* guest_observer =
      GetGuestByContainerID(container_instance_id);
  WebContentsImpl* guest_web_contents =
      guest_observer ?
          static_cast<WebContentsImpl*>(guest_observer->web_contents()): NULL;
  GURL url(src);
  if (!guest_observer) {
    std::string host = render_view_host->GetSiteInstance()->GetSite().host();
    GURL guest_url(
        base::StringPrintf("%s://%s", chrome::kGuestScheme, host.c_str()));
    // The SiteInstance of a given guest is based on the fact that it's a guest
    // in addition to which platform application the guest belongs to, rather
    // than the URL that the guest is being navigated to.
    SiteInstance* guest_site_instance =
        SiteInstance::CreateForURL(web_contents()->GetBrowserContext(),
        guest_url);
    guest_web_contents =
        static_cast<WebContentsImpl*>(
            WebContents::Create(
                web_contents()->GetBrowserContext(),
                guest_site_instance,
                MSG_ROUTING_NONE,
                NULL, // base WebContents
                NULL  // session storage namespace
            ));
    guest_observer =
        guest_web_contents->browser_plugin_host();
    guest_observer->set_embedder_render_process_host(
        render_view_host->GetProcess());
    guest_observer->set_instance_id(container_instance_id);
    RegisterContainerInstance(container_instance_id, guest_observer);
    AddGuest(guest_web_contents, frame_id);
  }
  guest_observer->web_contents()->SetDelegate(guest_observer);
  guest_observer->web_contents()->GetController().LoadURL(
      url,
      Referrer(),
      PAGE_TRANSITION_AUTO_SUBFRAME,
      std::string());
}

void BrowserPluginHost::OnNavigateFromGuest(
    PP_Instance instance,
    const std::string& src) {
  DCHECK(embedder_render_process_host());
  GURL url(src);
  web_contents()->GetController().LoadURL(
      url,
      Referrer(),
      PAGE_TRANSITION_AUTO_SUBFRAME,
      std::string());
}

void BrowserPluginHost::ConnectEmbedderToChannel(
    RenderViewHost* render_view_host,
    const IPC::ChannelHandle& channel_handle) {
  DCHECK(embedder_render_process_host());
  // Tell the BrowserPlugin in the embedder that we're done and that it can
  // begin using the guest renderer.
  embedder_render_process_host()->Send(
      new BrowserPluginMsg_LoadGuest(
          instance_id(),
          render_view_host->GetProcess()->
              GetID(),
          channel_handle));
}

void BrowserPluginHost::AddGuest(WebContentsImpl* guest, int64 frame_id) {
  guests_[guest] = frame_id;
}

void BrowserPluginHost::RemoveGuest(WebContentsImpl* guest) {
  guests_.erase(guest);
}

void BrowserPluginHost::DestroyGuests() {
  for (GuestMap::const_iterator it = guests_.begin();
       it != guests_.end(); ++it) {
    WebContentsImpl* web_contents = it->first;
    delete web_contents;
  }
  guests_.clear();
  guests_by_container_id_.clear();
}

void BrowserPluginHost::DidCommitProvisionalLoadForFrame(
    int64 frame_id,
    bool is_main_frame,
    const GURL& url,
    PageTransition transition_type,
    RenderViewHost* render_view_host) {
  // Clean-up guests that lie in the frame that we're navigating.
  typedef std::set<WebContentsImpl*> GuestSet;
  GuestSet guests_to_delete;
  for (GuestMap::const_iterator it = guests_.begin();
       it != guests_.end(); ++it) {
    WebContentsImpl* web_contents = it->first;
    if (it->second == frame_id) {
      guests_to_delete.insert(web_contents);
    }
  }
  for (GuestSet::const_iterator it = guests_to_delete.begin();
       it != guests_to_delete.end(); ++it) {
    delete *it;
    guests_.erase(*it);
  }
}

void BrowserPluginHost::RenderViewDeleted(RenderViewHost* render_view_host) {
  // TODO(fsamuel): For some reason ToT hangs when killing a guest, this wasn't
  // the case earlier. Presumably, when a guest is killed/crashes, one of
  // RenderViewDeleted, RenderViewGone or WebContentsDestroyed will get called.
  // At that point, we should remove reference to this BrowserPluginHost
  // from its embedder, in addition to destroying its guests, to ensure
  // that we do not attempt a double delete.
  DestroyGuests();
}

void BrowserPluginHost::RenderViewGone(base::TerminationStatus status) {
  DestroyGuests();
}

void BrowserPluginHost::WebContentsDestroyed(WebContents* web_contents) {
  DestroyGuests();
}

void BrowserPluginHost::Observe(
    int type,
    const NotificationSource& source,
    const NotificationDetails& details) {
  switch (type) {
    case NOTIFICATION_RENDER_VIEW_HOST_CREATED_FOR_TAB: {
      RenderViewHost* render_view_host =
          Details<RenderViewHost>(details).ptr();
      // BrowserPluginHostHelper is destroyed when its associated RenderViewHost
      // is destroyed.
      new BrowserPluginHostHelper(this, render_view_host);
      break;
    }
    case NOTIFICATION_WEB_CONTENTS_VISIBILITY_CHANGED: {
      bool visible = *Details<bool>(details).ptr();
      // If the embedder is hidden we need to hide the guests as well.
      for (GuestMap::const_iterator it = guests_.begin();
          it != guests_.end(); ++it) {
        WebContentsImpl* web_contents = it->first;
        if (visible)
          web_contents->WasRestored();
        else
          web_contents->WasHidden();
      }
      break;
    }
    default:
      NOTREACHED() << "Unexpected notification type: " << type;
  }
}

}  // namespace content
