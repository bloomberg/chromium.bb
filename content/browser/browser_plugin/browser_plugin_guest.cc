// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browser_plugin/browser_plugin_guest.h"

#include <algorithm>

#include "base/command_line.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "content/browser/browser_plugin/browser_plugin_embedder.h"
#include "content/browser/browser_plugin/browser_plugin_guest_helper.h"
#include "content/browser/browser_plugin/browser_plugin_guest_manager.h"
#include "content/browser/browser_plugin/browser_plugin_host_factory.h"
#include "content/browser/browser_thread_impl.h"
#include "content/browser/loader/resource_dispatcher_host_impl.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/browser/web_contents/web_contents_view_guest.h"
#include "content/common/browser_plugin/browser_plugin_constants.h"
#include "content/common/browser_plugin/browser_plugin_messages.h"
#include "content/common/content_constants_internal.h"
#include "content/common/drag_messages.h"
#include "content/common/gpu/gpu_messages.h"
#include "content/common/input_messages.h"
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
#include "content/public/common/content_switches.h"
#include "content/public/common/media_stream_request.h"
#include "content/public/common/result_codes.h"
#include "net/base/net_errors.h"
#include "net/url_request/url_request.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebCursorInfo.h"
#include "ui/base/keycodes/keyboard_codes.h"
#include "ui/surface/transport_dib.h"
#include "webkit/glue/resource_type.h"
#include "webkit/glue/webdropdata.h"

#if defined(OS_MACOSX)
#include "content/browser/browser_plugin/browser_plugin_popup_menu_helper_mac.h"
#endif

namespace content {

// static
BrowserPluginHostFactory* BrowserPluginGuest::factory_ = NULL;

// Parent class for the various types of permission requests, each of which
// should be able to handle the response to their permission request.
class BrowserPluginGuest::PermissionRequest {
 public:
  virtual void Respond(bool should_allow) = 0;
  virtual ~PermissionRequest() {}
 protected:
  PermissionRequest() {}
};

class BrowserPluginGuest::DownloadRequest : public PermissionRequest {
 public:
  explicit DownloadRequest(base::Callback<void(bool)> callback)
      : callback_(callback) {}
  virtual void Respond(bool should_allow) OVERRIDE {
    callback_.Run(should_allow);
  }
  virtual ~DownloadRequest() {}
 private:
  base::Callback<void(bool)> callback_;
};

class BrowserPluginGuest::GeolocationRequest : public PermissionRequest {
 public:
  GeolocationRequest(GeolocationCallback callback,
                     int bridge_id,
                     BrowserPluginGuest* guest,
                     base::WeakPtrFactory<BrowserPluginGuest>* weak_ptr_factory)
                     : callback_(callback),
                       bridge_id_(bridge_id),
                       guest_(guest),
                       weak_ptr_factory_(weak_ptr_factory) {}

  virtual void Respond(bool should_allow) OVERRIDE {
    WebContents* web_contents = guest_->embedder_web_contents();
    if (should_allow && web_contents) {
      // If renderer side embedder decides to allow gelocation, we need to check
      // if the app/embedder itself has geolocation access.
      BrowserContext* browser_context = web_contents->GetBrowserContext();
      if (browser_context) {
        GeolocationPermissionContext* geolocation_context =
            browser_context->GetGeolocationPermissionContext();
        if (geolocation_context) {
          base::Callback<void(bool)> geolocation_callback = base::Bind(
              &BrowserPluginGuest::SetGeolocationPermission,
              weak_ptr_factory_->GetWeakPtr(),
              callback_,
              bridge_id_);
          geolocation_context->RequestGeolocationPermission(
              web_contents->GetRenderProcessHost()->GetID(),
              web_contents->GetRoutingID(),
              // The geolocation permission request here is not initiated
              // through WebGeolocationPermissionRequest. We are only interested
              // in the fact whether the embedder/app has geolocation
              // permission. Therefore we use an invalid |bridge_id|.
              -1 /* bridge_id */,
              web_contents->GetURL(),
              geolocation_callback);
          return;
        }
      }
    }
    guest_->SetGeolocationPermission(callback_, bridge_id_, false);
  }
  virtual ~GeolocationRequest() {}
 private:
  base::Callback<void(bool)> callback_;
  int bridge_id_;
  BrowserPluginGuest* guest_;
  base::WeakPtrFactory<BrowserPluginGuest>* weak_ptr_factory_;
};

class BrowserPluginGuest::MediaRequest : public PermissionRequest {
 public:
  MediaRequest(const MediaStreamRequest& request,
               const MediaResponseCallback& callback,
               BrowserPluginGuest* guest)
               : request_(request),
                 callback_(callback),
                 guest_(guest) {}

  virtual void Respond(bool should_allow) OVERRIDE {
    WebContentsImpl* web_contents = guest_->embedder_web_contents();
    if (should_allow && web_contents) {
      // Re-route the request to the embedder's WebContents; the guest gets the
      // permission this way.
      web_contents->RequestMediaAccessPermission(request_, callback_);
    } else {
      // Deny the request.
      callback_.Run(MediaStreamDevices(), scoped_ptr<MediaStreamUI>());
    }

  }
  virtual ~MediaRequest() {}
 private:
  MediaStreamRequest request_;
  MediaResponseCallback callback_;
  BrowserPluginGuest* guest_;
};

class BrowserPluginGuest::NewWindowRequest : public PermissionRequest {
 public:
  NewWindowRequest(int instance_id, BrowserPluginGuest* guest)
      : instance_id_(instance_id),
        guest_(guest) {}

  virtual void Respond(bool should_allow) OVERRIDE {
    int embedder_render_process_id =
        guest_->embedder_web_contents()->GetRenderProcessHost()->GetID();
    BrowserPluginGuest* guest =
        guest_->GetWebContents()->GetBrowserPluginGuestManager()->
            GetGuestByInstanceID(instance_id_, embedder_render_process_id);
    if (!guest) {
      LOG(INFO) << "Guest not found. Instance ID: " << instance_id_;
      return;
    }

    // If we do not destroy the guest then we allow the new window.
    if (!should_allow)
      guest->Destroy();
  }
  virtual ~NewWindowRequest() {}
 private:
  int instance_id_;
  BrowserPluginGuest* guest_;
};

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

// Called on IO thread.
static std::string RetrieveDownloadURLFromRequestId(
    RenderViewHost* render_view_host,
    int url_request_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  int render_process_id = render_view_host->GetProcess()->GetID();
  GlobalRequestID global_id(render_process_id, url_request_id);
  net::URLRequest* url_request =
      ResourceDispatcherHostImpl::Get()->GetURLRequest(global_id);
  if (url_request)
    return url_request->url().possibly_invalid_spec();
  return std::string();
}

}  // namespace

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
    browser_plugin_guest_->embedder_web_contents_ = NULL;
    browser_plugin_guest_->Destroy();
  }

 private:
  BrowserPluginGuest* browser_plugin_guest_;

  DISALLOW_COPY_AND_ASSIGN(EmbedderRenderViewHostObserver);
};

BrowserPluginGuest::BrowserPluginGuest(
    int instance_id,
    WebContentsImpl* web_contents,
    BrowserPluginGuest* opener,
    bool has_render_view)
    : WebContentsObserver(web_contents),
      weak_ptr_factory_(this),
      embedder_web_contents_(NULL),
      instance_id_(instance_id),
      damage_buffer_sequence_id_(0),
      damage_buffer_size_(0),
      damage_buffer_scale_factor_(1.0f),
      guest_device_scale_factor_(1.0f),
      guest_hang_timeout_(
          base::TimeDelta::FromMilliseconds(kHungRendererDelayMs)),
      focused_(false),
      mouse_locked_(false),
      pending_lock_request_(false),
      embedder_visible_(true),
      next_permission_request_id_(0),
      has_render_view_(has_render_view) {
  DCHECK(web_contents);
  web_contents->SetDelegate(this);
  if (opener)
    opener_ = opener->AsWeakPtr();
  GetWebContents()->GetBrowserPluginGuestManager()->AddGuest(instance_id_,
                                                             GetWebContents());
}

bool BrowserPluginGuest::AddMessageToConsole(WebContents* source,
                                             int32 level,
                                             const string16& message,
                                             int32 line_no,
                                             const string16& source_id) {
  base::DictionaryValue message_info;
  // Log levels are from base/logging.h: LogSeverity.
  message_info.Set(browser_plugin::kLevel,
                   base::Value::CreateIntegerValue(level));
  message_info.Set(browser_plugin::kMessage,
                   base::Value::CreateStringValue(message));
  message_info.Set(browser_plugin::kLine,
                   base::Value::CreateIntegerValue(line_no));
  message_info.Set(browser_plugin::kSourceId,
                   base::Value::CreateStringValue(source_id));
  SendMessageToEmbedder(
      new BrowserPluginMsg_AddMessageToConsole(instance_id_, message_info));
  return false;
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
    IPC_MESSAGE_HANDLER(BrowserPluginHostMsg_CompositorFrameACK,
                        OnCompositorFrameACK)
    IPC_MESSAGE_HANDLER(BrowserPluginHostMsg_DragStatusUpdate,
                        OnDragStatusUpdate)
    IPC_MESSAGE_HANDLER(BrowserPluginHostMsg_ExecuteEditCommand,
                        OnExecuteEditCommand)
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
    const BrowserPluginHostMsg_Attach_Params& params) {
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

  has_render_view_ = true;

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
  return new BrowserPluginGuest(instance_id, web_contents, NULL, false);
}

// static
BrowserPluginGuest* BrowserPluginGuest::CreateWithOpener(
    int instance_id,
    WebContentsImpl* web_contents,
    BrowserPluginGuest* opener,
    bool has_render_view) {
  return new BrowserPluginGuest(instance_id,
                                web_contents,
                                opener,
                                has_render_view);
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
  if (was_blocked)
    *was_blocked = false;
  RequestNewWindowPermission(static_cast<WebContentsImpl*>(new_contents),
                             disposition, initial_pos, user_gesture);
}

void BrowserPluginGuest::CanDownload(
    RenderViewHost* render_view_host,
    int request_id,
    const std::string& request_method,
    const base::Callback<void(bool)>& callback) {
  if (permission_request_map_.size() >= kNumMaxOutstandingPermissionRequests) {
    // Deny the download request.
    callback.Run(false);
    return;
  }

  int permission_request_id = next_permission_request_id_++;
  permission_request_map_[permission_request_id] =
      new DownloadRequest(callback);

  BrowserThread::PostTaskAndReplyWithResult(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&RetrieveDownloadURLFromRequestId,
                 render_view_host, request_id),
      base::Bind(&BrowserPluginGuest::DidRetrieveDownloadURLFromRequestId,
                 weak_ptr_factory_.GetWeakPtr(),
                 request_method,
                 permission_request_id));
}

void BrowserPluginGuest::CloseContents(WebContents* source) {
  SendMessageToEmbedder(new BrowserPluginMsg_Close(instance_id_));
}

bool BrowserPluginGuest::HandleContextMenu(
    const ContextMenuParams& params) {
  // TODO(fsamuel): We have a do nothing context menu handler for now until
  // we implement the Apps Context Menu API for Browser Plugin (see
  // http://crbug.com/140315).
  return true;
}

void BrowserPluginGuest::HandleKeyboardEvent(
    WebContents* source,
    const NativeWebKeyboardEvent& event) {
  if (!attached())
    return;

  // Send the unhandled keyboard events back to the embedder to reprocess them.
  // TODO(fsamuel): This introduces the possibility of out-of-order keyboard
  // events because the guest may be arbitrarily delayed when responding to
  // keyboard events. In that time, the embedder may have received and processed
  // additional key events. This needs to be fixed as soon as possible.
  // See http://crbug.com/229882.
  embedder_web_contents_->GetDelegate()->HandleKeyboardEvent(
      web_contents(), event);
}

WebContents* BrowserPluginGuest::OpenURLFromTab(WebContents* source,
                                                const OpenURLParams& params) {
  // If the guest wishes to navigate away prior to attachment then we save the
  // navigation to perform upon attachment. Navigation initializes a lot of
  // state that assumes an embedder exists, such as RenderWidgetHostViewGuest.
  // Navigation also resumes resource loading which we don't want to allow
  // until attachment.
  if (!attached()) {
    PendingWindowMap::iterator it = opener()->pending_new_windows_.find(this);
    if (it == opener()->pending_new_windows_.end())
      return NULL;
    const TargetURL& old_target_url = it->second;
    TargetURL new_target_url(params.url);
    new_target_url.changed = new_target_url.url != old_target_url.url;
    it->second = new_target_url;
    return NULL;
  }
  // This can happen for cross-site redirects.
  source->GetController().LoadURL(
        params.url, params.referrer, params.transition, std::string());
  return source;
}

void BrowserPluginGuest::WebContentsCreated(WebContents* source_contents,
                                            int64 source_frame_id,
                                            const string16& frame_name,
                                            const GURL& target_url,
                                            WebContents* new_contents) {
  WebContentsImpl* new_contents_impl =
      static_cast<WebContentsImpl*>(new_contents);
  BrowserPluginGuest* guest = new_contents_impl->GetBrowserPluginGuest();
  guest->opener_ = AsWeakPtr();
  guest->name_ = UTF16ToUTF8(frame_name);
  // Take ownership of the new guest until it is attached to the embedder's DOM
  // tree to avoid leaking a guest if this guest is destroyed before attaching
  // the new guest.
  pending_new_windows_.insert(std::make_pair(guest, TargetURL(target_url)));
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
  const TargetURL& target_url = it->second;
  base::DictionaryValue request_info;
  request_info.Set(browser_plugin::kInitialHeight,
                   base::Value::CreateIntegerValue(initial_bounds.height()));
  request_info.Set(browser_plugin::kInitialWidth,
                   base::Value::CreateIntegerValue(initial_bounds.width()));
  request_info.Set(browser_plugin::kTargetURL,
                   base::Value::CreateStringValue(target_url.url.spec()));
  request_info.Set(browser_plugin::kWindowID,
                   base::Value::CreateIntegerValue(guest->instance_id()));
  request_info.Set(browser_plugin::kWindowOpenDisposition,
                   base::Value::CreateStringValue(
                       WindowOpenDispositionToString(disposition)));
  int request_id = next_permission_request_id_++;
  permission_request_map_[request_id] =
      new NewWindowRequest(guest->instance_id(), this);
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

void BrowserPluginGuest::DragSourceEndedAt(int client_x, int client_y,
    int screen_x, int screen_y, WebKit::WebDragOperation operation) {
  web_contents()->GetRenderViewHost()->DragSourceEndedAt(client_x, client_y,
      screen_x, screen_y, operation);
}

void BrowserPluginGuest::DragSourceMovedTo(int client_x, int client_y,
                                           int screen_x, int screen_y) {
  web_contents()->GetRenderViewHost()->DragSourceMovedTo(client_x, client_y,
                                                         screen_x, screen_y);
}

void BrowserPluginGuest::EndSystemDrag() {
  RenderViewHostImpl* guest_rvh = static_cast<RenderViewHostImpl*>(
      GetWebContents()->GetRenderViewHost());
  guest_rvh->DragSourceSystemDragEnded();
  // Issue a MouseUp event to get out of a selection state.
  WebKit::WebMouseEvent mouse_event;
  mouse_event.type = WebKit::WebInputEvent::MouseUp;
  mouse_event.button = WebKit::WebMouseEvent::ButtonLeft;
  guest_rvh->ForwardMouseEvent(mouse_event);
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
  if (permission_request_map_.size() >= kNumMaxOutstandingPermissionRequests) {
    // Deny the geolocation request.
    callback.Run(false);
    return;
  }
  int request_id = next_permission_request_id_++;
  permission_request_map_[request_id] = new GeolocationRequest(
      callback, bridge_id, this, &weak_ptr_factory_);
  DCHECK(bridge_id_to_request_id_map_.find(bridge_id) ==
         bridge_id_to_request_id_map_.end());
  bridge_id_to_request_id_map_[bridge_id] = request_id;

  base::DictionaryValue request_info;
  request_info.Set(browser_plugin::kURL,
                   base::Value::CreateStringValue(requesting_frame.spec()));

  SendMessageToEmbedder(
      new BrowserPluginMsg_RequestPermission(instance_id(),
          BrowserPluginPermissionTypeGeolocation, request_id, request_info));
}

void BrowserPluginGuest::CancelGeolocationRequest(int bridge_id) {
  std::map<int, int>::iterator bridge_itr =
      bridge_id_to_request_id_map_.find(bridge_id);
  if (bridge_itr == bridge_id_to_request_id_map_.end())
    return;

  int request_id = bridge_itr->second;
  bridge_id_to_request_id_map_.erase(bridge_itr);
  RequestMap::iterator request_itr = permission_request_map_.find(request_id);
  if (request_itr == permission_request_map_.end())
    return;
  delete request_itr->second;
  permission_request_map_.erase(request_itr);
}

void BrowserPluginGuest::SetGeolocationPermission(GeolocationCallback callback,
                                                  int bridge_id,
                                                  bool allowed) {
  callback.Run(allowed);
  CancelGeolocationRequest(bridge_id);
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
  bool disable_dragdrop = true;
#if defined(OS_LINUX) || defined(OS_MACOSX)
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableBrowserPluginDragDrop))
    disable_dragdrop = false;
#endif  // defined(OS_LINUX) || defined(OS_MACOSX)
  if (disable_dragdrop) {
    // Initiating a drag from inside a guest is currently not supported without
    // the kEnableBrowserPluginDragDrop flag on a linux platform. So inject some
    // JS to disable it. http://crbug.com/161112
    const char script[] = "window.addEventListener('dragstart', function() { "
                          "  window.event.preventDefault(); "
                          "});";
    render_view_host->ExecuteJavascriptInWebFrame(string16(),
                                                  ASCIIToUTF16(script));
  }
  SendMessageToEmbedder(new BrowserPluginMsg_LoadStop(instance_id()));
}

void BrowserPluginGuest::RenderViewReady() {
  // TODO(fsamuel): Investigate whether it's possible to update state earlier
  // here (see http://crbug.com/158151).
  Send(new InputMsg_SetFocus(routing_id(), focused_));
  UpdateVisibility();
  RenderViewHost* rvh = GetWebContents()->GetRenderViewHost();
  if (auto_size_enabled_)
    rvh->EnableAutoResize(min_auto_size_, max_auto_size_);
  else
    rvh->DisableAutoResize(damage_view_size_);

  Send(new ViewMsg_SetName(routing_id(), name_));

  RenderWidgetHostImpl::From(rvh)->
      set_hung_renderer_delay_ms(guest_hang_timeout_);
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
    case BrowserPluginHostMsg_CompositorFrameACK::ID:
    case BrowserPluginHostMsg_DragStatusUpdate::ID:
    case BrowserPluginHostMsg_ExecuteEditCommand::ID:
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
    IPC_MESSAGE_HANDLER(ViewHostMsg_ShowWidget, OnShowWidget)
    IPC_MESSAGE_HANDLER(ViewHostMsg_TakeFocus, OnTakeFocus)
    IPC_MESSAGE_HANDLER(ViewHostMsg_UnlockMouse, OnUnlockMouse)
    IPC_MESSAGE_HANDLER(ViewHostMsg_UpdateFrameName, OnUpdateFrameName)
    IPC_MESSAGE_HANDLER(ViewHostMsg_UpdateRect, OnUpdateRect)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void BrowserPluginGuest::Attach(
    WebContentsImpl* embedder_web_contents,
    BrowserPluginHostMsg_Attach_Params params) {
  if (attached())
    return;

  // Clear parameters that get inherited from the opener.
  params.storage_partition_id.clear();
  params.persist_storage = false;
  params.src.clear();

  // If a RenderView has already been created for this new window, then we need
  // to initialize the browser-side state now so that the RenderViewHostManager
  // does not create a new RenderView on navigation.
  if (has_render_view_) {
    static_cast<RenderViewHostImpl*>(
        GetWebContents()->GetRenderViewHost())->Init();
    WebContentsViewGuest* new_view =
        static_cast<WebContentsViewGuest*>(GetWebContents()->GetView());
    new_view->CreateViewForWidget(web_contents()->GetRenderViewHost());
  }

  // We need to do a navigation here if the target URL has changed between
  // the time the WebContents was created and the time it was attached.
  // We also need to do an initial navigation if a RenderView was never
  // created for the new window in cases where there is no referrer.
  PendingWindowMap::iterator it = opener()->pending_new_windows_.find(this);
  if (it != opener()->pending_new_windows_.end()) {
    const TargetURL& target_url = it->second;
    if (target_url.changed || !has_render_view_)
      params.src = it->second.url.spec();
  } else {
    NOTREACHED();
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

  // Inform the embedder of the guest's information.
  // We pull the partition information from the site's URL, which is of the form
  // guest://site/{persist}?{partition_name}.
  const GURL& site_url = GetWebContents()->GetSiteInstance()->GetSiteURL();
  BrowserPluginMsg_Attach_ACK_Params ack_params;
  ack_params.storage_partition_id = site_url.query();
  ack_params.persist_storage =
      site_url.path().find("persist") != std::string::npos;
  ack_params.name = name_;
  SendMessageToEmbedder(
      new BrowserPluginMsg_Attach_ACK(instance_id_, ack_params));
}

void BrowserPluginGuest::OnCompositorFrameACK(
    int instance_id,
    int route_id,
    int renderer_host_id,
    const cc::CompositorFrameAck& ack) {
  RenderWidgetHostImpl::SendSwapCompositorFrameAck(route_id,
                                                   renderer_host_id,
                                                   ack);
}

void BrowserPluginGuest::OnDragStatusUpdate(int instance_id,
                                            WebKit::WebDragStatus drag_status,
                                            const WebDropData& drop_data,
                                            WebKit::WebDragOperationsMask mask,
                                            const gfx::Point& location) {
  RenderViewHost* host = GetWebContents()->GetRenderViewHost();
  switch (drag_status) {
    case WebKit::WebDragStatusEnter:
      embedder_web_contents_->GetBrowserPluginEmbedder()->DragEnteredGuest(
          this);
      host->DragTargetDragEnter(drop_data, location, location, mask, 0);
      break;
    case WebKit::WebDragStatusOver:
      host->DragTargetDragOver(location, location, mask, 0);
      break;
    case WebKit::WebDragStatusLeave:
      embedder_web_contents_->GetBrowserPluginEmbedder()->DragLeftGuest(this);
      host->DragTargetDragLeave();
      break;
    case WebKit::WebDragStatusDrop:
      host->DragTargetDrop(location, location, 0);
      EndSystemDrag();
      break;
    case WebKit::WebDragStatusUnknown:
      NOTREACHED();
  }
}

void BrowserPluginGuest::OnExecuteEditCommand(int instance_id,
                                              const std::string& name) {
  Send(new InputMsg_ExecuteEditCommand(routing_id(), name, std::string()));
}

void BrowserPluginGuest::OnGo(int instance_id, int relative_index) {
  GetWebContents()->GetController().GoToOffset(relative_index);
}

void BrowserPluginGuest::OnHandleInputEvent(
    int instance_id,
    const gfx::Rect& guest_window_rect,
    const WebKit::WebInputEvent* event) {
  guest_window_rect_ = guest_window_rect;
  // If the embedder's RWHV is destroyed then that means that the embedder's
  // window has been closed but the embedder's WebContents has not yet been
  // destroyed. Computing screen coordinates of a BrowserPlugin only makes sense
  // if there is a visible embedder.
  if (embedder_web_contents_->GetRenderWidgetHostView()) {
    guest_screen_rect_ = guest_window_rect;
    guest_screen_rect_.Offset(
        embedder_web_contents_->GetRenderWidgetHostView()->
            GetViewBounds().OffsetFromOrigin());
  }
  RenderViewHostImpl* guest_rvh = static_cast<RenderViewHostImpl*>(
      GetWebContents()->GetRenderViewHost());

  if (WebKit::WebInputEvent::isMouseEventType(event->type)) {
    guest_rvh->ForwardMouseEvent(
        *static_cast<const WebKit::WebMouseEvent*>(event));
    return;
  }

  if (event->type == WebKit::WebInputEvent::MouseWheel) {
    guest_rvh->ForwardWheelEvent(
        *static_cast<const WebKit::WebMouseWheelEvent*>(event));
    return;
  }

  if (WebKit::WebInputEvent::isKeyboardEventType(event->type)) {
    RenderViewHostImpl* embedder_rvh = static_cast<RenderViewHostImpl*>(
        embedder_web_contents_->GetRenderViewHost());
    if (!embedder_rvh->GetLastKeyboardEvent())
      return;
    NativeWebKeyboardEvent keyboard_event(
        *embedder_rvh->GetLastKeyboardEvent());
    guest_rvh->ForwardKeyboardEvent(keyboard_event);
    return;
  }

  if (WebKit::WebInputEvent::isTouchEventType(event->type)) {
    guest_rvh->ForwardTouchEvent(
        *static_cast<const WebKit::WebTouchEvent*>(event));
    return;
  }

  if (WebKit::WebInputEvent::isGestureEventType(event->type)) {
    guest_rvh->ForwardGestureEvent(
        *static_cast<const WebKit::WebGestureEvent*>(event));
    return;
  }
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

    if (guest_device_scale_factor_ != params.scale_factor) {
      guest_device_scale_factor_ = params.scale_factor;
      render_widget_host->NotifyScreenInfoChanged();
    }
  }
  // Invalid damage buffer means we are in HW compositing mode,
  // so just resize the WebContents and repaint if needed.
  if (!base::SharedMemory::IsHandleValid(params.damage_buffer_handle)) {
    if (!params.view_size.IsEmpty())
      GetWebContents()->GetView()->SizeContents(params.view_size);
    if (params.repaint)
      Send(new ViewMsg_Repaint(routing_id(), params.view_size));
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
  Send(new InputMsg_SetFocus(routing_id(), focused));
  if (!focused && mouse_locked_)
    OnUnlockMouse();
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
  RequestMap::iterator request_itr = permission_request_map_.find(request_id);
  if (request_itr == permission_request_map_.end()) {
    LOG(INFO) << "Not a valid request ID.";
    return;
  }
  request_itr->second->Respond(should_allow);

  // Geolocation requests have to hang around for a while, so we don't delete
  // them here.
  if (permission_type != BrowserPluginPermissionTypeGeolocation) {
    delete request_itr->second;
    permission_request_map_.erase(request_itr);
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
  if (permission_request_map_.size() >= kNumMaxOutstandingPermissionRequests) {
    // Deny the media request.
    callback.Run(MediaStreamDevices(), scoped_ptr<MediaStreamUI>());
    return;
  }
  int request_id = next_permission_request_id_++;
  permission_request_map_[request_id] =
      new MediaRequest(request, callback, this);

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

void BrowserPluginGuest::DidRetrieveDownloadURLFromRequestId(
    const std::string& request_method,
    int permission_request_id,
    const std::string& url) {
  if (url.empty()) {
    OnRespondPermission(instance_id(), BrowserPluginPermissionTypeDownload,
                        permission_request_id, false);
    return;
  }

  base::DictionaryValue request_info;
  request_info.Set(browser_plugin::kRequestMethod,
                   base::Value::CreateStringValue(request_method));
  request_info.Set(browser_plugin::kURL, base::Value::CreateStringValue(url));

  SendMessageToEmbedder(
      new BrowserPluginMsg_RequestPermission(instance_id(),
          BrowserPluginPermissionTypeDownload, permission_request_id,
          request_info));
}

}  // namespace content
