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
#include "content/common/browser_plugin_messages.h"
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
      render_view_host_(render_view_host),
      visible_(true) {
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
                                     WebContents* guest_web_contents) {
  DCHECK(guest_web_contents_by_instance_id_.find(instance_id) ==
         guest_web_contents_by_instance_id_.end());
  guest_web_contents_by_instance_id_[instance_id] = guest_web_contents;
}

void BrowserPluginEmbedder::CreateGuest(RenderViewHost* render_view_host,
                                        int instance_id,
                                        std::string storage_partition_id,
                                        bool persist_storage) {
  WebContentsImpl* guest_web_contents = NULL;
  BrowserPluginGuest* guest = GetGuestByInstanceID(instance_id);
  CHECK(!guest);

  const std::string& host =
      render_view_host->GetSiteInstance()->GetSiteURL().host();
  guest_web_contents = WebContentsImpl::CreateGuest(
      web_contents()->GetBrowserContext(),
      host,
      instance_id);

  guest = guest_web_contents->GetBrowserPluginGuest();
  guest->set_embedder_web_contents(web_contents());

  RendererPreferences* guest_renderer_prefs =
      guest_web_contents->GetMutableRendererPrefs();
  // Copy renderer preferences (and nothing else) from the embedder's
  // TabContents to the guest.
  //
  // For GTK and Aura this is necessary to get proper renderer configuration
  // values for caret blinking interval, colors related to selection and
  // focus.
  *guest_renderer_prefs = *web_contents()->GetMutableRendererPrefs();

  guest_renderer_prefs->throttle_input_events = false;
  // Navigation is disabled in Chrome Apps. We want to make sure guest-initiated
  // navigations still continue to function inside the app.
  guest_renderer_prefs->browser_handles_all_top_level_requests = false;
  AddGuest(instance_id, guest_web_contents);
  guest_web_contents->SetDelegate(guest);
}

void BrowserPluginEmbedder::NavigateGuest(
    RenderViewHost* render_view_host,
    int instance_id,
    const std::string& src,
    const BrowserPluginHostMsg_ResizeGuest_Params& resize_params) {
  BrowserPluginGuest* guest = GetGuestByInstanceID(instance_id);
  CHECK(guest);
  GURL url(src);
  WebContentsImpl* guest_web_contents =
      static_cast<WebContentsImpl*>(guest->GetWebContents());

  // We do not load empty urls in web_contents.
  // If a guest sets empty src attribute after it has navigated to some
  // non-empty page, the action is considered no-op. This empty src navigation
  // should never be sent to BrowserPluginEmbedder (browser process).
  DCHECK(!src.empty());
  if (!src.empty()) {
    // TODO(creis): Check the validity of the URL: http://crbug.com/139397.
    guest_web_contents->GetController().LoadURL(url,
                                                Referrer(),
                                                PAGE_TRANSITION_AUTO_SUBFRAME,
                                                std::string());
  }

  // Resize the guest if the resize parameter was set from the renderer.
  ResizeGuest(render_view_host, instance_id, resize_params);
}

void BrowserPluginEmbedder::UpdateRectACK(int instance_id,
                                          int message_id,
                                          const gfx::Size& size) {
  BrowserPluginGuest* guest = GetGuestByInstanceID(instance_id);
  if (guest)
    guest->UpdateRectACK(message_id, size);
}

void BrowserPluginEmbedder::ResizeGuest(
    RenderViewHost* render_view_host,
    int instance_id,
    const BrowserPluginHostMsg_ResizeGuest_Params& params) {
  BrowserPluginGuest* guest = GetGuestByInstanceID(instance_id);
  if (!guest)
    return;
  WebContentsImpl* guest_web_contents =
      static_cast<WebContentsImpl*>(guest->GetWebContents());

  if (!TransportDIB::is_valid_id(params.damage_buffer_id)) {
    // Invalid transport dib, so just resize the WebContents.
    if (params.width && params.height) {
      guest_web_contents->GetView()->SizeContents(gfx::Size(params.width,
                                                            params.height));
    }
    return;
  }

  TransportDIB* damage_buffer = GetDamageBuffer(render_view_host, params);
  guest->SetDamageBuffer(damage_buffer,
#if defined(OS_WIN)
                         params.damage_buffer_size,
#endif
                         gfx::Size(params.width, params.height),
                         params.scale_factor);
  if (!params.resize_pending) {
    guest_web_contents->GetView()->SizeContents(gfx::Size(params.width,
                                                          params.height));
  }
}

TransportDIB* BrowserPluginEmbedder::GetDamageBuffer(
    RenderViewHost* render_view_host,
    const BrowserPluginHostMsg_ResizeGuest_Params& params) {
  TransportDIB* damage_buffer = NULL;
#if defined(OS_WIN)
  // On Windows we need to duplicate the handle from the remote process.
  HANDLE section;
  DuplicateHandle(render_view_host->GetProcess()->GetHandle(),
                  params.damage_buffer_id.handle,
                  GetCurrentProcess(),
                  &section,
                  STANDARD_RIGHTS_REQUIRED | FILE_MAP_READ | FILE_MAP_WRITE,
                  FALSE,
                  0);
  damage_buffer = TransportDIB::Map(section);
#elif defined(OS_MACOSX)
  // On OSX, we need the handle to map the transport dib.
  damage_buffer = TransportDIB::Map(params.damage_buffer_handle);
#elif defined(OS_ANDROID)
  damage_buffer = TransportDIB::Map(params.damage_buffer_id);
#elif defined(OS_POSIX)
  damage_buffer = TransportDIB::Map(params.damage_buffer_id.shmkey);
#endif  // defined(OS_POSIX)
  DCHECK(damage_buffer);
  return damage_buffer;
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
    WebContents* guest_web_contents = guest->GetWebContents();

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
  visible_ = visible;
  // If the embedder is hidden we need to hide the guests as well.
  for (ContainerInstanceMap::const_iterator it =
           guest_web_contents_by_instance_id_.begin();
       it != guest_web_contents_by_instance_id_.end(); ++it) {
    WebContentsImpl* web_contents = static_cast<WebContentsImpl*>(it->second);
    BrowserPluginGuest* guest = web_contents->GetBrowserPluginGuest();
    guest->SetVisibility(visible, guest->visible());
  }
}

void BrowserPluginEmbedder::PluginDestroyed(int instance_id) {
  DestroyGuestByInstanceID(instance_id);
}

void BrowserPluginEmbedder::SetGuestVisibility(int instance_id,
                                               bool guest_visible) {
  BrowserPluginGuest* guest = GetGuestByInstanceID(instance_id);
  if (guest)
    guest->SetVisibility(visible_, guest_visible);
}

void BrowserPluginEmbedder::DragStatusUpdate(
    int instance_id,
    WebKit::WebDragStatus drag_status,
    const WebDropData& drop_data,
    WebKit::WebDragOperationsMask drag_mask,
    const gfx::Point& location) {
  BrowserPluginGuest* guest = GetGuestByInstanceID(instance_id);
  if (guest)
    guest->DragStatusUpdate(drag_status, drop_data, drag_mask, location);
}

void BrowserPluginEmbedder::Go(int instance_id, int relative_index) {
  BrowserPluginGuest* guest = GetGuestByInstanceID(instance_id);
  if (guest)
    guest->Go(relative_index);
}

void BrowserPluginEmbedder::Stop(int instance_id) {
  BrowserPluginGuest* guest = GetGuestByInstanceID(instance_id);
  if (guest)
    guest->Stop();
}

void BrowserPluginEmbedder::Reload(int instance_id) {
  BrowserPluginGuest* guest = GetGuestByInstanceID(instance_id);
  if (guest)
    guest->Reload();
}

void BrowserPluginEmbedder::TerminateGuest(int instance_id) {
  BrowserPluginGuest* guest = GetGuestByInstanceID(instance_id);
  if (guest)
    guest->Terminate();
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
