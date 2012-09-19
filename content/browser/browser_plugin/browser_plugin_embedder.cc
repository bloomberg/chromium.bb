// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browser_plugin/browser_plugin_embedder.h"

#include <set>

#include "base/logging.h"
#include "base/stl_util.h"
#include "base/time.h"
#include "content/browser/browser_plugin/browser_plugin_embedder_helper.h"
#include "content/browser/browser_plugin/browser_plugin_guest.h"
#include "content/browser/browser_plugin/browser_plugin_host_factory.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/web_contents_view.h"
#include "content/public/common/url_constants.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputEvent.h"
#include "ui/gfx/size.h"
#include "ui/surface/transport_dib.h"

namespace content {

// static
BrowserPluginHostFactory* BrowserPluginEmbedder::factory_ = NULL;

BrowserPluginEmbedder::BrowserPluginEmbedder(
    WebContentsImpl* web_contents,
    RenderViewHost* render_view_host)
    : WebContentsObserver(web_contents),
      render_view_host_(render_view_host) {
  // Listen to visibility changes so that an embedder hides its guests
  // as well.
  registrar_.Add(this,
                 NOTIFICATION_WEB_CONTENTS_VISIBILITY_CHANGED,
                 Source<WebContents>(web_contents));

  // |render_view_host| manages the ownership of this BrowserPluginGuestHelper.
  new BrowserPluginEmbedderHelper(this, render_view_host);
}

BrowserPluginEmbedder::~BrowserPluginEmbedder() {
  // Destroy guests that are managed by the current embedder.
  DestroyGuests();
}

// static
BrowserPluginEmbedder* BrowserPluginEmbedder::Create(
    WebContentsImpl* web_contents,
    content::RenderViewHost* render_view_host) {
  if (factory_) {
    return factory_->CreateBrowserPluginEmbedder(web_contents,
                                                 render_view_host);
  }
  return new BrowserPluginEmbedder(web_contents, render_view_host);
}

BrowserPluginGuest* BrowserPluginEmbedder::GetGuestByInstanceID(
    int instance_id) const {
  ContainerInstanceMap::const_iterator it =
      guest_web_contents_by_instance_id_.find(instance_id);
  if (it != guest_web_contents_by_instance_id_.end())
    return static_cast<WebContentsImpl*>(it->second)->GetBrowserPluginGuest();
  return NULL;
}

void BrowserPluginEmbedder::AddGuest(int instance_id,
                                     WebContents* guest_web_contents,
                                     int64 frame_id) {
  DCHECK(guest_web_contents_by_instance_id_.find(instance_id) ==
         guest_web_contents_by_instance_id_.end());
  guest_web_contents_by_instance_id_[instance_id] = guest_web_contents;
}

void BrowserPluginEmbedder::NavigateGuest(RenderViewHost* render_view_host,
                                          int instance_id,
                                          int64 frame_id,
                                          const std::string& src,
                                          const gfx::Size& size) {
  BrowserPluginGuest* guest = GetGuestByInstanceID(instance_id);
  WebContentsImpl* guest_web_contents = NULL;
  GURL url(src);
  if (!guest) {
    const std::string& host =
        render_view_host->GetSiteInstance()->GetSite().host();
    guest_web_contents = WebContentsImpl::CreateGuest(
        web_contents()->GetBrowserContext(),
        host,
        instance_id);

    guest = guest_web_contents->GetBrowserPluginGuest();
    guest->set_embedder_render_process_host(
        render_view_host->GetProcess());

    guest_web_contents->GetMutableRendererPrefs()->
        throttle_input_events = false;
    AddGuest(instance_id, guest_web_contents, frame_id);
    guest_web_contents->SetDelegate(guest);
  } else {
    guest_web_contents = static_cast<WebContentsImpl*>(guest->web_contents());
  }

  // We ignore loading empty urls in web_contents.
  // If a guest sets empty src attribute after it has navigated to some
  // non-empty page, the action is considered no-op.
  // TODO(lazyboy): The js shim for browser-plugin might need to reflect empty
  // src ignoring in the shadow DOM element: http://crbug.com/149001.
  if (!src.empty()) {
    guest_web_contents->GetController().LoadURL(url,
                                                Referrer(),
                                                PAGE_TRANSITION_AUTO_SUBFRAME,
                                                std::string());
  }

  if (!size.IsEmpty())
    guest_web_contents->GetView()->SizeContents(size);
}

void BrowserPluginEmbedder::UpdateRectACK(int instance_id,
                                          int message_id,
                                          const gfx::Size& size) {
  BrowserPluginGuest* guest = GetGuestByInstanceID(instance_id);
  if (guest)
    guest->UpdateRectACK(message_id, size);
}

void BrowserPluginEmbedder::ResizeGuest(int instance_id,
                                        TransportDIB* damage_buffer,
#if defined(OS_WIN)
                                        int damage_buffer_size,
#endif
                                        int width,
                                        int height,
                                        bool resize_pending,
                                        float scale_factor) {
  BrowserPluginGuest* guest = GetGuestByInstanceID(instance_id);
  if (!guest)
    return;
  WebContentsImpl* guest_web_contents =
      static_cast<WebContentsImpl*>(guest->web_contents());
  guest->SetDamageBuffer(damage_buffer,
#if defined(OS_WIN)
                         damage_buffer_size,
#endif
                         gfx::Size(width, height),
                         scale_factor);
  if (!resize_pending)
    guest_web_contents->GetView()->SizeContents(gfx::Size(width, height));
}

void BrowserPluginEmbedder::SetFocus(int instance_id,
                                     bool focused) {
  BrowserPluginGuest* guest = GetGuestByInstanceID(instance_id);
  if (guest)
    guest->SetFocus(focused);
}

void BrowserPluginEmbedder::DestroyGuests() {
  STLDeleteContainerPairSecondPointers(
      guest_web_contents_by_instance_id_.begin(),
      guest_web_contents_by_instance_id_.end());
  guest_web_contents_by_instance_id_.clear();
}

void BrowserPluginEmbedder::HandleInputEvent(int instance_id,
                                             RenderViewHost* render_view_host,
                                             const gfx::Rect& guest_rect,
                                             const WebKit::WebInputEvent& event,
                                             IPC::Message* reply_message) {
  BrowserPluginGuest* guest = GetGuestByInstanceID(instance_id);
  if (guest)
    guest->HandleInputEvent(render_view_host, guest_rect, event, reply_message);
}

void BrowserPluginEmbedder::DestroyGuestByInstanceID(int instance_id) {
  BrowserPluginGuest* guest = GetGuestByInstanceID(instance_id);
  if (guest) {
    WebContents* guest_web_contents = guest->web_contents();

    // Destroy the guest's web_contents.
    delete guest_web_contents;
    guest_web_contents_by_instance_id_.erase(instance_id);
  }
}

void BrowserPluginEmbedder::RenderViewDeleted(
    RenderViewHost* render_view_host) {
  DestroyGuests();
}

void BrowserPluginEmbedder::RenderViewGone(base::TerminationStatus status) {
  DestroyGuests();
}

void BrowserPluginEmbedder::WebContentsVisibilityChanged(bool visible) {
  // If the embedder is hidden we need to hide the guests as well.
  for (ContainerInstanceMap::const_iterator it =
           guest_web_contents_by_instance_id_.begin();
       it != guest_web_contents_by_instance_id_.end(); ++it) {
    WebContents* web_contents = it->second;
    if (visible)
      web_contents->WasShown();
    else
      web_contents->WasHidden();
  }
}

void BrowserPluginEmbedder::PluginDestroyed(int instance_id) {
  DestroyGuestByInstanceID(instance_id);
}

void BrowserPluginEmbedder::Observe(int type,
                                    const NotificationSource& source,
                                    const NotificationDetails& details) {
  switch (type) {
    case NOTIFICATION_WEB_CONTENTS_VISIBILITY_CHANGED: {
      bool visible = *Details<bool>(details).ptr();
      WebContentsVisibilityChanged(visible);
      break;
    }
    default:
      NOTREACHED() << "Unexpected notification type: " << type;
  }
}

}  // namespace content
