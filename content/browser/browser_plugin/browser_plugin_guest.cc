// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browser_plugin/browser_plugin_guest.h"

#include <algorithm>

#include "base/string_util.h"
#include "content/browser/browser_plugin/browser_plugin_embedder.h"
#include "content/browser/browser_plugin/browser_plugin_guest_helper.h"
#include "content/browser/browser_plugin/browser_plugin_host_factory.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/browser_plugin_messages.h"
#include "content/common/view_messages.h"
#include "content/port/browser/render_view_host_delegate_view.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/resource_request_details.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents_view.h"
#include "content/public/common/result_codes.h"
#include "content/browser/browser_plugin/browser_plugin_host_factory.h"
#include "net/base/net_errors.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebCursorInfo.h"
#include "ui/surface/transport_dib.h"
#include "webkit/glue/webdropdata.h"
#include "webkit/glue/resource_type.h"

namespace content {

// static
BrowserPluginHostFactory* BrowserPluginGuest::factory_ = NULL;

namespace {
const int kGuestHangTimeoutMs = 5000;
}

BrowserPluginGuest::BrowserPluginGuest(
    int instance_id,
    WebContentsImpl* web_contents,
    const BrowserPluginHostMsg_CreateGuest_Params& params)
    : WebContentsObserver(web_contents),
      embedder_web_contents_(NULL),
      instance_id_(instance_id),
#if defined(OS_WIN)
      damage_buffer_size_(0),
      remote_damage_buffer_handle_(0),
#endif
      damage_buffer_scale_factor_(1.0f),
      pending_update_counter_(0),
      guest_hang_timeout_(
          base::TimeDelta::FromMilliseconds(kGuestHangTimeoutMs)),
      focused_(params.focused),
      visible_(params.visible),
      auto_size_enabled_(params.auto_size_params.enable),
      max_auto_size_(params.auto_size_params.max_size),
      min_auto_size_(params.auto_size_params.min_size) {
  DCHECK(web_contents);
}

void BrowserPluginGuest::InstallHelper(
    content::RenderViewHost* render_view_host) {
  // |render_view_host| manages the ownership of this BrowserPluginGuestHelper.
  new BrowserPluginGuestHelper(this, render_view_host);

  notification_registrar_.Add(
      this, content::NOTIFICATION_RESOURCE_RECEIVED_REDIRECT,
      content::Source<content::WebContents>(web_contents()));
}

BrowserPluginGuest::~BrowserPluginGuest() {
}

// static
BrowserPluginGuest* BrowserPluginGuest::Create(
    int instance_id,
    WebContentsImpl* web_contents,
    const BrowserPluginHostMsg_CreateGuest_Params& params) {
  RecordAction(UserMetricsAction("BrowserPlugin.Guest.Create"));
  if (factory_) {
    return factory_->CreateBrowserPluginGuest(instance_id,
                                              web_contents,
                                              params);
  }
  return new BrowserPluginGuest(instance_id, web_contents,params);
}

void BrowserPluginGuest::Observe(int type,
                                 const NotificationSource& source,
                                 const NotificationDetails& details) {
  switch (type) {
    case NOTIFICATION_RESOURCE_RECEIVED_REDIRECT: {
      DCHECK_EQ(Source<WebContents>(source).ptr(), web_contents());
      ResourceRedirectDetails* resource_redirect_details =
            Details<ResourceRedirectDetails>(details).ptr();
      bool is_top_level =
          resource_redirect_details->resource_type == ResourceType::MAIN_FRAME;
      LoadRedirect(resource_redirect_details->url,
                   resource_redirect_details->new_url,
                   is_top_level);
      break;
    }
    default:
      NOTREACHED() << "Unexpected notification sent.";
      break;
  }
}

bool BrowserPluginGuest::ViewTakeFocus(bool reverse) {
  SendMessageToEmbedder(
      new BrowserPluginMsg_AdvanceFocus(embedder_routing_id(),
                                        instance_id(),
                                        reverse));
  return true;
}

void BrowserPluginGuest::Go(int relative_index) {
  web_contents()->GetController().GoToOffset(relative_index);
}

bool BrowserPluginGuest::CanDownload(RenderViewHost* render_view_host,
                                    int request_id,
                                    const std::string& request_method) {
  // TODO(fsamuel): We disable downloads in guests for now, but we will later
  // expose API to allow embedders to handle them.
  // Note: it seems content_shell ignores this. This should be fixed
  // for debugging and test purposes.
  return false;
}

bool BrowserPluginGuest::HandleContextMenu(
    const ContextMenuParams& params) {
  // TODO(fsamuel): We have a do nothing context menu handler for now until
  // we implement the Apps Context Menu API for Browser Plugin (see
  // http://crbug.com/140315).
  return true;
}

void BrowserPluginGuest::RendererUnresponsive(WebContents* source) {
  base::ProcessHandle process_handle =
      web_contents()->GetRenderProcessHost()->GetHandle();
  base::KillProcess(process_handle, RESULT_CODE_HUNG, false);
  RecordAction(UserMetricsAction("BrowserPlugin.Guest.Hung"));
}

void BrowserPluginGuest::RunFileChooser(WebContents* web_contents,
                                        const FileChooserParams& params) {
  embedder_web_contents_->GetDelegate()->RunFileChooser(web_contents, params);
}

bool BrowserPluginGuest::ShouldFocusPageAfterCrash() {
  // Rather than managing focus in WebContentsImpl::RenderViewReady, we will
  // manage the focus ourselves.
  return false;
}

void BrowserPluginGuest::SetIsAcceptingTouchEvents(bool accept) {
  SendMessageToEmbedder(
      new BrowserPluginMsg_ShouldAcceptTouchEvents(embedder_routing_id(),
                                                   instance_id(),
                                                   accept));
}

void BrowserPluginGuest::SetVisibility(bool embedder_visible, bool visible) {
  visible_ = visible;
  if (embedder_visible && visible)
    web_contents()->WasShown();
  else
    web_contents()->WasHidden();
}

void BrowserPluginGuest::DragStatusUpdate(WebKit::WebDragStatus drag_status,
                                          const WebDropData& drop_data,
                                          WebKit::WebDragOperationsMask mask,
                                          const gfx::Point& location) {
  RenderViewHost* host = web_contents()->GetRenderViewHost();
  switch (drag_status) {
    case WebKit::WebDragStatusEnter:
      host->DragTargetDragEnter(drop_data, location, location, mask, 0);
      break;
    case WebKit::WebDragStatusOver:
      host->DragTargetDragOver(location, location, mask, 0);
      break;
    case WebKit::WebDragStatusLeave:
      host->DragTargetDragLeave();
      break;
    case WebKit::WebDragStatusDrop:
      host->DragTargetDrop(location, location, 0);
      break;
    case WebKit::WebDragStatusUnknown:
      NOTREACHED();
  }
}

void BrowserPluginGuest::SetSize(
    const BrowserPluginHostMsg_AutoSize_Params& auto_size_params,
    const BrowserPluginHostMsg_ResizeGuest_Params& resize_guest_params) {
  bool old_auto_size_enabled = auto_size_enabled_;
  gfx::Size old_max_size = max_auto_size_;
  gfx::Size old_min_size = min_auto_size_;
  auto_size_enabled_ = auto_size_params.enable;
  max_auto_size_ = auto_size_params.max_size;
  min_auto_size_ = auto_size_params.min_size;
  if (auto_size_enabled_ && (!old_auto_size_enabled ||
                             (old_max_size != max_auto_size_) ||
                             (old_min_size != min_auto_size_))) {
    web_contents()->GetRenderViewHost()->EnableAutoResize(
        min_auto_size_, max_auto_size_);
    // TODO(fsamuel): If we're changing autosize parameters, then we force
    // the guest to completely repaint itself, because BrowserPlugin has
    // allocated a new damage buffer and expects a full frame of pixels.
    // Ideally, we shouldn't need to do this because we shouldn't need to
    // allocate a new damage buffer unless |max_auto_size_| has changed.
    // However, even in that case, layout may not change and so we may
    // not get a full frame worth of pixels.
    web_contents()->GetRenderViewHost()->Send(new ViewMsg_Repaint(
        web_contents()->GetRenderViewHost()->GetRoutingID(),
        max_auto_size_));
  } else if (!auto_size_enabled_ && old_auto_size_enabled) {
    web_contents()->GetRenderViewHost()->DisableAutoResize(
        resize_guest_params.view_size);
  }
  Resize(embedder_web_contents_->GetRenderViewHost(), resize_guest_params);
}

void BrowserPluginGuest::UpdateDragCursor(WebKit::WebDragOperation operation) {
  RenderViewHostImpl* embedder_render_view_host =
      static_cast<RenderViewHostImpl*>(
          embedder_web_contents_->GetRenderViewHost());
  CHECK(embedder_render_view_host);
  RenderViewHostDelegateView* view =
      embedder_render_view_host->GetDelegate()->GetDelegateView();
  if (view)
    view->UpdateDragCursor(operation);
}

WebContents* BrowserPluginGuest::GetWebContents() {
  return web_contents();
}

void BrowserPluginGuest::Terminate() {
  RecordAction(UserMetricsAction("BrowserPlugin.Guest.Terminate"));
  base::ProcessHandle process_handle =
      web_contents()->GetRenderProcessHost()->GetHandle();
  base::KillProcess(process_handle, RESULT_CODE_KILLED, false);
}

void BrowserPluginGuest::Resize(
    RenderViewHost* embedder_rvh,
    const BrowserPluginHostMsg_ResizeGuest_Params& params) {
  // BrowserPlugin manages resize flow control itself and does not depend
  // on RenderWidgetHost's mechanisms for flow control, so we reset those flags
  // here.
  RenderWidgetHostImpl* render_widget_host =
      RenderWidgetHostImpl::From(web_contents()->GetRenderViewHost());
  render_widget_host->ResetSizeAndRepaintPendingFlags();
  if (!TransportDIB::is_valid_id(params.damage_buffer_id)) {
    // Invalid transport dib, so just resize the WebContents.
    if (!params.view_size.IsEmpty())
      web_contents()->GetView()->SizeContents(params.view_size);
    return;
  }
  TransportDIB* damage_buffer =
      GetDamageBufferFromEmbedder(embedder_rvh, params);
  SetDamageBuffer(damage_buffer,
#if defined(OS_WIN)
                  params.damage_buffer_size,
                  params.damage_buffer_id.handle,
#endif
                  params.view_size,
                  params.scale_factor);
  web_contents()->GetView()->SizeContents(params.view_size);
}

TransportDIB* BrowserPluginGuest::GetDamageBufferFromEmbedder(
    RenderViewHost* embedder_rvh,
    const BrowserPluginHostMsg_ResizeGuest_Params& params) {
  TransportDIB* damage_buffer = NULL;
#if defined(OS_WIN)
  // On Windows we need to duplicate the handle from the remote process.
  HANDLE section;
  DuplicateHandle(embedder_rvh->GetProcess()->GetHandle(),
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

void BrowserPluginGuest::SetDamageBuffer(
    TransportDIB* damage_buffer,
#if defined(OS_WIN)
    int damage_buffer_size,
    TransportDIB::Handle remote_handle,
#endif
    const gfx::Size& damage_view_size,
    float scale_factor) {
  // Sanity check: Verify that we've correctly shared the damage buffer memory
  // between the embedder and browser processes.
  DCHECK(*static_cast<unsigned int*>(damage_buffer->memory()) == 0xdeadbeef);
  damage_buffer_.reset(damage_buffer);
#if defined(OS_WIN)
  damage_buffer_size_ = damage_buffer_size;
  remote_damage_buffer_handle_ = remote_handle;
#endif
  damage_view_size_ = damage_view_size;
  damage_buffer_scale_factor_ = scale_factor;
}

gfx::Point BrowserPluginGuest::GetScreenCoordinates(
    const gfx::Point& relative_position) const {
  gfx::Point screen_pos(relative_position);
  screen_pos += guest_window_rect_.OffsetFromOrigin();
  return screen_pos;
}

int BrowserPluginGuest::embedder_routing_id() const {
  return embedder_web_contents_->GetRoutingID();
}

void BrowserPluginGuest::SetCompositingBufferData(int gpu_process_id,
                                                  uint32 client_id,
                                                  uint32 context_id,
                                                  uint32 texture_id_0,
                                                  uint32 texture_id_1,
                                                  uint32 sync_point) {
  // This is the signal for having no context
  if (texture_id_0 == 0) {
    DCHECK(texture_id_1 == 0);
    return;
  }

  DCHECK(texture_id_1 != 0);
  DCHECK(texture_id_0 != texture_id_1);

  surface_handle_ = gfx::GLSurfaceHandle(gfx::kNullPluginWindow, true);
  surface_handle_.parent_gpu_process_id = gpu_process_id;
  surface_handle_.parent_client_id = client_id;
}

bool BrowserPluginGuest::InAutoSizeBounds(const gfx::Size& size) const {
  return size.width() <= max_auto_size_.width() &&
      size.height() <= max_auto_size_.height();
}

void BrowserPluginGuest::UpdateRect(
    RenderViewHost* render_view_host,
    const ViewHostMsg_UpdateRect_Params& params) {
  // This handler is only of interest to us for the 2D software rendering path.
  // needs_ack should always be true for the 2D path.
  // TODO(fsamuel): Do we need to do something different in the 3D case?
  if (!params.needs_ack)
    return;

  // Only copy damage if the guest is in autosize mode and the guest's view size
  // is less than the maximum size or the guest's view size is equal to the
  // damage buffer's size and the guest's scale factor is equal to the damage
  // buffer's scale factor.
  // The scaling change can happen due to asynchronous updates of the DPI on a
  // resolution change.
  if (((auto_size_enabled_ && InAutoSizeBounds(params.view_size)) ||
      (params.view_size.width() == damage_view_size().width() &&
       params.view_size.height() == damage_view_size().height())) &&
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
#if defined(OS_MACOSX)
  relay_params.damage_buffer_identifier = damage_buffer_->id();
#elif defined(OS_WIN)
  // On Windows, the handle used locally differs from the handle received from
  // the embedder process, since we duplicate the remote handle.
  relay_params.damage_buffer_identifier = remote_damage_buffer_handle_;
#else
  relay_params.damage_buffer_identifier = damage_buffer_->handle();
#endif
  relay_params.bitmap_rect = params.bitmap_rect;
  relay_params.scroll_delta = params.scroll_delta;
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

  SendMessageToEmbedder(new BrowserPluginMsg_UpdateRect(embedder_routing_id(),
                                                        instance_id(),
                                                        message_id,
                                                        relay_params));
}

void BrowserPluginGuest::UpdateRectACK(
    int message_id,
    const BrowserPluginHostMsg_AutoSize_Params& auto_size_params,
    const BrowserPluginHostMsg_ResizeGuest_Params& resize_guest_params) {
  RenderViewHost* render_view_host = pending_updates_.Lookup(message_id);
  // If the guest has crashed since it sent the initial ViewHostMsg_UpdateRect
  // then the pending_updates_ map will have been cleared.
  if (render_view_host) {
    pending_updates_.Remove(message_id);
    render_view_host->Send(
        new ViewMsg_UpdateRect_ACK(render_view_host->GetRoutingID()));
  }
  SetSize(auto_size_params, resize_guest_params);
}

void BrowserPluginGuest::HandleInputEvent(RenderViewHost* render_view_host,
                                          const gfx::Rect& guest_window_rect,
                                          const gfx::Rect& guest_screen_rect,
                                          const WebKit::WebInputEvent& event,
                                          IPC::Message* reply_message) {
  DCHECK(!pending_input_event_reply_.get());
  guest_window_rect_ = guest_window_rect;
  guest_screen_rect_ = guest_screen_rect;
  RenderViewHostImpl* guest_rvh = static_cast<RenderViewHostImpl*>(
      web_contents()->GetRenderViewHost());
  IPC::Message* message = new ViewMsg_HandleInputEvent(routing_id());

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
  if (!Send(message)) {
    // If the embedder is waiting for a previous input ack, a new input message
    // won't get sent to the guest. Reply immediately with handled = false so
    // embedder doesn't hang.
    BrowserPluginHostMsg_HandleInputEvent::WriteReplyParams(
        reply_message, false /* handled */);
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
                                                          handled);
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
  if (focused_ == focused)
      return;
  focused_ = focused;
  Send(new ViewMsg_SetFocus(routing_id(), focused));
}

void BrowserPluginGuest::ShowWidget(RenderViewHost* render_view_host,
                                    int route_id,
                                    const gfx::Rect& initial_pos) {
  gfx::Rect screen_pos(initial_pos);
  screen_pos.Offset(guest_screen_rect_.OffsetFromOrigin());
  static_cast<WebContentsImpl*>(web_contents())->ShowCreatedWidget(route_id,
                                                                   screen_pos);
}

void BrowserPluginGuest::SetCursor(const WebCursor& cursor) {
  SendMessageToEmbedder(new BrowserPluginMsg_SetCursor(embedder_routing_id(),
                                                       instance_id(),
                                                       cursor));
}

void BrowserPluginGuest::DidStartProvisionalLoadForFrame(
    int64 frame_id,
    int64 parent_frame_id,
    bool is_main_frame,
    const GURL& validated_url,
    bool is_error_page,
    RenderViewHost* render_view_host) {
  // Inform the embedder of the loadStart.
  SendMessageToEmbedder(
      new BrowserPluginMsg_LoadStart(embedder_routing_id(),
                                     instance_id(),
                                     validated_url,
                                     is_main_frame));
}

void BrowserPluginGuest::DidFailProvisionalLoad(
    int64 frame_id,
    bool is_main_frame,
    const GURL& validated_url,
    int error_code,
    const string16& error_description,
    RenderViewHost* render_view_host) {
  // Translate the |error_code| into an error string.
  std::string error_type;
  RemoveChars(net::ErrorToString(error_code), "net::", &error_type);
  // Inform the embedder of the loadAbort.
  SendMessageToEmbedder(
      new BrowserPluginMsg_LoadAbort(embedder_routing_id(),
                                     instance_id(),
                                     validated_url,
                                     is_main_frame,
                                     error_type));
}

void BrowserPluginGuest::LoadRedirect(
    const GURL& old_url,
    const GURL& new_url,
    bool is_top_level) {
  SendMessageToEmbedder(
      new BrowserPluginMsg_LoadRedirect(embedder_routing_id(),
                                        instance_id(),
                                        old_url,
                                        new_url,
                                        is_top_level));
}

void BrowserPluginGuest::DidCommitProvisionalLoadForFrame(
    int64 frame_id,
    bool is_main_frame,
    const GURL& url,
    PageTransition transition_type,
    RenderViewHost* render_view_host) {
  // Inform its embedder of the updated URL.
  BrowserPluginMsg_LoadCommit_Params params;
  params.url = url;
  params.is_top_level = is_main_frame;
  params.process_id = render_view_host->GetProcess()->GetID();
  params.current_entry_index =
      web_contents()->GetController().GetCurrentEntryIndex();
  params.entry_count =
      web_contents()->GetController().GetEntryCount();
  SendMessageToEmbedder(
      new BrowserPluginMsg_LoadCommit(embedder_routing_id(),
                                      instance_id(),
                                      params));
  RecordAction(UserMetricsAction("BrowserPlugin.Guest.DidNavigate"));
}

void BrowserPluginGuest::DidStopLoading(RenderViewHost* render_view_host) {
  SendMessageToEmbedder(new BrowserPluginMsg_LoadStop(embedder_routing_id(),
                                                      instance_id()));
}

void BrowserPluginGuest::RenderViewReady() {
  // TODO(fsamuel): Investigate whether it's possible to update state earlier
  // here (see http://crbug.com/158151).
  Send(new ViewMsg_SetFocus(routing_id(), focused_));
  bool embedder_visible =
      embedder_web_contents_->GetBrowserPluginEmbedder()->visible();
  SetVisibility(embedder_visible, visible());
  if (auto_size_enabled_) {
    web_contents()->GetRenderViewHost()->EnableAutoResize(
        min_auto_size_, max_auto_size_);
  } else {
    web_contents()->GetRenderViewHost()->DisableAutoResize(damage_view_size_);
  }
}

void BrowserPluginGuest::RenderViewGone(base::TerminationStatus status) {
  if (pending_input_event_reply_.get()) {
    IPC::Message* reply_message = pending_input_event_reply_.release();
    BrowserPluginHostMsg_HandleInputEvent::WriteReplyParams(reply_message,
                                                            false);
    SendMessageToEmbedder(reply_message);
  }
  int process_id = web_contents()->GetRenderProcessHost()->GetID();
  SendMessageToEmbedder(new BrowserPluginMsg_GuestGone(embedder_routing_id(),
                                                       instance_id(),
                                                       process_id,
                                                       status));
  IDMap<RenderViewHost>::const_iterator iter(&pending_updates_);
  while (!iter.IsAtEnd()) {
    pending_updates_.Remove(iter.GetCurrentKey());
    iter.Advance();
  }

  switch (status) {
    case base::TERMINATION_STATUS_PROCESS_WAS_KILLED:
      RecordAction(UserMetricsAction("BrowserPlugin.Guest.Killed"));
      break;
    case base::TERMINATION_STATUS_PROCESS_CRASHED:
      RecordAction(UserMetricsAction("BrowserPlugin.Guest.Crashed"));
      break;
    case base::TERMINATION_STATUS_ABNORMAL_TERMINATION:
      RecordAction(UserMetricsAction("BrowserPlugin.Guest.AbnormalDeath"));
      break;
    default:
      break;
  }
}

void BrowserPluginGuest::SendMessageToEmbedder(IPC::Message* msg) {
  embedder_web_contents_->Send(msg);
}

}  // namespace content
