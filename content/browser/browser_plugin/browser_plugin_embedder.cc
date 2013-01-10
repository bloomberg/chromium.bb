// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browser_plugin/browser_plugin_embedder.h"

#include "base/stl_util.h"
#include "content/browser/browser_plugin/browser_plugin_guest.h"
#include "content/browser/browser_plugin/browser_plugin_host_factory.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/browser_plugin_messages.h"
#include "content/common/gpu/gpu_messages.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/common/result_codes.h"
#include "content/public/common/url_constants.h"
#include "net/base/escape.h"

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

void BrowserPluginEmbedder::CreateGuest(
    int instance_id,
    int routing_id,
    BrowserPluginGuest* guest_opener,
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
    base::KillProcess(render_view_host_->GetProcess()->GetHandle(),
                      content::RESULT_CODE_KILLED_BAD_MESSAGE, false);
    return;
  }

  const std::string& host =
      render_view_host_->GetSiteInstance()->GetSiteURL().host();
  std::string url_encoded_partition = net::EscapeQueryParamValue(
      params.storage_partition_id, false);

  SiteInstance* guest_site_instance = NULL;
  if (guest_opener) {
    guest_site_instance = guest_opener->GetWebContents()->GetSiteInstance();
  } else {
    // The SiteInstance of a given webview tag is based on the fact that it's a
    // guest process in addition to which platform application the tag belongs
    // to and what storage partition is in use, rather than the URL that the tag
    // is being navigated to.
    GURL guest_site(
        base::StringPrintf("%s://%s/%s?%s", chrome::kGuestScheme,
                          host.c_str(), params.persist_storage ? "persist" : "",
                          url_encoded_partition.c_str()));

    // If we already have a webview tag in the same app using the same storage
    // partition, we should use the same SiteInstance so the existing tag and
    // the new tag can script each other.
    for (ContainerInstanceMap::const_iterator it =
            guest_web_contents_by_instance_id_.begin();
        it != guest_web_contents_by_instance_id_.end(); ++it) {
      if (it->second->GetSiteInstance()->GetSiteURL() == guest_site) {
        guest_site_instance = it->second->GetSiteInstance();
        break;
      }
    }
    if (!guest_site_instance) {
      // Create the SiteInstance in a new BrowsingInstance, which will ensure
      // that webview tags are also not allowed to send messages across
      // different partitions.
      guest_site_instance = SiteInstance::CreateForURL(
          web_contents()->GetBrowserContext(), guest_site);
    }
  }
  WebContentsImpl* opener_web_contents = static_cast<WebContentsImpl*>(
      guest_opener ? guest_opener->GetWebContents() : NULL);
  guest_web_contents = WebContentsImpl::CreateGuest(
      web_contents()->GetBrowserContext(),
      guest_site_instance,
      routing_id,
      opener_web_contents,
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
  // We would like the guest to report changes to frame names so that we can
  // update the BrowserPlugin's corresponding 'name' attribute.
  // TODO(fsamuel): Remove this once http://crbug.com/169110 is addressed.
  guest_renderer_prefs->report_frame_name_changes = true;
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
  render_view_host_->Send(new BrowserPluginMsg_GuestContentWindowReady(
      render_view_host_->GetRoutingID(), instance_id, guest_routing_id));

  guest->Initialize(params, guest_web_contents->GetRenderViewHost());
}

BrowserPluginGuest* BrowserPluginEmbedder::GetGuestByInstanceID(
    int instance_id) const {
  ContainerInstanceMap::const_iterator it =
      guest_web_contents_by_instance_id_.find(instance_id);
  if (it != guest_web_contents_by_instance_id_.end())
    return static_cast<WebContentsImpl*>(it->second)->GetBrowserPluginGuest();
  return NULL;
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

void BrowserPluginEmbedder::GetRenderViewHostAtPosition(
    int x, int y, const WebContents::GetRenderViewHostCallback& callback) {
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

void BrowserPluginEmbedder::RenderViewDeleted(
    RenderViewHost* render_view_host) {
}

void BrowserPluginEmbedder::RenderViewGone(base::TerminationStatus status) {
  CleanUp();
}

bool BrowserPluginEmbedder::OnMessageReceived(const IPC::Message& message) {
  if (ShouldForwardToBrowserPluginGuest(message)) {
    int instance_id = 0;
    // All allowed messages must have instance_id as their first parameter.
    PickleIterator iter(message);
    bool success = iter.ReadInt(&instance_id);
    DCHECK(success);
    BrowserPluginGuest* guest = GetGuestByInstanceID(instance_id);
    if (guest && guest->OnMessageReceivedFromEmbedder(message))
      return true;
  }
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(BrowserPluginEmbedder, message)
    IPC_MESSAGE_HANDLER(BrowserPluginHostMsg_CreateGuest,
                        OnCreateGuest)
    IPC_MESSAGE_HANDLER(BrowserPluginHostMsg_NavigateGuest,
                        OnNavigateGuest)
    IPC_MESSAGE_HANDLER(BrowserPluginHostMsg_PluginAtPositionResponse,
                        OnPluginAtPositionResponse)
    IPC_MESSAGE_HANDLER(BrowserPluginHostMsg_PluginDestroyed,
                        OnPluginDestroyed)
    IPC_MESSAGE_HANDLER(BrowserPluginHostMsg_BuffersSwappedACK,
                        OnSwapBuffersACK)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
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

void BrowserPluginEmbedder::AddGuest(int instance_id,
                                     WebContents* guest_web_contents) {
  DCHECK(guest_web_contents_by_instance_id_.find(instance_id) ==
         guest_web_contents_by_instance_id_.end());
  guest_web_contents_by_instance_id_[instance_id] = guest_web_contents;
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

void BrowserPluginEmbedder::WebContentsVisibilityChanged(bool visible) {
  visible_ = visible;
  // If the embedder is hidden we need to hide the guests as well.
  for (ContainerInstanceMap::const_iterator it =
           guest_web_contents_by_instance_id_.begin();
       it != guest_web_contents_by_instance_id_.end(); ++it) {
    WebContentsImpl* web_contents = static_cast<WebContentsImpl*>(it->second);
    BrowserPluginGuest* guest = web_contents->GetBrowserPluginGuest();
    guest->UpdateVisibility();
  }
}

// static
bool BrowserPluginEmbedder::ShouldForwardToBrowserPluginGuest(
    const IPC::Message& message) {
  switch (message.type()) {
    case BrowserPluginHostMsg_DragStatusUpdate::ID:
    case BrowserPluginHostMsg_Go::ID:
    case BrowserPluginHostMsg_HandleInputEvent::ID:
    case BrowserPluginHostMsg_Reload::ID:
    case BrowserPluginHostMsg_ResizeGuest::ID:
    case BrowserPluginHostMsg_SetAutoSize::ID:
    case BrowserPluginHostMsg_SetFocus::ID:
    case BrowserPluginHostMsg_SetName::ID:
    case BrowserPluginHostMsg_SetVisibility::ID:
    case BrowserPluginHostMsg_Stop::ID:
    case BrowserPluginHostMsg_TerminateGuest::ID:
    case BrowserPluginHostMsg_UpdateRect_ACK::ID:
      return true;
    default:
      break;
  }
  return false;
}

void BrowserPluginEmbedder::OnCreateGuest(
    int instance_id,
    const BrowserPluginHostMsg_CreateGuest_Params& params) {
  CreateGuest(instance_id, MSG_ROUTING_NONE, NULL, params);
}

void BrowserPluginEmbedder::OnNavigateGuest(
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

void BrowserPluginEmbedder::OnPluginAtPositionResponse(
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

void BrowserPluginEmbedder::OnPluginDestroyed(int instance_id) {
  DestroyGuestByInstanceID(instance_id);
}

void BrowserPluginEmbedder::OnSwapBuffersACK(int route_id,
                                             int gpu_host_id,
                                             const std::string& mailbox_name,
                                             uint32 sync_point) {
  AcceleratedSurfaceMsg_BufferPresented_Params ack_params;
  ack_params.mailbox_name = mailbox_name;
  ack_params.sync_point = sync_point;
  RenderWidgetHostImpl::AcknowledgeBufferPresent(route_id,
                                                 gpu_host_id,
                                                 ack_params);
}

}  // namespace content
