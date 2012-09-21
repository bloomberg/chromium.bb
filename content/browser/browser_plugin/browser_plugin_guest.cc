// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browser_plugin/browser_plugin_guest.h"

#include <algorithm>

#include "content/browser/browser_plugin/browser_plugin_guest_helper.h"
#include "content/browser/browser_plugin/browser_plugin_host_factory.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/browser_plugin_messages.h"
#include "content/common/view_messages.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/common/result_codes.h"
#include "content/browser/browser_plugin/browser_plugin_host_factory.h"
#include "ui/surface/transport_dib.h"

namespace content {

// static
BrowserPluginHostFactory* BrowserPluginGuest::factory_ = NULL;

namespace {
const int kGuestHangTimeoutMs = 5000;
}

BrowserPluginGuest::BrowserPluginGuest(int instance_id,
                                       WebContentsImpl* web_contents,
                                       RenderViewHost* render_view_host)
    : WebContentsObserver(web_contents),
      embedder_render_process_host_(NULL),
      instance_id_(instance_id),
#if defined(OS_WIN)
      damage_buffer_size_(0),
#endif
      pending_update_counter_(0),
      guest_hang_timeout_(
          base::TimeDelta::FromMilliseconds(kGuestHangTimeoutMs)) {
  DCHECK(web_contents);
  // |render_view_host| manages the ownership of this BrowserPluginGuestHelper.
  new BrowserPluginGuestHelper(this, render_view_host);
}

BrowserPluginGuest::~BrowserPluginGuest() {
}

// static
BrowserPluginGuest* BrowserPluginGuest::Create(
    int instance_id,
    WebContentsImpl* web_contents,
    content::RenderViewHost* render_view_host) {
  if (factory_) {
    return factory_->CreateBrowserPluginGuest(instance_id,
                                              web_contents,
                                              render_view_host);
  }
  return new BrowserPluginGuest(instance_id, web_contents, render_view_host);
}

bool BrowserPluginGuest::ViewTakeFocus(bool reverse) {
  SendMessageToEmbedder(
      new BrowserPluginMsg_AdvanceFocus(instance_id(), reverse));
  return true;
}

void BrowserPluginGuest::RendererUnresponsive(WebContents* source) {
  base::ProcessHandle process_handle =
      web_contents()->GetRenderProcessHost()->GetHandle();
  base::KillProcess(process_handle, RESULT_CODE_HUNG, false);
}

WebContents* BrowserPluginGuest::GetWebContents() {
  return web_contents();
}

void BrowserPluginGuest::SetDamageBuffer(
    TransportDIB* damage_buffer,
#if defined(OS_WIN)
    int damage_buffer_size,
#endif
    const gfx::Size& damage_view_size,
    float scale_factor) {
  // Sanity check: Verify that we've correctly shared the damage buffer memory
  // between the embedder and browser processes.
  DCHECK(*static_cast<unsigned int*>(damage_buffer->memory()) == 0xdeadbeef);
  damage_buffer_.reset(damage_buffer);
#if defined(OS_WIN)
  damage_buffer_size_ = damage_buffer_size;
#endif
  damage_view_size_ = damage_view_size;
  damage_buffer_scale_factor_ = scale_factor;
}

void BrowserPluginGuest::UpdateRect(
    RenderViewHost* render_view_host,
    const ViewHostMsg_UpdateRect_Params& params) {
  RenderWidgetHostImpl* render_widget_host =
      RenderWidgetHostImpl::From(render_view_host);
  render_widget_host->ResetSizeAndRepaintPendingFlags();
  // This handler is only of interest to us for the 2D software rendering path.
  // needs_ack should always be true for the 2D path.
  // TODO(fsamuel): Do we need to do something different in the 3D case?
  if (!params.needs_ack)
    return;

  // Only copy damage if the guest's view size is equal to the damage buffer's
  // size and the guest's scale factor is equal to the damage buffer's scale
  // factor.
  // The scaling change can happen due to asynchronous updates of the DPI on a
  // resolution change.
  if (params.view_size.width() == damage_view_size().width() &&
      params.view_size.height() == damage_view_size().height() &&
      params.scale_factor == damage_buffer_scale_factor()) {
    TransportDIB* dib = render_view_host->GetProcess()->
        GetTransportDIB(params.bitmap);
    if (dib) {
#if defined(OS_WIN)
      size_t guest_damage_buffer_size = params.bitmap_rect.width() *
                                        params.bitmap_rect.height() * 4;
      size_t embedder_damage_buffer_size = damage_buffer_size_;
#else
      size_t guest_damage_buffer_size = dib->size();
      size_t embedder_damage_buffer_size = damage_buffer_->size();
#endif
      void* guest_memory = dib->memory();
      void* embedder_memory = damage_buffer_->memory();
      size_t size = std::min(guest_damage_buffer_size,
                             embedder_damage_buffer_size);
      memcpy(embedder_memory, guest_memory, size);
    }
  }
  BrowserPluginMsg_UpdateRect_Params relay_params;
  relay_params.bitmap_rect = params.bitmap_rect;
  relay_params.dx = params.dx;
  relay_params.dy = params.dy;
  relay_params.scroll_rect = params.scroll_rect;
  relay_params.copy_rects = params.copy_rects;
  relay_params.view_size = params.view_size;
  relay_params.scale_factor = params.scale_factor;
  relay_params.is_resize_ack = ViewHostMsg_UpdateRect_Flags::is_resize_ack(
      params.flags);

  // We need to send the ACK to the same render_view_host that issued
  // the UpdateRect. We keep track of this correspondence via a message_id.
  int message_id = pending_update_counter_++;
  pending_updates_.AddWithID(render_view_host, message_id);

  gfx::Size param_size = gfx::Size(params.view_size.width(),
                                   params.view_size.height());

  SendMessageToEmbedder(new BrowserPluginMsg_UpdateRect(instance_id(),
                                                        message_id,
                                                        relay_params));
}

void BrowserPluginGuest::UpdateRectACK(int message_id, const gfx::Size& size) {
  RenderViewHost* render_view_host = pending_updates_.Lookup(message_id);
  // If the guest has crashed since it sent the initial ViewHostMsg_UpdateRect
  // then the pending_updates_ map will have been cleared.
  if (!render_view_host)
    return;
  pending_updates_.Remove(message_id);
  render_view_host->Send(
      new ViewMsg_UpdateRect_ACK(render_view_host->GetRoutingID()));
  if (!size.IsEmpty())
    render_view_host->GetView()->SetSize(size);
}

void BrowserPluginGuest::HandleInputEvent(RenderViewHost* render_view_host,
                                          const gfx::Rect& guest_rect,
                                          const WebKit::WebInputEvent& event,
                                          IPC::Message* reply_message) {
  DCHECK(!pending_input_event_reply_.get());
  guest_rect_ = guest_rect;
  RenderViewHostImpl* guest_rvh = static_cast<RenderViewHostImpl*>(
      web_contents()->GetRenderViewHost());
  IPC::Message* message = new ViewMsg_HandleInputEvent(
      guest_rvh->GetRoutingID());

  // Copy the WebInputEvent and modify the event type. The guest expects
  // WebInputEvent::RawKeyDowns and not KeyDowns.
  scoped_array<char> input_buffer(new char[event.size]);
  memcpy(input_buffer.get(), &event, event.size);
  WebKit::WebInputEvent* input_event =
      reinterpret_cast<WebKit::WebInputEvent*>(input_buffer.get());
  if (event.type == WebKit::WebInputEvent::KeyDown)
    input_event->type = WebKit::WebInputEvent::RawKeyDown;

  message->WriteData(input_buffer.get(), event.size);
  // TODO(fsamuel): What do we need to do here? This is for keyboard shortcuts.
  if (input_event->type == WebKit::WebInputEvent::RawKeyDown)
    message->WriteBool(false);
  bool sent = guest_rvh->Send(message);
  if (!sent) {
    // If the embedder is waiting for a previous input ack, a new input message
    // won't get sent to the guest. Reply immediately with handled = false so
    // embedder doesn't hang.
    BrowserPluginHostMsg_HandleInputEvent::WriteReplyParams(
        reply_message, false /* handled */, cursor_);
    SendMessageToEmbedder(reply_message);
    return;
  }

  pending_input_event_reply_.reset(reply_message);
  // Input events are handled synchronously, meaning it blocks the embedder. We
  // set a hang monitor here that will kill the guest process (5s timeout) if we
  // don't receive an ack. This will kill all the guests that are running in the
  // same process (undesired behavior).
  // TODO(fsamuel,lazyboy): Find a way to get rid of guest process kill
  // behavior. http://crbug.com/147272.
  guest_rvh->StartHangMonitorTimeout(guest_hang_timeout_);
}

void BrowserPluginGuest::HandleInputEventAck(RenderViewHost* render_view_host,
                                             bool handled) {
  RenderViewHostImpl* guest_rvh =
      static_cast<RenderViewHostImpl*>(render_view_host);
  guest_rvh->StopHangMonitorTimeout();
  DCHECK(pending_input_event_reply_.get());
  IPC::Message* reply_message = pending_input_event_reply_.release();
  BrowserPluginHostMsg_HandleInputEvent::WriteReplyParams(reply_message,
                                                          handled,
                                                          cursor_);
  SendMessageToEmbedder(reply_message);
}

void BrowserPluginGuest::Stop() {
  web_contents()->Stop();
}

void BrowserPluginGuest::Reload() {
  // TODO(fsamuel): Don't check for repost because we don't want to show
  // Chromium's repost warning. We might want to implement a separate API
  // for registering a callback if a repost is about to happen.
  web_contents()->GetController().Reload(false);
}

void BrowserPluginGuest::SetFocus(bool focused) {
  RenderViewHost* render_view_host = web_contents()->GetRenderViewHost();
  render_view_host->Send(
      new ViewMsg_SetFocus(render_view_host->GetRoutingID(), focused));
}

void BrowserPluginGuest::ShowWidget(RenderViewHost* render_view_host,
                                    int route_id,
                                    const gfx::Rect& initial_pos) {
  gfx::Rect screen_pos(initial_pos);
  screen_pos.Offset(guest_rect_.origin());
  static_cast<WebContentsImpl*>(web_contents())->ShowCreatedWidget(route_id,
                                                                   screen_pos);
}

void BrowserPluginGuest::SetCursor(const WebCursor& cursor) {
  cursor_ = cursor;
}

void BrowserPluginGuest::DidCommitProvisionalLoadForFrame(
    int64 frame_id,
    bool is_main_frame,
    const GURL& url,
    PageTransition transition_type,
    RenderViewHost* render_view_host) {
  // Inform its embedder of the updated URL.
  if (is_main_frame)
    SendMessageToEmbedder(
        new BrowserPluginMsg_DidNavigate(
            instance_id(),
            url,
            render_view_host->GetProcess()->GetID()));
}

void BrowserPluginGuest::RenderViewGone(base::TerminationStatus status) {
  if (pending_input_event_reply_.get()) {
    IPC::Message* reply_message = pending_input_event_reply_.release();
    BrowserPluginHostMsg_HandleInputEvent::WriteReplyParams(reply_message,
                                                            false,
                                                            cursor_);
    SendMessageToEmbedder(reply_message);
  }
  SendMessageToEmbedder(new BrowserPluginMsg_GuestCrashed(instance_id()));
  IDMap<RenderViewHost>::const_iterator iter(&pending_updates_);
  while (!iter.IsAtEnd()) {
    pending_updates_.Remove(iter.GetCurrentKey());
    iter.Advance();
  }
}

void BrowserPluginGuest::SendMessageToEmbedder(IPC::Message* msg) {
  DCHECK(embedder_render_process_host());
  embedder_render_process_host()->Send(msg);
}

}  // namespace content
