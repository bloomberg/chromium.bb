// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browser_plugin/browser_plugin_embedder.h"

#include "base/command_line.h"
#include "base/stl_util.h"
#include "content/browser/browser_plugin/browser_plugin_guest.h"
#include "content/browser/browser_plugin/browser_plugin_host_factory.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/browser_plugin/browser_plugin_messages.h"
#include "content/common/gpu/gpu_messages.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/common/content_switches.h"
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
      next_get_render_view_request_id_(0),
      next_instance_id_(0) {
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
  SiteInstance* guest_site_instance = NULL;
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

  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kSitePerProcess)) {
    // When --site-per-process is specified, the behavior of BrowserPlugin
    // as <webview> is broken and we use it for rendering out-of-process
    // iframes instead. We use the src URL sent by the renderer to find the
    // right process in which to place this instance.
    // Note: Since BrowserPlugin doesn't support cross-process navigation,
    // the instance will stay in the initially assigned process, regardless
    // of the site it is navigated to.
    // TODO(nasko): Fix this, and such that cross-process navigations are
    // supported.
    guest_site_instance =
        web_contents()->GetSiteInstance()->GetRelatedSiteInstance(
            GURL(params.src));
  } else {
    const std::string& host =
        render_view_host_->GetSiteInstance()->GetSiteURL().host();
    std::string url_encoded_partition = net::EscapeQueryParamValue(
        params.storage_partition_id, false);

    if (guest_opener) {
      guest_site_instance = guest_opener->GetWebContents()->GetSiteInstance();
    } else {
      // The SiteInstance of a given webview tag is based on the fact that it's
      // a guest process in addition to which platform application the tag
      // belongs to and what storage partition is in use, rather than the URL
      // that the tag is being navigated to.
      GURL guest_site(
          base::StringPrintf("%s://%s/%s?%s", chrome::kGuestScheme,
                             host.c_str(),
                             params.persist_storage ? "persist" : "",
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
  }

  WebContentsImpl* opener_web_contents = static_cast<WebContentsImpl*>(
      guest_opener ? guest_opener->GetWebContents() : NULL);
  WebContentsImpl::CreateGuest(
      web_contents()->GetBrowserContext(),
      guest_site_instance,
      routing_id,
      static_cast<WebContentsImpl*>(web_contents()),
      opener_web_contents,
      instance_id,
      params);
}

BrowserPluginGuest* BrowserPluginEmbedder::GetGuestByInstanceID(
    int instance_id) const {
  ContainerInstanceMap::const_iterator it =
      guest_web_contents_by_instance_id_.find(instance_id);
  if (it != guest_web_contents_by_instance_id_.end())
    return static_cast<WebContentsImpl*>(it->second)->GetBrowserPluginGuest();
  return NULL;
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
    IPC_MESSAGE_HANDLER(BrowserPluginHostMsg_AllocateInstanceID,
                        OnAllocateInstanceID)
    IPC_MESSAGE_HANDLER(BrowserPluginHostMsg_CreateGuest,
                        OnCreateGuest)
    IPC_MESSAGE_HANDLER(BrowserPluginHostMsg_PluginAtPositionResponse,
                        OnPluginAtPositionResponse)
    IPC_MESSAGE_HANDLER(BrowserPluginHostMsg_BuffersSwappedACK,
                        OnUnhandledSwapBuffersACK)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void BrowserPluginEmbedder::AddGuest(int instance_id,
                                     WebContents* guest_web_contents) {
  DCHECK(guest_web_contents_by_instance_id_.find(instance_id) ==
         guest_web_contents_by_instance_id_.end());
  guest_web_contents_by_instance_id_[instance_id] = guest_web_contents;
}

void BrowserPluginEmbedder::RemoveGuest(int instance_id) {
  DCHECK(guest_web_contents_by_instance_id_.find(instance_id) !=
         guest_web_contents_by_instance_id_.end());
  guest_web_contents_by_instance_id_.erase(instance_id);
}

void BrowserPluginEmbedder::CleanUp() {
  // CleanUp gets called when BrowserPluginEmbedder's WebContents goes away
  // or the associated RenderViewHost is destroyed or swapped out. Therefore we
  // don't need to care about the pending callbacks anymore.
  pending_get_render_view_callbacks_.clear();
}

// static
bool BrowserPluginEmbedder::ShouldForwardToBrowserPluginGuest(
    const IPC::Message& message) {
  switch (message.type()) {
    case BrowserPluginHostMsg_BuffersSwappedACK::ID:
    case BrowserPluginHostMsg_DragStatusUpdate::ID:
    case BrowserPluginHostMsg_Go::ID:
    case BrowserPluginHostMsg_HandleInputEvent::ID:
    case BrowserPluginHostMsg_NavigateGuest::ID:
    case BrowserPluginHostMsg_PluginDestroyed::ID:
    case BrowserPluginHostMsg_Reload::ID:
    case BrowserPluginHostMsg_ResizeGuest::ID:
    case BrowserPluginHostMsg_RespondPermission::ID:
    case BrowserPluginHostMsg_SetAutoSize::ID:
    case BrowserPluginHostMsg_SetFocus::ID:
    case BrowserPluginHostMsg_SetName::ID:
    case BrowserPluginHostMsg_SetVisibility::ID:
    case BrowserPluginHostMsg_Stop::ID:
    case BrowserPluginHostMsg_TerminateGuest::ID:
    case BrowserPluginHostMsg_UpdateRect_ACK::ID:
    case BrowserPluginHostMsg_LockMouse_ACK::ID:
    case BrowserPluginHostMsg_UnlockMouse_ACK::ID:
      return true;
    default:
      break;
  }
  return false;
}

void BrowserPluginEmbedder::OnAllocateInstanceID(int request_id) {
  int instance_id = ++next_instance_id_;
  render_view_host_->Send(new BrowserPluginMsg_AllocateInstanceID_ACK(
      render_view_host_->GetRoutingID(), request_id, instance_id));
}

void BrowserPluginEmbedder::OnCreateGuest(
    int instance_id,
    const BrowserPluginHostMsg_CreateGuest_Params& params) {
  CreateGuest(instance_id, MSG_ROUTING_NONE, NULL, params);
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

// We only get here during teardown if we have one last buffer pending,
// otherwise the ACK is handled by the guest.
void BrowserPluginEmbedder::OnUnhandledSwapBuffersACK(
    int instance_id,
    int route_id,
    int gpu_host_id,
    const std::string& mailbox_name,
    uint32 sync_point) {
  BrowserPluginGuest::AcknowledgeBufferPresent(route_id,
                                               gpu_host_id,
                                               mailbox_name,
                                               sync_point);
}

}  // namespace content
