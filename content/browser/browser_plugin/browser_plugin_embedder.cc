// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browser_plugin/browser_plugin_embedder.h"

#include "base/stl_util.h"
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
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/common/result_codes.h"
#include "content/public/common/url_constants.h"
#include "net/base/escape.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputEvent.h"
#include "ui/gfx/size.h"

namespace content {

// static
BrowserPluginHostFactory* BrowserPluginEmbedder::factory_ = NULL;

BrowserPluginEmbedder::BrowserPluginEmbedder(
    WebContentsImpl* web_contents,
    RenderViewHost* render_view_host)
    : WebContentsObserver(web_contents),
      render_view_host_(render_view_host),
      visible_(true),
      next_get_render_view_request_id_(0) {
  // Listen to visibility changes so that an embedder hides its guests
  // as well.
  registrar_.Add(this,
                 NOTIFICATION_WEB_CONTENTS_VISIBILITY_CHANGED,
                 Source<WebContents>(web_contents));

  // |render_view_host| manages the ownership of this BrowserPluginGuestHelper.
  new BrowserPluginEmbedderHelper(this, render_view_host);
}

BrowserPluginEmbedder::~BrowserPluginEmbedder() {
  CleanUp();
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

void BrowserPluginEmbedder::CreateGuest(
    RenderViewHost* render_view_host,
    int instance_id,
    const BrowserPluginHostMsg_CreateGuest_Params& params) {
  WebContentsImpl* guest_web_contents = NULL;
  BrowserPluginGuest* guest = GetGuestByInstanceID(instance_id);
  CHECK(!guest);

  // Validate that the partition id coming from the renderer is valid UTF-8,
  // since we depend on this in other parts of the code, such as FilePath
  // creation. If the validation fails, treat it as a bad message and kill the
  // renderer process.
  if (!IsStringUTF8(params.storage_partition_id)) {
    content::RecordAction(UserMetricsAction("BadMessageTerminate_BPE"));
    base::KillProcess(render_view_host->GetProcess()->GetHandle(),
                      content::RESULT_CODE_KILLED_BAD_MESSAGE, false);
    return;
  }

  const std::string& host =
      render_view_host->GetSiteInstance()->GetSiteURL().host();
  std::string url_encoded_partition = net::EscapeQueryParamValue(
      params.storage_partition_id, false);

  // The SiteInstance of a given webview tag is based on the fact that it's a
  // guest process in addition to which platform application the tag belongs to
  // and what storage partition is in use, rather than the URL that the tag is
  // being navigated to.
  GURL guest_site(
      base::StringPrintf("%s://%s/%s?%s", chrome::kGuestScheme,
                         host.c_str(), params.persist_storage ? "persist" : "",
                         url_encoded_partition.c_str()));

  // If we already have a webview tag in the same app using the same storage
  // partition, we should use the same SiteInstance so the existing tag and
  // the new tag can script each other.
  SiteInstance* guest_site_instance = NULL;
  for (ContainerInstanceMap::const_iterator it =
           guest_web_contents_by_instance_id_.begin();
       it != guest_web_contents_by_instance_id_.end(); ++it) {
    if (it->second->GetSiteInstance()->GetSiteURL() == guest_site) {
      guest_site_instance = it->second->GetSiteInstance();
      break;
    }
  }
  if (!guest_site_instance) {
    // Create the SiteInstance in a new BrowsingInstance, which will ensure that
    // webview tags are also not allowed to send messages across different
    // partitions.
    guest_site_instance = SiteInstance::CreateForURL(
        web_contents()->GetBrowserContext(), guest_site);
  }

  guest_web_contents = WebContentsImpl::CreateGuest(
      web_contents()->GetBrowserContext(),
      guest_site_instance,
      instance_id,
      params);

  guest = guest_web_contents->GetBrowserPluginGuest();
  guest->set_embedder_web_contents(
      static_cast<WebContentsImpl*>(web_contents()));

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

  // Create a swapped out RenderView for the guest in the embedder render
  // process, so that the embedder can access the guest's window object.
  int guest_routing_id =
      static_cast<WebContentsImpl*>(guest->GetWebContents())->
            CreateSwappedOutRenderView(web_contents()->GetSiteInstance());
  render_view_host->Send(new BrowserPluginMsg_GuestContentWindowReady(
      render_view_host->GetRoutingID(), instance_id, guest_routing_id));

  guest->SetSize(params.auto_size_params, params.resize_guest_params);
}

void BrowserPluginEmbedder::NavigateGuest(
    RenderViewHost* render_view_host,
    int instance_id,
    const std::string& src) {
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
    // Because guests do not swap processes on navigation, only navigations to
    // normal web URLs are supported.  No protocol handlers are installed for
    // other schemes (e.g., WebUI or extensions), and no permissions or bindings
    // can be granted to the guest process.
    guest_web_contents->GetController().LoadURL(url,
                                                Referrer(),
                                                PAGE_TRANSITION_AUTO_TOPLEVEL,
                                                std::string());
  }
}

void BrowserPluginEmbedder::UpdateRectACK(
    int instance_id,
    int message_id,
    const BrowserPluginHostMsg_AutoSize_Params& auto_size_params,
    const BrowserPluginHostMsg_ResizeGuest_Params& resize_guest_params) {
  BrowserPluginGuest* guest = GetGuestByInstanceID(instance_id);
  if (guest)
    guest->UpdateRectACK(message_id, auto_size_params, resize_guest_params);
}

void BrowserPluginEmbedder::ResizeGuest(
    RenderViewHost* render_view_host,
    int instance_id,
    const BrowserPluginHostMsg_ResizeGuest_Params& params) {
  BrowserPluginGuest* guest = GetGuestByInstanceID(instance_id);
  if (guest)
    guest->Resize(render_view_host, params);
}

void BrowserPluginEmbedder::SetFocus(int instance_id,
                                     bool focused) {
  BrowserPluginGuest* guest = GetGuestByInstanceID(instance_id);
  if (guest)
    guest->SetFocus(focused);
}

void BrowserPluginEmbedder::CleanUp() {
  // Destroy guests that are managed by the current embedder.
  STLDeleteContainerPairSecondPointers(
      guest_web_contents_by_instance_id_.begin(),
      guest_web_contents_by_instance_id_.end());
  guest_web_contents_by_instance_id_.clear();

  // CleanUp gets called when BrowserPluginEmbedder's WebContents goes away
  // or the associated RenderViewHost is destroyed or swapped out. Therefore we
  // don't need to care about the pending callbacks anymore.
  pending_get_render_view_callbacks_.clear();
}

void BrowserPluginEmbedder::HandleInputEvent(
    int instance_id,
    RenderViewHost* render_view_host,
    const gfx::Rect& guest_window_rect,
    const WebKit::WebInputEvent& event) {
  // Convert the window coordinates into screen coordinates.
  gfx::Rect guest_screen_rect(guest_window_rect);
  guest_screen_rect.Offset(
      static_cast<RenderViewHostImpl*>(render_view_host)->GetView()->
          GetViewBounds().OffsetFromOrigin());
  BrowserPluginGuest* guest = GetGuestByInstanceID(instance_id);
  if (guest) {
    guest->HandleInputEvent(render_view_host,
                            guest_window_rect,
                            guest_screen_rect,
                            event);
  }
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
}

void BrowserPluginEmbedder::RenderViewGone(base::TerminationStatus status) {
  CleanUp();
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

void BrowserPluginEmbedder::SetAutoSize(
    int instance_id,
    const BrowserPluginHostMsg_AutoSize_Params& auto_size_params,
    const BrowserPluginHostMsg_ResizeGuest_Params& resize_guest_params) {
  BrowserPluginGuest* guest = GetGuestByInstanceID(instance_id);
  if (guest)
    guest->SetSize(auto_size_params, resize_guest_params);
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

void BrowserPluginEmbedder::GetRenderViewHostAtPosition(
    int x,
    int y,
    const WebContents::GetRenderViewHostCallback& callback) {
  // Store the callback so we can call it later when we have the response.
  pending_get_render_view_callbacks_.insert(
      std::make_pair(next_get_render_view_request_id_, callback));
  render_view_host_->Send(
      new BrowserPluginMsg_PluginAtPositionRequest(
          render_view_host_->GetRoutingID(),
          next_get_render_view_request_id_,
          gfx::Point(x, y)));
  ++next_get_render_view_request_id_;
}

void BrowserPluginEmbedder::PluginAtPositionResponse(
    int instance_id, int request_id, const gfx::Point& position) {
  const std::map<int, WebContents::GetRenderViewHostCallback>::iterator
      callback_iter = pending_get_render_view_callbacks_.find(request_id);
  if (callback_iter == pending_get_render_view_callbacks_.end())
    return;

  RenderViewHost* render_view_host;
  BrowserPluginGuest* guest = GetGuestByInstanceID(instance_id);
  if (guest)
    render_view_host = guest->GetWebContents()->GetRenderViewHost();
  else  // No plugin, use embedder's RenderViewHost.
    render_view_host = render_view_host_;

  callback_iter->second.Run(render_view_host, position.x(), position.y());
  pending_get_render_view_callbacks_.erase(callback_iter);
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
