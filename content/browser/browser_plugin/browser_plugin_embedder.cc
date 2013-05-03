// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browser_plugin/browser_plugin_embedder.h"

#include "content/browser/browser_plugin/browser_plugin_guest.h"
#include "content/browser/browser_plugin/browser_plugin_guest_manager.h"
#include "content/browser/browser_plugin/browser_plugin_host_factory.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/browser_plugin/browser_plugin_constants.h"
#include "content/common/browser_plugin/browser_plugin_messages.h"
#include "content/common/drag_messages.h"
#include "content/common/gpu/gpu_messages.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/result_codes.h"
#include "content/public/common/url_constants.h"
#include "net/base/escape.h"

namespace content {

// static
BrowserPluginHostFactory* BrowserPluginEmbedder::factory_ = NULL;

BrowserPluginEmbedder::BrowserPluginEmbedder(WebContentsImpl* web_contents)
    : WebContentsObserver(web_contents),
      next_get_render_view_request_id_(0) {
}

BrowserPluginEmbedder::~BrowserPluginEmbedder() {
  CleanUp();
}

// static
BrowserPluginEmbedder* BrowserPluginEmbedder::Create(
    WebContentsImpl* web_contents) {
  if (factory_)
    return factory_->CreateBrowserPluginEmbedder(web_contents);
  return new BrowserPluginEmbedder(web_contents);
}

void BrowserPluginEmbedder::DragEnteredGuest(BrowserPluginGuest* guest) {
  guest_dragging_over_ = guest->AsWeakPtr();
}

void BrowserPluginEmbedder::DragLeftGuest(BrowserPluginGuest* guest) {
  // Avoid race conditions in switching between guests being hovered over by
  // only un-setting if the caller is marked as the guest being dragged over.
  if (guest_dragging_over_ == guest) {
    guest_dragging_over_.reset();
  }
}

void BrowserPluginEmbedder::StartDrag(BrowserPluginGuest* guest) {
  guest_started_drag_ = guest->AsWeakPtr();
}

void BrowserPluginEmbedder::StopDrag(BrowserPluginGuest* guest) {
  if (guest_started_drag_ == guest) {
    guest_started_drag_.reset();
  }
}

void BrowserPluginEmbedder::GetRenderViewHostAtPosition(
    int x, int y, const WebContents::GetRenderViewHostCallback& callback) {
  // Store the callback so we can call it later when we have the response.
  pending_get_render_view_callbacks_.insert(
      std::make_pair(next_get_render_view_request_id_, callback));
  Send(new BrowserPluginMsg_PluginAtPositionRequest(
      routing_id(),
      next_get_render_view_request_id_,
      gfx::Point(x, y)));
  ++next_get_render_view_request_id_;
}

void BrowserPluginEmbedder::RenderViewGone(base::TerminationStatus status) {
  CleanUp();
}

bool BrowserPluginEmbedder::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(BrowserPluginEmbedder, message)
    IPC_MESSAGE_HANDLER(BrowserPluginHostMsg_AllocateInstanceID,
                        OnAllocateInstanceID)
    IPC_MESSAGE_HANDLER(BrowserPluginHostMsg_Attach, OnAttach)
    IPC_MESSAGE_HANDLER(BrowserPluginHostMsg_PluginAtPositionResponse,
                        OnPluginAtPositionResponse)
    IPC_MESSAGE_HANDLER_GENERIC(DragHostMsg_UpdateDragCursor,
                                OnUpdateDragCursor(&handled));
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void BrowserPluginEmbedder::DragSourceEndedAt(int client_x, int client_y,
    int screen_x, int screen_y, WebKit::WebDragOperation operation) {
  if (guest_started_drag_) {
    gfx::Point guest_offset =
        guest_started_drag_->GetScreenCoordinates(gfx::Point());
    guest_started_drag_->DragSourceEndedAt(client_x - guest_offset.x(),
        client_y - guest_offset.y(), screen_x, screen_y, operation);
  }
}

void BrowserPluginEmbedder::DragSourceMovedTo(int client_x, int client_y,
                                              int screen_x, int screen_y) {
  if (guest_started_drag_) {
    gfx::Point guest_offset =
        guest_started_drag_->GetScreenCoordinates(gfx::Point());
    guest_started_drag_->DragSourceMovedTo(client_x - guest_offset.x(),
        client_y - guest_offset.y(), screen_x, screen_y);
  }
}

void BrowserPluginEmbedder::SystemDragEnded() {
  if (guest_started_drag_ && (guest_started_drag_ != guest_dragging_over_))
    guest_started_drag_->EndSystemDrag();
  guest_started_drag_.reset();
  guest_dragging_over_.reset();
}

void BrowserPluginEmbedder::OnUpdateDragCursor(bool* handled) {
  *handled = (guest_dragging_over_ != NULL);
}

void BrowserPluginEmbedder::CleanUp() {
  // CleanUp gets called when BrowserPluginEmbedder's WebContents goes away
  // or the associated RenderViewHost is destroyed or swapped out. Therefore we
  // don't need to care about the pending callbacks anymore.
  pending_get_render_view_callbacks_.clear();
}

BrowserPluginGuestManager*
    BrowserPluginEmbedder::GetBrowserPluginGuestManager() {
  BrowserPluginGuestManager* guest_manager = static_cast<WebContentsImpl*>(
      web_contents())->GetBrowserPluginGuestManager();
  if (!guest_manager) {
    guest_manager = BrowserPluginGuestManager::Create();
    web_contents()->GetBrowserContext()->SetUserData(
        browser_plugin::kBrowserPluginGuestManagerKeyName, guest_manager);
  }
  return guest_manager;
}

void BrowserPluginEmbedder::OnAllocateInstanceID(int request_id) {
  int instance_id = GetBrowserPluginGuestManager()->get_next_instance_id();
  Send(new BrowserPluginMsg_AllocateInstanceID_ACK(
      routing_id(), request_id, instance_id));
}

void BrowserPluginEmbedder::OnAttach(
    int instance_id,
    const BrowserPluginHostMsg_Attach_Params& params) {
  if (!GetBrowserPluginGuestManager()->CanEmbedderAccessInstanceIDMaybeKill(
          web_contents()->GetRenderProcessHost()->GetID(), instance_id))
    return;

  BrowserPluginGuest* guest =
      GetBrowserPluginGuestManager()->GetGuestByInstanceID(
          instance_id, web_contents()->GetRenderProcessHost()->GetID());

  if (guest) {
    guest->Attach(static_cast<WebContentsImpl*>(web_contents()), params);
    return;
  }

  guest = GetBrowserPluginGuestManager()->CreateGuest(
      web_contents()->GetSiteInstance(), instance_id, params);
  if (guest)
    guest->Initialize(static_cast<WebContentsImpl*>(web_contents()), params);
}

void BrowserPluginEmbedder::OnPluginAtPositionResponse(
    int instance_id, int request_id, const gfx::Point& position) {
  const std::map<int, WebContents::GetRenderViewHostCallback>::iterator
      callback_iter = pending_get_render_view_callbacks_.find(request_id);
  if (callback_iter == pending_get_render_view_callbacks_.end())
    return;

  RenderViewHost* render_view_host;
  BrowserPluginGuest* guest = NULL;
  if (instance_id != browser_plugin::kInstanceIDNone) {
    guest = GetBrowserPluginGuestManager()->GetGuestByInstanceID(
                instance_id, web_contents()->GetRenderProcessHost()->GetID());
  }

  if (guest)
    render_view_host = guest->GetWebContents()->GetRenderViewHost();
  else  // No plugin, use embedder's RenderViewHost.
    render_view_host = web_contents()->GetRenderViewHost();

  callback_iter->second.Run(render_view_host, position.x(), position.y());
  pending_get_render_view_callbacks_.erase(callback_iter);
}

}  // namespace content
