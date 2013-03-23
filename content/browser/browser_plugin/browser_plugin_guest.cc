// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browser_plugin/browser_plugin_guest.h"

#include <algorithm>

#include "base/message_loop.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "content/browser/browser_plugin/browser_plugin_embedder.h"
#include "content/browser/browser_plugin/browser_plugin_guest_helper.h"
#include "content/browser/browser_plugin/browser_plugin_guest_manager.h"
#include "content/browser/browser_plugin/browser_plugin_host_factory.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/browser/web_contents/web_contents_view_guest.h"
#include "content/common/browser_plugin/browser_plugin_constants.h"
#include "content/common/browser_plugin/browser_plugin_messages.h"
#include "content/common/content_constants_internal.h"
#include "content/common/drag_messages.h"
#include "content/common/gpu/gpu_messages.h"
#include "content/common/view_messages.h"
#include "content/port/browser/render_view_host_delegate_view.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/geolocation_permission_context.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/resource_request_details.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents_view.h"
#include "content/public/common/media_stream_request.h"
#include "content/public/common/result_codes.h"
#include "net/base/net_errors.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebCursorInfo.h"
#include "ui/surface/transport_dib.h"
#include "webkit/glue/resource_type.h"
#include "webkit/glue/webdropdata.h"

#if defined(OS_MACOSX)
#include "content/browser/browser_plugin/browser_plugin_popup_menu_helper_mac.h"
#endif

namespace content {

// static
BrowserPluginHostFactory* BrowserPluginGuest::factory_ = NULL;

namespace {
const size_t kNumMaxOutstandingPermissionRequests = 1024;

static std::string WindowOpenDispositionToString(
  WindowOpenDisposition window_open_disposition) {
  switch (window_open_disposition) {
      case IGNORE_ACTION:
        return "ignore";
      case SAVE_TO_DISK:
        return "save_to_disk";
      case CURRENT_TAB:
        return "current_tab";
      case NEW_BACKGROUND_TAB:
        return "new_background_tab";
      case NEW_FOREGROUND_TAB:
        return "new_foreground_tab";
      case NEW_WINDOW:
        return "new_window";
      case NEW_POPUP:
        return "new_popup";
      default:
        NOTREACHED() << "Unknown Window Open Disposition";
        return "ignore";
  }
}

}

class BrowserPluginGuest::EmbedderRenderViewHostObserver
    : public RenderViewHostObserver {
 public:
  explicit EmbedderRenderViewHostObserver(BrowserPluginGuest* guest)
      : RenderViewHostObserver(
          guest->embedder_web_contents()->GetRenderViewHost()),
        browser_plugin_guest_(guest) {
  }

  virtual ~EmbedderRenderViewHostObserver() {
  }

  // RenderViewHostObserver:
  virtual void RenderViewHostDestroyed(
      RenderViewHost* render_view_host) OVERRIDE {
    browser_plugin_guest_->Destroy();
  }

 private:
  BrowserPluginGuest* browser_plugin_guest_;

  DISALLOW_COPY_AND_ASSIGN(EmbedderRenderViewHostObserver);
};

BrowserPluginGuest::BrowserPluginGuest(
    int instance_id,
    WebContentsImpl* web_contents)
    : WebContentsObserver(web_contents),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)),
      embedder_web_contents_(NULL),
      instance_id_(instance_id),
      damage_buffer_sequence_id_(0),
      damage_buffer_size_(0),
      damage_buffer_scale_factor_(1.0f),
      guest_hang_timeout_(
          base::TimeDelta::FromMilliseconds(kHungRendererDelayMs)),
      focused_(false),
      mouse_locked_(false),
      pending_lock_request_(false),
      embedder_visible_(true),
      opener_(NULL),
      next_permission_request_id_(0) {
  DCHECK(web_contents);
  web_contents->SetDelegate(this);
  GetWebContents()->GetBrowserPluginGuestManager()->AddGuest(instance_id_,
                                                             GetWebContents());
}

void BrowserPluginGuest::DestroyUnattachedWindows() {
  // Destroy() reaches in and removes the BrowserPluginGuest from its opener's
  // pending_new_windows_ set. To avoid mutating the set while iterating, we
  // create a copy of the pending new windows set and iterate over the copy.
  PendingWindowMap pending_new_windows(pending_new_windows_);
  // Clean up unattached new windows opened by this guest.
  for (PendingWindowMap::const_iterator it = pending_new_windows.begin();
       it != pending_new_windows.end(); ++it) {
    it->first->Destroy();
  }
  // All pending windows should be removed from the set after Destroy() is
  // called on all of them.
  DCHECK_EQ(0ul, pending_new_windows_.size());
}

void BrowserPluginGuest::Destroy() {
  if (!attached() && opener())
    opener()->pending_new_windows_.erase(this);
  DestroyUnattachedWindows();
  GetWebContents()->GetBrowserPluginGuestManager()->RemoveGuest(instance_id_);
  delete GetWebContents();
}

bool BrowserPluginGuest::OnMessageReceivedFromEmbedder(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(BrowserPluginGuest, message)
    IPC_MESSAGE_HANDLER(BrowserPluginHostMsg_BuffersSwappedACK,
                        OnSwapBuffersACK)
    IPC_MESSAGE_HANDLER(BrowserPluginHostMsg_DragStatusUpdate,
                        OnDragStatusUpdate)
    IPC_MESSAGE_HANDLER(BrowserPluginHostMsg_Go, OnGo)
    IPC_MESSAGE_HANDLER(BrowserPluginHostMsg_HandleInputEvent,
                        OnHandleInputEvent)
    IPC_MESSAGE_HANDLER(BrowserPluginHostMsg_LockMouse_ACK, OnLockMouseAck)
    IPC_MESSAGE_HANDLER(BrowserPluginHostMsg_NavigateGuest, OnNavigateGuest)
    IPC_MESSAGE_HANDLER(BrowserPluginHostMsg_PluginDestroyed, OnPluginDestroyed)
    IPC_MESSAGE_HANDLER(BrowserPluginHostMsg_Reload, OnReload)
    IPC_MESSAGE_HANDLER(BrowserPluginHostMsg_ResizeGuest, OnResizeGuest)
    IPC_MESSAGE_HANDLER(BrowserPluginHostMsg_RespondPermission,
                        OnRespondPermission)
    IPC_MESSAGE_HANDLER(BrowserPluginHostMsg_SetAutoSize, OnSetSize)
    IPC_MESSAGE_HANDLER(BrowserPluginHostMsg_SetFocus, OnSetFocus)
    IPC_MESSAGE_HANDLER(BrowserPluginHostMsg_SetName, OnSetName)
    IPC_MESSAGE_HANDLER(BrowserPluginHostMsg_SetVisibility, OnSetVisibility)
    IPC_MESSAGE_HANDLER(BrowserPluginHostMsg_Stop, OnStop)
    IPC_MESSAGE_HANDLER(BrowserPluginHostMsg_TerminateGuest, OnTerminateGuest)
    IPC_MESSAGE_HANDLER(BrowserPluginHostMsg_UnlockMouse_ACK, OnUnlockMouseAck)
    IPC_MESSAGE_HANDLER(BrowserPluginHostMsg_UpdateRect_ACK, OnUpdateRectACK)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void BrowserPluginGuest::Initialize(
    WebContentsImpl* embedder_web_contents,
    const BrowserPluginHostMsg_CreateGuest_Params& params) {
  focused_ = params.focused;
  guest_visible_ = params.visible;
  if (!params.name.empty())
    name_ = params.name;
  auto_size_enabled_ = params.auto_size_params.enable;
  max_auto_size_ = params.auto_size_params.max_size;
  min_auto_size_ = params.auto_size_params.min_size;

  // Once a BrowserPluginGuest has an embedder WebContents, it's considered to
  // be attached.
  embedder_web_contents_ = embedder_web_contents;

  // |render_view_host| manages the ownership of this BrowserPluginGuestHelper.
  new BrowserPluginGuestHelper(this, GetWebContents()->GetRenderViewHost());

  RendererPreferences* renderer_prefs =
      GetWebContents()->GetMutableRendererPrefs();
  // Copy renderer preferences (and nothing else) from the embedder's
  // WebContents to the guest.
  //
  // For GTK and Aura this is necessary to get proper renderer configuration
  // values for caret blinking interval, colors related to selection and
  // focus.
  *renderer_prefs = *embedder_web_contents_->GetMutableRendererPrefs();

  renderer_prefs->throttle_input_events = false;
  // We would like the guest to report changes to frame names so that we can
  // update the BrowserPlugin's corresponding 'name' attribute.
  // TODO(fsamuel): Remove this once http://crbug.com/169110 is addressed.
  renderer_prefs->report_frame_name_changes = true;
  // Navigation is disabled in Chrome Apps. We want to make sure guest-initiated
  // navigations still continue to function inside the app.
  renderer_prefs->browser_handles_all_top_level_requests = false;

  notification_registrar_.Add(
      this, NOTIFICATION_RESOURCE_RECEIVED_REDIRECT,
      Source<WebContents>(GetWebContents()));

  // Listen to embedder visibility changes so that the guest is in a 'shown'
  // state if both the embedder is visible and the BrowserPlugin is marked as
  // visible.
  notification_registrar_.Add(
      this, NOTIFICATION_WEB_CONTENTS_VISIBILITY_CHANGED,
      Source<WebContents>(embedder_web_contents_));

  embedder_rvh_observer_.reset(new EmbedderRenderViewHostObserver(this));

  OnSetSize(instance_id_, params.auto_size_params, params.resize_guest_params);

  // Create a swapped out RenderView for the guest in the embedder render
  // process, so that the embedder can access the guest's window object.
  int guest_routing_id =
      GetWebContents()->CreateSwappedOutRenderView(
          embedder_web_contents_->GetSiteInstance());
  SendMessageToEmbedder(
      new BrowserPluginMsg_GuestContentWindowReady(instance_id_,
                                                   guest_routing_id));

  if (!params.src.empty())
    OnNavigateGuest(instance_id_, params.src);

  GetContentClient()->browser()->GuestWebContentsCreated(
      GetWebContents(), embedder_web_contents_);
}

BrowserPluginGuest::~BrowserPluginGuest() {
}

// static
BrowserPluginGuest* BrowserPluginGuest::Create(
    int instance_id,
    WebContentsImpl* web_contents) {
  RecordAction(UserMetricsAction("BrowserPlugin.Guest.Create"));
  if (factory_)
    return factory_->CreateBrowserPluginGuest(instance_id, web_contents);
  return new BrowserPluginGuest(instance_id, web_contents);
}

RenderWidgetHostView* BrowserPluginGuest::GetEmbedderRenderWidgetHostView() {
  return embedder_web_contents_->GetRenderWidgetHostView();
}

void BrowserPluginGuest::UpdateVisibility() {
  OnSetVisibility(instance_id_, visible());
}

void BrowserPluginGuest::Observe(int type,
                                 const NotificationSource& source,
                                 const NotificationDetails& details) {
  switch (type) {
    case NOTIFICATION_RESOURCE_RECEIVED_REDIRECT: {
      DCHECK_EQ(Source<WebContents>(source).ptr(), GetWebContents());
      ResourceRedirectDetails* resource_redirect_details =
            Details<ResourceRedirectDetails>(details).ptr();
      bool is_top_level =
          resource_redirect_details->resource_type == ResourceType::MAIN_FRAME;
      LoadRedirect(resource_redirect_details->url,
                   resource_redirect_details->new_url,
                   is_top_level);
      break;
    }
    case NOTIFICATION_WEB_CONTENTS_VISIBILITY_CHANGED: {
      DCHECK_EQ(Source<WebContents>(source).ptr(), embedder_web_contents_);
      embedder_visible_ = *Details<bool>(details).ptr();
      UpdateVisibility();
      break;
    }
    default:
      NOTREACHED() << "Unexpected notification sent.";
      break;
  }
}

void BrowserPluginGuest::AddNewContents(WebContents* source,
                                        WebContents* new_contents,
                                        WindowOpenDisposition disposition,
                                        const gfx::Rect& initial_pos,
                                        bool user_gesture,
                                        bool* was_blocked) {
  *was_blocked = false;
  RequestNewWindowPermission(static_cast<WebContentsImpl*>(new_contents),
                             disposition, initial_pos, user_gesture);
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

void BrowserPluginGuest::WebContentsCreated(WebContents* source_contents,
                                            int64 source_frame_id,
                                            const string16& frame_name,
                                            const GURL& target_url,
                                            WebContents* new_contents) {
  WebContentsImpl* new_contents_impl =
      static_cast<WebContentsImpl*>(new_contents);
  BrowserPluginGuest* guest = new_contents_impl->GetBrowserPluginGuest();
  guest->opener_ = this;
  guest->name_ = UTF16ToUTF8(frame_name);
  // Take ownership of the new guest until it is attached to the embedder's DOM
  // tree to avoid leaking a guest if this guest is destroyed before attaching
  // the new guest.
  pending_new_windows_.insert(make_pair(guest, target_url.spec()));
}

void BrowserPluginGuest::RendererUnresponsive(WebContents* source) {
  int process_id =
      GetWebContents()->GetRenderProcessHost()->GetID();
  SendMessageToEmbedder(
      new BrowserPluginMsg_GuestUnresponsive(instance_id(), process_id));
  RecordAction(UserMetricsAction("BrowserPlugin.Guest.Hung"));
}

void BrowserPluginGuest::RendererResponsive(WebContents* source) {
  int process_id =
      GetWebContents()->GetRenderProcessHost()->GetID();
  SendMessageToEmbedder(
      new BrowserPluginMsg_GuestResponsive(instance_id(), process_id));
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

WebContentsImpl* BrowserPluginGuest::GetWebContents() {
  return static_cast<WebContentsImpl*>(web_contents());
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

bool BrowserPluginGuest::InAutoSizeBounds(const gfx::Size& size) const {
  return size.width() <= max_auto_size_.width() &&
      size.height() <= max_auto_size_.height();
}

void BrowserPluginGuest::RequestNewWindowPermission(
    WebContentsImpl* new_contents,
    WindowOpenDisposition disposition,
    const gfx::Rect& initial_bounds,
    bool user_gesture) {
  BrowserPluginGuest* guest = new_contents->GetBrowserPluginGuest();
  PendingWindowMap::iterator it = pending_new_windows_.find(guest);
  if (it == pending_new_windows_.end())
    return;
  const std::string& target_url = it->second;
  base::DictionaryValue request_info;
  request_info.Set(browser_plugin::kInitialHeight,
                   base::Value::CreateIntegerValue(initial_bounds.height()));
  request_info.Set(browser_plugin::kInitialWidth,
                   base::Value::CreateIntegerValue(initial_bounds.width()));
  request_info.Set(browser_plugin::kTargetURL,
                   base::Value::CreateStringValue(target_url));
  request_info.Set(browser_plugin::kWindowID,
                   base::Value::CreateIntegerValue(guest->instance_id()));
  request_info.Set(browser_plugin::kWindowOpenDisposition,
                   base::Value::CreateStringValue(
                       WindowOpenDispositionToString(disposition)));
  int request_id = next_permission_request_id_++;
  new_window_request_map_[request_id] = guest->instance_id();
  SendMessageToEmbedder(new BrowserPluginMsg_RequestPermission(
      instance_id(), BrowserPluginPermissionTypeNewWindow,
      request_id, request_info));
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
      new BrowserPluginMsg_LoadStart(instance_id(),
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
      new BrowserPluginMsg_LoadAbort(instance_id(),
                                     validated_url,
                                     is_main_frame,
                                     error_type));
}

void BrowserPluginGuest::SendMessageToEmbedder(IPC::Message* msg) {
  if (!attached()) {
    delete msg;
    return;
  }
  msg->set_routing_id(embedder_web_contents_->GetRoutingID());
  embedder_web_contents_->Send(msg);
}

void BrowserPluginGuest::LoadRedirect(
    const GURL& old_url,
    const GURL& new_url,
    bool is_top_level) {
  SendMessageToEmbedder(
      new BrowserPluginMsg_LoadRedirect(instance_id(),
                                        old_url,
                                        new_url,
                                        is_top_level));
}

void BrowserPluginGuest::AskEmbedderForGeolocationPermission(
    int bridge_id,
    const GURL& requesting_frame,
    const GeolocationCallback& callback) {
  if (geolocation_request_callback_map_.size() >=
          kNumMaxOutstandingPermissionRequests) {
    // Deny the geolocation request.
    callback.Run(false);
    return;
  }
  int request_id = next_permission_request_id_++;
  geolocation_request_callback_map_[request_id] = callback;

  base::DictionaryValue request_info;
  request_info.Set(browser_plugin::kURL,
                   base::Value::CreateStringValue(requesting_frame.spec()));

  SendMessageToEmbedder(
      new BrowserPluginMsg_RequestPermission(instance_id(),
          BrowserPluginPermissionTypeGeolocation, request_id, request_info));
}

void BrowserPluginGuest::CancelGeolocationRequest(int bridge_id) {
  GeolocationRequestsMap::iterator callback_iter =
      geolocation_request_callback_map_.find(bridge_id);
  if (callback_iter != geolocation_request_callback_map_.end())
    geolocation_request_callback_map_.erase(callback_iter);
}

void BrowserPluginGuest::SetGeolocationPermission(int request_id,
                                                  bool allowed) {
  GeolocationRequestsMap::iterator callback_iter =
      geolocation_request_callback_map_.find(request_id);
  if (callback_iter != geolocation_request_callback_map_.end()) {
    callback_iter->second.Run(allowed);
    geolocation_request_callback_map_.erase(callback_iter);
  }
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
  params.route_id = render_view_host->GetRoutingID();
  params.current_entry_index =
      GetWebContents()->GetController().GetCurrentEntryIndex();
  params.entry_count =
      GetWebContents()->GetController().GetEntryCount();
  SendMessageToEmbedder(
      new BrowserPluginMsg_LoadCommit(instance_id(), params));
  RecordAction(UserMetricsAction("BrowserPlugin.Guest.DidNavigate"));
}

void BrowserPluginGuest::DidStopLoading(RenderViewHost* render_view_host) {
  // Initiating a drag from inside a guest is currently not supported. So inject
  // some JS to disable it. http://crbug.com/161112
  const char script[] = "window.addEventListener('dragstart', function() { "
                        "  window.event.preventDefault(); "
                        "});";
  render_view_host->ExecuteJavascriptInWebFrame(string16(),
                                                ASCIIToUTF16(script));
  SendMessageToEmbedder(new BrowserPluginMsg_LoadStop(instance_id()));
}

void BrowserPluginGuest::RenderViewReady() {
  // TODO(fsamuel): Investigate whether it's possible to update state earlier
  // here (see http://crbug.com/158151).
  Send(new ViewMsg_SetFocus(routing_id(), focused_));
  UpdateVisibility();
  RenderViewHost* rvh = GetWebContents()->GetRenderViewHost();
  if (auto_size_enabled_)
    rvh->EnableAutoResize(min_auto_size_, max_auto_size_);
  else
    rvh->DisableAutoResize(damage_view_size_);

  Send(new ViewMsg_SetName(routing_id(), name_));
}

void BrowserPluginGuest::RenderViewGone(base::TerminationStatus status) {
  int process_id = GetWebContents()->GetRenderProcessHost()->GetID();
  SendMessageToEmbedder(
      new BrowserPluginMsg_GuestGone(instance_id(), process_id, status));
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

// static
void BrowserPluginGuest::AcknowledgeBufferPresent(
    int route_id,
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

// static
bool BrowserPluginGuest::ShouldForwardToBrowserPluginGuest(
    const IPC::Message& message) {
  switch (message.type()) {
    case BrowserPluginHostMsg_BuffersSwappedACK::ID:
    case BrowserPluginHostMsg_DragStatusUpdate::ID:
    case BrowserPluginHostMsg_Go::ID:
    case BrowserPluginHostMsg_HandleInputEvent::ID:
    case BrowserPluginHostMsg_LockMouse_ACK::ID:
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
    case BrowserPluginHostMsg_UnlockMouse_ACK::ID:
    case BrowserPluginHostMsg_UpdateRect_ACK::ID:
      return true;
    default:
      break;
  }
  return false;
}

bool BrowserPluginGuest::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(BrowserPluginGuest, message)
    IPC_MESSAGE_HANDLER(ViewHostMsg_HandleInputEvent_ACK, OnHandleInputEventAck)
    IPC_MESSAGE_HANDLER(ViewHostMsg_HasTouchEventHandlers,
                        OnHasTouchEventHandlers)
    IPC_MESSAGE_HANDLER(ViewHostMsg_LockMouse, OnLockMouse)
    IPC_MESSAGE_HANDLER(ViewHostMsg_SetCursor, OnSetCursor)
 #if defined(OS_MACOSX)
    // MacOSX creates and populates platform-specific select drop-down menus
    // whereas other platforms merely create a popup window that the guest
    // renderer process paints inside.
    IPC_MESSAGE_HANDLER(ViewHostMsg_ShowPopup, OnShowPopup)
 #endif
    IPC_MESSAGE_HANDLER(ViewHostMsg_ShowView, OnShowView)
    IPC_MESSAGE_HANDLER(ViewHostMsg_ShowWidget, OnShowWidget)
    IPC_MESSAGE_HANDLER(ViewHostMsg_TakeFocus, OnTakeFocus)
    IPC_MESSAGE_HANDLER(ViewHostMsg_UnlockMouse, OnUnlockMouse)
    IPC_MESSAGE_HANDLER(DragHostMsg_UpdateDragCursor, OnUpdateDragCursor)
    IPC_MESSAGE_HANDLER(ViewHostMsg_UpdateFrameName, OnUpdateFrameName)
    IPC_MESSAGE_HANDLER(ViewHostMsg_UpdateRect, OnUpdateRect)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void BrowserPluginGuest::Attach(
    WebContentsImpl* embedder_web_contents,
    BrowserPluginHostMsg_CreateGuest_Params params) {
  const std::string target_url = opener()->pending_new_windows_[this];
  if (!GetWebContents()->opener()) {
    // For guests that have a suppressed opener, we navigate now.
    // Navigation triggers the creation of a RenderWidgetHostViewGuest so
    // we don't need to create one manually.
    params.src = target_url;
  } else {
    // Ensure that the newly attached guest gets a RenderWidgetHostViewGuest.
    WebContentsViewGuest* new_view =
        static_cast<WebContentsViewGuest*>(GetWebContents()->GetView());
    new_view->CreateViewForWidget(web_contents()->GetRenderViewHost());

    // Reply to ViewHostMsg_ShowView to inform the renderer that the browser has
    // processed the move.  The browser may have ignored the move, but it
    // finished processing.  This is used because the renderer keeps a temporary
    // cache of the widget position while these asynchronous operations are in
    // progress.
    Send(new ViewMsg_Move_ACK(web_contents()->GetRoutingID()));
  }
  // Once a new guest is attached to the DOM of the embedder page, then the
  // lifetime of the new guest is no longer managed by the opener guest.
  opener()->pending_new_windows_.erase(this);

  // The guest's frame name takes precedence over the BrowserPlugin's name.
  // The guest's frame name is assigned in
  // BrowserPluginGuest::WebContentsCreated.
  if (!name_.empty())
    params.name.clear();

  Initialize(embedder_web_contents, params);

  // We initialize the RenderViewHost after a BrowserPlugin has been attached
  // to it and is ready to receive pixels. Until a RenderViewHost is
  // initialized, it will not allow any resize requests.
  if (!GetWebContents()->GetRenderViewHost()->IsRenderViewLive()) {
    static_cast<RenderViewHostImpl*>(
        GetWebContents()->GetRenderViewHost())->Init();
  }

  // Inform the embedder BrowserPlugin of the attached guest.
  if (!name_.empty()) {
    SendMessageToEmbedder(
        new BrowserPluginMsg_UpdatedName(instance_id_, name_));
  }
}

void BrowserPluginGuest::OnDragStatusUpdate(int instance_id,
                                            WebKit::WebDragStatus drag_status,
                                            const WebDropData& drop_data,
                                            WebKit::WebDragOperationsMask mask,
                                            const gfx::Point& location) {
  RenderViewHost* host = GetWebContents()->GetRenderViewHost();
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

void BrowserPluginGuest::OnGo(int instance_id, int relative_index) {
  GetWebContents()->GetController().GoToOffset(relative_index);
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
      GetWebContents()->GetRenderViewHost());

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

  Send(message);
  guest_rvh->StartHangMonitorTimeout(guest_hang_timeout_);
}

void BrowserPluginGuest::OnLockMouse(bool user_gesture,
                                     bool last_unlocked_by_target,
                                     bool privileged) {
  if (pending_lock_request_) {
    // Immediately reject the lock because only one pointerLock may be active
    // at a time.
    Send(new ViewMsg_LockMouse_ACK(routing_id(), false));
    return;
  }
  pending_lock_request_ = true;
  int request_id = next_permission_request_id_++;
  base::DictionaryValue request_info;
  request_info.Set(browser_plugin::kUserGesture,
                   base::Value::CreateBooleanValue(user_gesture));
  request_info.Set(browser_plugin::kLastUnlockedBySelf,
                   base::Value::CreateBooleanValue(last_unlocked_by_target));
  request_info.Set(browser_plugin::kURL,
                   base::Value::CreateStringValue(
                       web_contents()->GetURL().spec()));

  SendMessageToEmbedder(new BrowserPluginMsg_RequestPermission(
      instance_id(), BrowserPluginPermissionTypePointerLock,
      request_id, request_info));
}

void BrowserPluginGuest::OnLockMouseAck(int instance_id, bool succeeded) {
  Send(new ViewMsg_LockMouse_ACK(routing_id(), succeeded));
  pending_lock_request_ = false;
  if (succeeded)
    mouse_locked_ = true;
}

void BrowserPluginGuest::OnNavigateGuest(
    int instance_id,
    const std::string& src) {
  GURL url(src);
  // We do not load empty urls in web_contents.
  // If a guest sets empty src attribute after it has navigated to some
  // non-empty page, the action is considered no-op. This empty src navigation
  // should never be sent to BrowserPluginGuest (browser process).
  DCHECK(!src.empty());
  if (!src.empty()) {
    // As guests do not swap processes on navigation, only navigations to
    // normal web URLs are supported.  No protocol handlers are installed for
    // other schemes (e.g., WebUI or extensions), and no permissions or bindings
    // can be granted to the guest process.
    GetWebContents()->GetController().LoadURL(url, Referrer(),
                                            PAGE_TRANSITION_AUTO_TOPLEVEL,
                                            std::string());
  }
}

void BrowserPluginGuest::OnPluginDestroyed(int instance_id) {
  Destroy();
}

void BrowserPluginGuest::OnReload(int instance_id) {
  // TODO(fsamuel): Don't check for repost because we don't want to show
  // Chromium's repost warning. We might want to implement a separate API
  // for registering a callback if a repost is about to happen.
  GetWebContents()->GetController().Reload(false);
}

void BrowserPluginGuest::OnResizeGuest(
    int instance_id,
    const BrowserPluginHostMsg_ResizeGuest_Params& params) {
  // BrowserPlugin manages resize flow control itself and does not depend
  // on RenderWidgetHost's mechanisms for flow control, so we reset those flags
  // here. If we are setting the size for the first time before navigating then
  // BrowserPluginGuest does not yet have a RenderViewHost.
  if (GetWebContents()->GetRenderViewHost()) {
    RenderWidgetHostImpl* render_widget_host =
        RenderWidgetHostImpl::From(GetWebContents()->GetRenderViewHost());
    render_widget_host->ResetSizeAndRepaintPendingFlags();
  }
  if (!base::SharedMemory::IsHandleValid(params.damage_buffer_handle)) {
    // Invalid damage buffer, so just resize the WebContents.
    if (!params.view_size.IsEmpty())
      GetWebContents()->GetView()->SizeContents(params.view_size);
    return;
  }
  SetDamageBuffer(params);
  GetWebContents()->GetView()->SizeContents(params.view_size);
  if (params.repaint)
    Send(new ViewMsg_Repaint(routing_id(), params.view_size));
}

void BrowserPluginGuest::OnSetFocus(int instance_id, bool focused) {
  if (focused_ == focused)
      return;
  focused_ = focused;
  Send(new ViewMsg_SetFocus(routing_id(), focused));
}

void BrowserPluginGuest::OnSetName(int instance_id, const std::string& name) {
  if (name == name_)
    return;
  name_ = name;
  Send(new ViewMsg_SetName(routing_id(), name));
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
    GetWebContents()->GetRenderViewHost()->EnableAutoResize(
        min_auto_size_, max_auto_size_);
    // TODO(fsamuel): If we're changing autosize parameters, then we force
    // the guest to completely repaint itself, because BrowserPlugin has
    // allocated a new damage buffer and expects a full frame of pixels.
    // Ideally, we shouldn't need to do this because we shouldn't need to
    // allocate a new damage buffer unless |max_auto_size_| has changed.
    // However, even in that case, layout may not change and so we may
    // not get a full frame worth of pixels.
    Send(new ViewMsg_Repaint(routing_id(), max_auto_size_));
  } else if (!auto_size_enabled_ && old_auto_size_enabled) {
    GetWebContents()->GetRenderViewHost()->DisableAutoResize(
        resize_guest_params.view_size);
  }
  OnResizeGuest(instance_id_, resize_guest_params);
}

void BrowserPluginGuest::OnSetVisibility(int instance_id, bool visible) {
  guest_visible_ = visible;
  if (embedder_visible_ && guest_visible_)
    GetWebContents()->WasShown();
  else
    GetWebContents()->WasHidden();
}

void BrowserPluginGuest::OnStop(int instance_id) {
  GetWebContents()->Stop();
}

void BrowserPluginGuest::OnRespondPermission(
    int instance_id,
    BrowserPluginPermissionType permission_type,
    int request_id,
    bool should_allow) {
  switch (permission_type) {
    case BrowserPluginPermissionTypeGeolocation:
      OnRespondPermissionGeolocation(request_id, should_allow);
      break;
    case BrowserPluginPermissionTypeMedia:
      OnRespondPermissionMedia(request_id, should_allow);
      break;
    case BrowserPluginPermissionTypeNewWindow:
      OnRespondPermissionNewWindow(request_id, should_allow);
      break;
    default:
      NOTREACHED();
      break;
  }
}

void BrowserPluginGuest::OnSwapBuffersACK(int instance_id,
                                          int route_id,
                                          int gpu_host_id,
                                          const std::string& mailbox_name,
                                          uint32 sync_point) {
  AcknowledgeBufferPresent(route_id, gpu_host_id, mailbox_name, sync_point);

// This is only relevant on MACOSX and WIN when threaded compositing
// is not enabled. In threaded mode, above ACK is sufficient.
#if defined(OS_MACOSX) || defined(OS_WIN)
  RenderWidgetHostImpl* render_widget_host =
        RenderWidgetHostImpl::From(GetWebContents()->GetRenderViewHost());
  render_widget_host->AcknowledgeSwapBuffersToRenderer();
#endif  // defined(OS_MACOSX) || defined(OS_WIN)
}

void BrowserPluginGuest::OnTerminateGuest(int instance_id) {
  RecordAction(UserMetricsAction("BrowserPlugin.Guest.Terminate"));
  base::ProcessHandle process_handle =
      GetWebContents()->GetRenderProcessHost()->GetHandle();
  if (process_handle)
    base::KillProcess(process_handle, RESULT_CODE_KILLED, false);
}

void BrowserPluginGuest::OnUnlockMouse() {
  SendMessageToEmbedder(new BrowserPluginMsg_UnlockMouse(instance_id()));
}

void BrowserPluginGuest::OnUnlockMouseAck(int instance_id) {
  // mouse_locked_ could be false here if the lock attempt was cancelled due
  // to window focus, or for various other reasons before the guest was informed
  // of the lock's success.
  if (mouse_locked_)
    Send(new ViewMsg_MouseLockLost(routing_id()));
  mouse_locked_ = false;
}

void BrowserPluginGuest::OnUpdateRectACK(
    int instance_id,
    const BrowserPluginHostMsg_AutoSize_Params& auto_size_params,
    const BrowserPluginHostMsg_ResizeGuest_Params& resize_guest_params) {
  Send(new ViewMsg_UpdateRect_ACK(routing_id()));
  OnSetSize(instance_id_, auto_size_params, resize_guest_params);
}

void BrowserPluginGuest::OnHandleInputEventAck(
      WebKit::WebInputEvent::Type event_type,
      InputEventAckState ack_result) {
  RenderViewHostImpl* guest_rvh =
      static_cast<RenderViewHostImpl*>(GetWebContents()->GetRenderViewHost());
  guest_rvh->StopHangMonitorTimeout();
}

void BrowserPluginGuest::OnHasTouchEventHandlers(bool accept) {
  SendMessageToEmbedder(
      new BrowserPluginMsg_ShouldAcceptTouchEvents(instance_id(), accept));
}

void BrowserPluginGuest::OnSetCursor(const WebCursor& cursor) {
  SendMessageToEmbedder(new BrowserPluginMsg_SetCursor(instance_id(), cursor));
}

#if defined(OS_MACOSX)
void BrowserPluginGuest::OnShowPopup(
    const ViewHostMsg_ShowPopup_Params& params) {
  gfx::Rect translated_bounds(params.bounds);
  translated_bounds.Offset(guest_window_rect_.OffsetFromOrigin());
  BrowserPluginPopupMenuHelper popup_menu_helper(
      embedder_web_contents_->GetRenderViewHost(),
      GetWebContents()->GetRenderViewHost());
  popup_menu_helper.ShowPopupMenu(translated_bounds,
                                  params.item_height,
                                  params.item_font_size,
                                  params.selected_item,
                                  params.popup_items,
                                  params.right_aligned,
                                  params.allow_multiple_selection);
}
#endif

void BrowserPluginGuest::OnShowView(int route_id,
                                    WindowOpenDisposition disposition,
                                    const  gfx::Rect& initial_bounds,
                                    bool user_gesture) {
  RenderViewHostImpl* rvh = RenderViewHostImpl::FromID(
      web_contents()->GetRenderProcessHost()->GetID(), route_id);
  WebContentsImpl* web_contents = static_cast<WebContentsImpl*>(
      WebContents::FromRenderViewHost(rvh));
  RequestNewWindowPermission(
      web_contents, disposition, initial_bounds, user_gesture);
}

void BrowserPluginGuest::OnShowWidget(int route_id,
                                      const gfx::Rect& initial_pos) {
  gfx::Rect screen_pos(initial_pos);
  screen_pos.Offset(guest_screen_rect_.OffsetFromOrigin());
  GetWebContents()->ShowCreatedWidget(route_id, screen_pos);
}

void BrowserPluginGuest::OnTakeFocus(bool reverse) {
  SendMessageToEmbedder(
      new BrowserPluginMsg_AdvanceFocus(instance_id(), reverse));
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

void BrowserPluginGuest::OnUpdateFrameName(int frame_id,
                                           bool is_top_level,
                                           const std::string& name) {
  if (!is_top_level)
    return;

  name_ = name;
  SendMessageToEmbedder(new BrowserPluginMsg_UpdatedName(instance_id_, name));
}

void BrowserPluginGuest::RequestMediaAccessPermission(
    WebContents* web_contents,
    const MediaStreamRequest& request,
    const MediaResponseCallback& callback) {
  if (media_requests_map_.size() >= kNumMaxOutstandingPermissionRequests) {
    // Deny the media request.
    callback.Run(MediaStreamDevices());
    return;
  }
  int request_id = next_permission_request_id_++;
  media_requests_map_.insert(
      std::make_pair(request_id,
                     std::make_pair(request, callback)));

  base::DictionaryValue request_info;
  request_info.Set(
      browser_plugin::kURL,
      base::Value::CreateStringValue(request.security_origin.spec()));
  SendMessageToEmbedder(new BrowserPluginMsg_RequestPermission(
      instance_id(), BrowserPluginPermissionTypeMedia,
      request_id, request_info));
}

void BrowserPluginGuest::OnUpdateRect(
    const ViewHostMsg_UpdateRect_Params& params) {

  BrowserPluginMsg_UpdateRect_Params relay_params;
  relay_params.view_size = params.view_size;
  relay_params.scale_factor = params.scale_factor;
  relay_params.is_resize_ack = ViewHostMsg_UpdateRect_Flags::is_resize_ack(
      params.flags);
  relay_params.needs_ack = params.needs_ack;

  // HW accelerated case, acknowledge resize only
  if (!params.needs_ack || !damage_buffer_) {
    relay_params.damage_buffer_sequence_id = 0;
    SendMessageToEmbedder(
        new BrowserPluginMsg_UpdateRect(instance_id(), relay_params));
    return;
  }

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
    TransportDIB* dib = GetWebContents()->GetRenderProcessHost()->
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

  SendMessageToEmbedder(
      new BrowserPluginMsg_UpdateRect(instance_id(), relay_params));
}

void BrowserPluginGuest::OnRespondPermissionGeolocation(
    int request_id, bool should_allow) {
  if (should_allow && embedder_web_contents_) {
    // If renderer side embedder decides to allow gelocation, we need to check
    // if the app/embedder itself has geolocation access.
    BrowserContext* browser_context =
        embedder_web_contents_->GetBrowserContext();
    if (browser_context) {
      GeolocationPermissionContext* geolocation_context =
          browser_context->GetGeolocationPermissionContext();
      if (geolocation_context) {
        base::Callback<void(bool)> geolocation_callback = base::Bind(
            &BrowserPluginGuest::SetGeolocationPermission,
            weak_ptr_factory_.GetWeakPtr(),
            request_id);
        geolocation_context->RequestGeolocationPermission(
            embedder_web_contents_->GetRenderProcessHost()->GetID(),
            embedder_web_contents_->GetRoutingID(),
            // The geolocation permission request here is not initiated through
            // WebGeolocationPermissionRequest. We are only interested in the
            // fact whether the embedder/app has geolocation permission.
            // Therefore we use an invalid |bridge_id|.
            -1 /* bridge_id */,
            embedder_web_contents_->GetURL(),
            geolocation_callback);
        return;
      }
    }
  }
  SetGeolocationPermission(request_id, false);
}

void BrowserPluginGuest::OnRespondPermissionMedia(
    int request_id, bool should_allow) {
  MediaStreamRequestsMap::iterator media_request_iter =
      media_requests_map_.find(request_id);
  if (media_request_iter == media_requests_map_.end()) {
    LOG(INFO) << "Not a valid request ID.";
    return;
  }
  const MediaStreamRequest& request = media_request_iter->second.first;
  const MediaResponseCallback& callback =
      media_request_iter->second.second;

  if (should_allow && embedder_web_contents_) {
    // Re-route the request to the embedder's WebContents; the guest gets the
    // permission this way.
    embedder_web_contents_->RequestMediaAccessPermission(request, callback);
  } else {
    // Deny the request.
    callback.Run(MediaStreamDevices());
  }
  media_requests_map_.erase(media_request_iter);
}

void BrowserPluginGuest::OnRespondPermissionNewWindow(
    int request_id, bool should_allow) {
  NewWindowRequestMap::iterator new_window_request_iter =
      new_window_request_map_.find(request_id);
  if (new_window_request_iter == new_window_request_map_.end()) {
    LOG(INFO) << "Not a valid request ID.";
    return;
  }
  int instance_id = new_window_request_iter->second;
  int embedder_render_process_id =
      embedder_web_contents_->GetRenderProcessHost()->GetID();
  BrowserPluginGuest* guest =
      GetWebContents()->GetBrowserPluginGuestManager()->
          GetGuestByInstanceID(instance_id, embedder_render_process_id);
  if (!guest) {
    LOG(INFO) << "Guest not found. Instance ID: " << instance_id;
    return;
  }
  if (!should_allow)
    guest->Destroy();
  // If we do not destroy the guest then we allow the new window.
  new_window_request_map_.erase(new_window_request_iter);
}

}  // namespace content
