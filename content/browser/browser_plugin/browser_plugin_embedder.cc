// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browser_plugin/browser_plugin_embedder.h"

#include "base/values.h"
#include "content/browser/browser_plugin/browser_plugin_guest.h"
#include "content/browser/browser_plugin/browser_plugin_guest_manager.h"
#include "content/browser/browser_plugin/browser_plugin_host_factory.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/browser_plugin/browser_plugin_constants.h"
#include "content/common/browser_plugin/browser_plugin_messages.h"
#include "content/common/drag_messages.h"
#include "content/common/gpu/gpu_messages.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/result_codes.h"
#include "content/public/common/url_constants.h"
#include "net/base/escape.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace content {

// static
BrowserPluginHostFactory* BrowserPluginEmbedder::factory_ = NULL;

BrowserPluginEmbedder::BrowserPluginEmbedder(WebContentsImpl* web_contents)
    : WebContentsObserver(web_contents) {
}

BrowserPluginEmbedder::~BrowserPluginEmbedder() {
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
  if (guest_dragging_over_.get() == guest) {
    guest_dragging_over_.reset();
  }
}

void BrowserPluginEmbedder::StartDrag(BrowserPluginGuest* guest) {
  guest_started_drag_ = guest->AsWeakPtr();
}

WebContentsImpl* BrowserPluginEmbedder::GetWebContents() {
  return static_cast<WebContentsImpl*>(web_contents());
}

bool BrowserPluginEmbedder::DidSendScreenRectsCallback(
   BrowserPluginGuest* guest) {
  static_cast<RenderViewHostImpl*>(
      guest->GetWebContents()->GetRenderViewHost())->SendScreenRects();
  // Not handled => Iterate over all guests.
  return false;
}

void BrowserPluginEmbedder::DidSendScreenRects() {
  GetBrowserPluginGuestManager()->ForEachGuest(GetWebContents(), base::Bind(
      &BrowserPluginEmbedder::DidSendScreenRectsCallback,
      base::Unretained(this)));
}

bool BrowserPluginEmbedder::UnlockMouseIfNecessaryCallback(
    const NativeWebKeyboardEvent& event,
    BrowserPluginGuest* guest) {
  return guest->UnlockMouseIfNecessary(event);
}

bool BrowserPluginEmbedder::HandleKeyboardEvent(
    const NativeWebKeyboardEvent& event) {
  if ((event.type != blink::WebInputEvent::RawKeyDown) ||
      (event.windowsKeyCode != ui::VKEY_ESCAPE) ||
      (event.modifiers & blink::WebInputEvent::InputModifiers)) {
    return false;
  }

  return GetBrowserPluginGuestManager()->ForEachGuest(GetWebContents(),
      base::Bind(&BrowserPluginEmbedder::UnlockMouseIfNecessaryCallback,
                 base::Unretained(this),
                 event));
}

bool BrowserPluginEmbedder::SetZoomLevelCallback(
    double level, BrowserPluginGuest* guest) {
  double zoom_factor = content::ZoomLevelToZoomFactor(level);
  guest->SetZoom(zoom_factor);
  // Not handled => Iterate over all guests.
  return false;
}

void BrowserPluginEmbedder::SetZoomLevel(double level) {
  GetBrowserPluginGuestManager()->ForEachGuest(GetWebContents(), base::Bind(
      &BrowserPluginEmbedder::SetZoomLevelCallback,
      base::Unretained(this),
      level));
}

bool BrowserPluginEmbedder::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(BrowserPluginEmbedder, message)
    IPC_MESSAGE_HANDLER(BrowserPluginHostMsg_AllocateInstanceID,
                        OnAllocateInstanceID)
    IPC_MESSAGE_HANDLER(BrowserPluginHostMsg_Attach, OnAttach)
    IPC_MESSAGE_HANDLER_GENERIC(DragHostMsg_UpdateDragCursor,
                                OnUpdateDragCursor(&handled));
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void BrowserPluginEmbedder::DragSourceEndedAt(int client_x, int client_y,
    int screen_x, int screen_y, blink::WebDragOperation operation) {
  if (guest_started_drag_.get()) {
    gfx::Point guest_offset =
        guest_started_drag_->GetScreenCoordinates(gfx::Point());
    guest_started_drag_->DragSourceEndedAt(client_x - guest_offset.x(),
        client_y - guest_offset.y(), screen_x, screen_y, operation);
  }
}

void BrowserPluginEmbedder::SystemDragEnded() {
  // When the embedder's drag/drop operation ends, we need to pass the message
  // to the guest that initiated the drag/drop operation. This will ensure that
  // the guest's RVH state is reset properly.
  if (guest_started_drag_.get())
    guest_started_drag_->EndSystemDrag();
  guest_started_drag_.reset();
  guest_dragging_over_.reset();
}

void BrowserPluginEmbedder::OnUpdateDragCursor(bool* handled) {
  *handled = (guest_dragging_over_.get() != NULL);
}

BrowserPluginGuestManager*
    BrowserPluginEmbedder::GetBrowserPluginGuestManager() {
  BrowserPluginGuestManager* guest_manager =
      GetWebContents()->GetBrowserPluginGuestManager();
  if (!guest_manager) {
    guest_manager = BrowserPluginGuestManager::Create();
    GetWebContents()->GetBrowserContext()->SetUserData(
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
    const BrowserPluginHostMsg_Attach_Params& params,
    const base::DictionaryValue& extra_params) {
  if (!GetBrowserPluginGuestManager()->CanEmbedderAccessInstanceIDMaybeKill(
          GetWebContents()->GetRenderProcessHost()->GetID(), instance_id))
    return;

  BrowserPluginGuest* guest =
      GetBrowserPluginGuestManager()->GetGuestByInstanceID(
          instance_id, GetWebContents()->GetRenderProcessHost()->GetID());

  if (guest) {
    // There is an implicit order expectation here:
    // 1. The content embedder is made aware of the attachment.
    // 2. BrowserPluginGuest::Attach is called.
    // 3. The content embedder issues queued events if any that happened
    //    prior to attachment.
    GetContentClient()->browser()->GuestWebContentsAttached(
        guest->GetWebContents(),
        GetWebContents(),
        extra_params);
    guest->Attach(GetWebContents(), params, extra_params);
    return;
  }

  scoped_ptr<base::DictionaryValue> copy_extra_params(extra_params.DeepCopy());
  guest = GetBrowserPluginGuestManager()->CreateGuest(
      GetWebContents()->GetSiteInstance(),
      instance_id, params,
      copy_extra_params.Pass());
  if (guest) {
    GetContentClient()->browser()->GuestWebContentsAttached(
        guest->GetWebContents(),
        GetWebContents(),
        extra_params);
    guest->Initialize(params, GetWebContents());
  }
}

}  // namespace content
