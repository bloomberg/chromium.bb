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
#include "content/common/content_constants_internal.h"
#include "content/common/drag_messages.h"
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

#if defined(OS_MACOSX)
#include "content/browser/browser_plugin/browser_plugin_popup_menu_helper_mac.h"
#endif

namespace content {

// static
BrowserPluginHostFactory* BrowserPluginGuest::factory_ = NULL;

BrowserPluginGuest::BrowserPluginGuest(
    int instance_id,
    WebContentsImpl* web_contents,
    const BrowserPluginHostMsg_CreateGuest_Params& params)
    : WebContentsObserver(web_contents),
      embedder_web_contents_(NULL),
      instance_id_(instance_id),
      damage_buffer_sequence_id_(0),
      damage_buffer_size_(0),
      damage_buffer_scale_factor_(1.0f),
      guest_hang_timeout_(
          base::TimeDelta::FromMilliseconds(kHungRendererDelayMs)),
      focused_(params.focused),
      visible_(params.visible),
      auto_size_enabled_(params.auto_size_params.enable),
      max_auto_size_(params.auto_size_params.max_size),
      min_auto_size_(params.auto_size_params.min_size) {
  DCHECK(web_contents);
}

bool BrowserPluginGuest::OnMessageReceivedFromEmbedder(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(BrowserPluginGuest, message)
    IPC_MESSAGE_HANDLER(BrowserPluginHostMsg_DragStatusUpdate,
                        OnDragStatusUpdate)
    IPC_MESSAGE_HANDLER(BrowserPluginHostMsg_Go, OnGo)
    IPC_MESSAGE_HANDLER(BrowserPluginHostMsg_HandleInputEvent,
                        OnHandleInputEvent)
    IPC_MESSAGE_HANDLER(BrowserPluginHostMsg_Reload, OnReload)
    IPC_MESSAGE_HANDLER(BrowserPluginHostMsg_ResizeGuest, OnResizeGuest)
    IPC_MESSAGE_HANDLER(BrowserPluginHostMsg_SetAutoSize, OnSetSize)
    IPC_MESSAGE_HANDLER(BrowserPluginHostMsg_SetFocus, OnSetFocus)
    IPC_MESSAGE_HANDLER(BrowserPluginHostMsg_SetVisibility, OnSetVisibility)
    IPC_MESSAGE_HANDLER(BrowserPluginHostMsg_Stop, OnStop)
    IPC_MESSAGE_HANDLER(BrowserPluginHostMsg_TerminateGuest, OnTerminateGuest)
    IPC_MESSAGE_HANDLER(BrowserPluginHostMsg_UpdateRect_ACK, OnUpdateRectACK)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void BrowserPluginGuest::Initialize(
    const BrowserPluginHostMsg_CreateGuest_Params& params,
    content::RenderViewHost* render_view_host) {
  // |render_view_host| manages the ownership of this BrowserPluginGuestHelper.
  new BrowserPluginGuestHelper(this, render_view_host);

  notification_registrar_.Add(
      this, content::NOTIFICATION_RESOURCE_RECEIVED_REDIRECT,
      content::Source<content::WebContents>(web_contents()));

  OnSetSize(instance_id_, params.auto_size_params, params.resize_guest_params);
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

void BrowserPluginGuest::UpdateVisibility() {
  OnSetVisibility(instance_id_, visible());
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
  int process_id =
      web_contents()->GetRenderProcessHost()->GetID();
  SendMessageToEmbedder(
      new BrowserPluginMsg_GuestUnresponsive(embedder_routing_id(),
                                             instance_id(),
                                             process_id));
  RecordAction(UserMetricsAction("BrowserPlugin.Guest.Hung"));
}

void BrowserPluginGuest::RendererResponsive(WebContents* source) {
  int process_id =
      web_contents()->GetRenderProcessHost()->GetID();
  SendMessageToEmbedder(
      new BrowserPluginMsg_GuestResponsive(embedder_routing_id(),
                                           instance_id(),
                                           process_id));
  RecordAction(UserMetricsAction("BrowserPlugin.Guest.Responsive"));
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

WebContents* BrowserPluginGuest::GetWebContents() {
  return web_contents();
}

base::SharedMemory* BrowserPluginGuest::GetDamageBufferFromEmbedder(
    const BrowserPluginHostMsg_ResizeGuest_Params& params) {
#if defined(OS_WIN)
  base::ProcessHandle handle =
      embedder_web_contents_->GetRenderProcessHost()->GetHandle();
  scoped_ptr<base::SharedMemory> shared_buf(
      new base::SharedMemory(params.damage_buffer_handle, false, handle));
#elif defined(OS_POSIX)
  scoped_ptr<base::SharedMemory> shared_buf(
      new base::SharedMemory(params.damage_buffer_handle, false));
#endif
  if (!shared_buf->Map(params.damage_buffer_size)) {
    NOTREACHED();
    return NULL;
  }
  return shared_buf.release();
}

void BrowserPluginGuest::SetDamageBuffer(
    const BrowserPluginHostMsg_ResizeGuest_Params& params) {
  damage_buffer_.reset(GetDamageBufferFromEmbedder(params));
  // Sanity check: Verify that we've correctly shared the damage buffer memory
  // between the embedder and browser processes.
  DCHECK(*static_cast<unsigned int*>(damage_buffer_->memory()) == 0xdeadbeef);
  damage_buffer_sequence_id_ = params.damage_buffer_sequence_id;
  damage_buffer_size_ = params.damage_buffer_size;
  damage_view_size_ = params.view_size;
  damage_buffer_scale_factor_ = params.scale_factor;
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

bool BrowserPluginGuest::InAutoSizeBounds(const gfx::Size& size) const {
  return size.width() <= max_auto_size_.width() &&
      size.height() <= max_auto_size_.height();
}

void BrowserPluginGuest::DidStartProvisionalLoadForFrame(
    int64 frame_id,
    int64 parent_frame_id,
    bool is_main_frame,
    const GURL& validated_url,
    bool is_error_page,
    bool is_iframe_srcdoc,
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

void BrowserPluginGuest::SendMessageToEmbedder(IPC::Message* msg) {
  embedder_web_contents_->Send(msg);
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
  UpdateVisibility();
  if (auto_size_enabled_) {
    web_contents()->GetRenderViewHost()->EnableAutoResize(
        min_auto_size_, max_auto_size_);
  } else {
    web_contents()->GetRenderViewHost()->DisableAutoResize(damage_view_size_);
  }
}

void BrowserPluginGuest::RenderViewGone(base::TerminationStatus status) {
  int process_id = web_contents()->GetRenderProcessHost()->GetID();
  SendMessageToEmbedder(new BrowserPluginMsg_GuestGone(embedder_routing_id(),
                                                       instance_id(),
                                                       process_id,
                                                       status));
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

bool BrowserPluginGuest::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(BrowserPluginGuest, message)
    IPC_MESSAGE_HANDLER(ViewHostMsg_CreateWindow, OnCreateWindow)
    IPC_MESSAGE_HANDLER(ViewHostMsg_HandleInputEvent_ACK, OnHandleInputEventAck)
    IPC_MESSAGE_HANDLER(ViewHostMsg_HasTouchEventHandlers,
                        OnHasTouchEventHandlers)
    IPC_MESSAGE_HANDLER(ViewHostMsg_SetCursor, OnSetCursor)
 #if defined(OS_MACOSX)
    // MacOSX creates and populates platform-specific select drop-down menus
    // whereas other platforms merely create a popup window that the guest
    // renderer process paints inside.
    IPC_MESSAGE_HANDLER(ViewHostMsg_ShowPopup, OnShowPopup)
 #endif
    IPC_MESSAGE_HANDLER(ViewHostMsg_ShowWidget, OnShowWidget)
    IPC_MESSAGE_HANDLER(ViewHostMsg_TakeFocus, OnTakeFocus)
    IPC_MESSAGE_HANDLER(DragHostMsg_UpdateDragCursor, OnUpdateDragCursor)
    IPC_MESSAGE_HANDLER(ViewHostMsg_UpdateRect, OnUpdateRect)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void BrowserPluginGuest::OnGo(int instance_id, int relative_index) {
  web_contents()->GetController().GoToOffset(relative_index);
}

void BrowserPluginGuest::OnDragStatusUpdate(int instance_id,
                                            WebKit::WebDragStatus drag_status,
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

void BrowserPluginGuest::OnHandleInputEvent(
    int instance_id,
    const gfx::Rect& guest_window_rect,
    const WebKit::WebInputEvent* event) {
  guest_window_rect_ = guest_window_rect;
  guest_screen_rect_ = guest_window_rect;
  guest_screen_rect_.Offset(
      embedder_web_contents_->GetRenderViewHost()->GetView()->
          GetViewBounds().OffsetFromOrigin());
  RenderViewHostImpl* guest_rvh = static_cast<RenderViewHostImpl*>(
      web_contents()->GetRenderViewHost());

  IPC::Message* message = NULL;

  // TODO(fsamuel): What should we do for keyboard_shortcut field?
  if (event->type == WebKit::WebInputEvent::KeyDown) {
    CHECK_EQ(sizeof(WebKit::WebKeyboardEvent), event->size);
    WebKit::WebKeyboardEvent key_event;
    memcpy(&key_event, event, event->size);
    key_event.type = WebKit::WebInputEvent::RawKeyDown;
    message = new ViewMsg_HandleInputEvent(routing_id(), &key_event, false);
  } else {
    message = new ViewMsg_HandleInputEvent(routing_id(), event, false);
  }

  guest_rvh->Send(message);
  guest_rvh->StartHangMonitorTimeout(guest_hang_timeout_);
}

void BrowserPluginGuest::OnReload(int instance_id) {
  // TODO(fsamuel): Don't check for repost because we don't want to show
  // Chromium's repost warning. We might want to implement a separate API
  // for registering a callback if a repost is about to happen.
  web_contents()->GetController().Reload(false);
}

void BrowserPluginGuest::OnResizeGuest(
    int instance_id,
    const BrowserPluginHostMsg_ResizeGuest_Params& params) {
  // BrowserPlugin manages resize flow control itself and does not depend
  // on RenderWidgetHost's mechanisms for flow control, so we reset those flags
  // here. If we are setting the size for the first time before navigating then
  // BrowserPluginGuest does not yet have a RenderViewHost.
  if (web_contents()->GetRenderViewHost()) {
    RenderWidgetHostImpl* render_widget_host =
        RenderWidgetHostImpl::From(web_contents()->GetRenderViewHost());
    render_widget_host->ResetSizeAndRepaintPendingFlags();
  }
  if (!base::SharedMemory::IsHandleValid(params.damage_buffer_handle)) {
    // Invalid damage buffer, so just resize the WebContents.
    if (!params.view_size.IsEmpty())
      web_contents()->GetView()->SizeContents(params.view_size);
    return;
  }
  SetDamageBuffer(params);
  web_contents()->GetView()->SizeContents(params.view_size);
}

void BrowserPluginGuest::OnSetFocus(int instance_id, bool focused) {
  if (focused_ == focused)
      return;
  focused_ = focused;
  Send(new ViewMsg_SetFocus(routing_id(), focused));
}

void BrowserPluginGuest::OnSetSize(
    int instance_id,
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
  OnResizeGuest(instance_id_, resize_guest_params);
}

void BrowserPluginGuest::OnSetVisibility(int instance_id, bool visible) {
  visible_ = visible;
  BrowserPluginEmbedder* embedder =
      embedder_web_contents_->GetBrowserPluginEmbedder();
  if (embedder->visible() && visible)
    web_contents()->WasShown();
  else
    web_contents()->WasHidden();
}

void BrowserPluginGuest::OnStop(int instance_id) {
  web_contents()->Stop();
}

void BrowserPluginGuest::OnTerminateGuest(int instance_id) {
  RecordAction(UserMetricsAction("BrowserPlugin.Guest.Terminate"));
  base::ProcessHandle process_handle =
      web_contents()->GetRenderProcessHost()->GetHandle();
  base::KillProcess(process_handle, RESULT_CODE_KILLED, false);
}

void BrowserPluginGuest::OnUpdateRectACK(
    int instance_id,
    const BrowserPluginHostMsg_AutoSize_Params& auto_size_params,
    const BrowserPluginHostMsg_ResizeGuest_Params& resize_guest_params) {
  RenderViewHost* render_view_host = web_contents()->GetRenderViewHost();
  render_view_host->Send(
      new ViewMsg_UpdateRect_ACK(render_view_host->GetRoutingID()));
  OnSetSize(instance_id_, auto_size_params, resize_guest_params);
}

void BrowserPluginGuest::OnCreateWindow(
    const ViewHostMsg_CreateWindow_Params& params,
    int* route_id,
    int* surface_id,
    int64* cloned_session_storage_namespace_id) {
  // TODO(fsamuel): We do not currently support window.open.
  // See http://crbug.com/140316.
  *route_id = MSG_ROUTING_NONE;
  *surface_id = 0;
  *cloned_session_storage_namespace_id = 0l;
}

void BrowserPluginGuest::OnHandleInputEventAck(
      WebKit::WebInputEvent::Type event_type,
      InputEventAckState ack_result) {
  RenderViewHostImpl* guest_rvh =
      static_cast<RenderViewHostImpl*>(web_contents()->GetRenderViewHost());
  guest_rvh->StopHangMonitorTimeout();
}

void BrowserPluginGuest::OnHasTouchEventHandlers(bool accept) {
  SendMessageToEmbedder(
      new BrowserPluginMsg_ShouldAcceptTouchEvents(embedder_routing_id(),
                                                   instance_id(),
                                                   accept));
}

void BrowserPluginGuest::OnSetCursor(const WebCursor& cursor) {
  SendMessageToEmbedder(new BrowserPluginMsg_SetCursor(embedder_routing_id(),
                                                       instance_id(),
                                                       cursor));
}

#if defined(OS_MACOSX)
void BrowserPluginGuest::OnShowPopup(
    const ViewHostMsg_ShowPopup_Params& params) {
  gfx::Rect translated_bounds(params.bounds);
  translated_bounds.Offset(guest_window_rect_.OffsetFromOrigin());
  BrowserPluginPopupMenuHelper popup_menu_helper(
      embedder_web_contents_->GetRenderViewHost(),
      web_contents()->GetRenderViewHost());
  popup_menu_helper.ShowPopupMenu(translated_bounds,
                                  params.item_height,
                                  params.item_font_size,
                                  params.selected_item,
                                  params.popup_items,
                                  params.right_aligned,
                                  params.allow_multiple_selection);
}
#endif

void BrowserPluginGuest::OnShowWidget(int route_id,
                                      const gfx::Rect& initial_pos) {
  gfx::Rect screen_pos(initial_pos);
  screen_pos.Offset(guest_screen_rect_.OffsetFromOrigin());
  static_cast<WebContentsImpl*>(web_contents())->ShowCreatedWidget(route_id,
                                                                   screen_pos);
}

void BrowserPluginGuest::OnTakeFocus(bool reverse) {
  SendMessageToEmbedder(
      new BrowserPluginMsg_AdvanceFocus(embedder_routing_id(),
                                        instance_id(),
                                        reverse));
}

void BrowserPluginGuest::OnUpdateDragCursor(
    WebKit::WebDragOperation operation) {
  RenderViewHostImpl* embedder_render_view_host =
      static_cast<RenderViewHostImpl*>(
          embedder_web_contents_->GetRenderViewHost());
  CHECK(embedder_render_view_host);
  RenderViewHostDelegateView* view =
      embedder_render_view_host->GetDelegate()->GetDelegateView();
  if (view)
    view->UpdateDragCursor(operation);
}

void BrowserPluginGuest::OnUpdateRect(
    const ViewHostMsg_UpdateRect_Params& params) {

  BrowserPluginMsg_UpdateRect_Params relay_params;
  relay_params.view_size = params.view_size;
  relay_params.scale_factor = params.scale_factor;
  relay_params.is_resize_ack = ViewHostMsg_UpdateRect_Flags::is_resize_ack(
      params.flags);

  // HW accelerated case, acknowledge resize only
  if (!params.needs_ack) {
    relay_params.damage_buffer_sequence_id = 0;
    SendMessageToEmbedder(new BrowserPluginMsg_UpdateRect(
        embedder_routing_id(),
        instance_id(),
        relay_params));
     return;
  }

  RenderViewHost* render_view_host = web_contents()->GetRenderViewHost();
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
      size_t guest_damage_buffer_size =
#if defined(OS_WIN)
          params.bitmap_rect.width() *
          params.bitmap_rect.height() * 4;
#else
          dib->size();
#endif
      size_t embedder_damage_buffer_size = damage_buffer_size_;
      void* guest_memory = dib->memory();
      void* embedder_memory = damage_buffer_->memory();
      size_t size = std::min(guest_damage_buffer_size,
                             embedder_damage_buffer_size);
      memcpy(embedder_memory, guest_memory, size);
    }
  }
  relay_params.damage_buffer_sequence_id = damage_buffer_sequence_id_;
  relay_params.bitmap_rect = params.bitmap_rect;
  relay_params.scroll_delta = params.scroll_delta;
  relay_params.scroll_rect = params.scroll_rect;
  relay_params.copy_rects = params.copy_rects;

  SendMessageToEmbedder(new BrowserPluginMsg_UpdateRect(embedder_routing_id(),
                                                        instance_id(),
                                                        relay_params));
}

}  // namespace content
