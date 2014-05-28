// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/guest_view/web_view/web_view_guest.h"

#include "base/message_loop/message_loop.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/api/web_request/web_request_api.h"
#include "chrome/browser/extensions/api/webview/webview_api.h"
#include "chrome/browser/extensions/chrome_extension_web_contents_observer.h"
#include "chrome/browser/extensions/extension_renderer_state.h"
#include "chrome/browser/extensions/menu_manager.h"
#include "chrome/browser/extensions/script_executor.h"
#include "chrome/browser/favicon/favicon_tab_helper.h"
#include "chrome/browser/guest_view/guest_view_constants.h"
#include "chrome/browser/guest_view/guest_view_manager.h"
#include "chrome/browser/guest_view/web_view/web_view_constants.h"
#include "chrome/browser/guest_view/web_view/web_view_permission_types.h"
#include "chrome/browser/renderer_context_menu/context_menu_delegate.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/render_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/geolocation_permission_context.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/resource_request_details.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/media_stream_request.h"
#include "content/public/common/page_zoom.h"
#include "content/public/common/result_codes.h"
#include "content/public/common/stop_find_action.h"
#include "content/public/common/url_constants.h"
#include "extensions/common/constants.h"
#include "ipc/ipc_message_macros.h"
#include "net/base/net_errors.h"
#include "third_party/WebKit/public/web/WebFindOptions.h"
#include "ui/base/models/simple_menu_model.h"

#if defined(ENABLE_PRINTING)
#if defined(ENABLE_FULL_PRINTING)
#include "chrome/browser/printing/print_preview_message_handler.h"
#include "chrome/browser/printing/print_view_manager.h"
#else
#include "chrome/browser/printing/print_view_manager_basic.h"
#endif  // defined(ENABLE_FULL_PRINTING)
#endif  // defined(ENABLE_PRINTING)

#if defined(ENABLE_PLUGINS)
#include "chrome/browser/guest_view/web_view/plugin_permission_helper.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#endif

using base::UserMetricsAction;
using content::RenderFrameHost;
using content::WebContents;

namespace {

std::string WindowOpenDispositionToString(
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

static std::string TerminationStatusToString(base::TerminationStatus status) {
  switch (status) {
    case base::TERMINATION_STATUS_NORMAL_TERMINATION:
      return "normal";
    case base::TERMINATION_STATUS_ABNORMAL_TERMINATION:
    case base::TERMINATION_STATUS_STILL_RUNNING:
      return "abnormal";
    case base::TERMINATION_STATUS_PROCESS_WAS_KILLED:
      return "killed";
    case base::TERMINATION_STATUS_PROCESS_CRASHED:
#if defined(OS_ANDROID)
    case base::TERMINATION_STATUS_OOM_PROTECTED:
#endif
      return "crashed";
    case base::TERMINATION_STATUS_MAX_ENUM:
      break;
  }
  NOTREACHED() << "Unknown Termination Status.";
  return "unknown";
}

static std::string PermissionTypeToString(WebViewPermissionType type) {
  switch (type) {
    case WEB_VIEW_PERMISSION_TYPE_DOWNLOAD:
      return webview::kPermissionTypeDownload;
    case WEB_VIEW_PERMISSION_TYPE_GEOLOCATION:
      return webview::kPermissionTypeGeolocation;
    case WEB_VIEW_PERMISSION_TYPE_JAVASCRIPT_DIALOG:
      return webview::kPermissionTypeDialog;
    case WEB_VIEW_PERMISSION_TYPE_LOAD_PLUGIN:
      return webview::kPermissionTypeLoadPlugin;
    case WEB_VIEW_PERMISSION_TYPE_MEDIA:
      return webview::kPermissionTypeMedia;
    case WEB_VIEW_PERMISSION_TYPE_NEW_WINDOW:
      return webview::kPermissionTypeNewWindow;
    case WEB_VIEW_PERMISSION_TYPE_POINTER_LOCK:
      return webview::kPermissionTypePointerLock;
    default:
      NOTREACHED();
      return std::string();
  }
}

void RemoveWebViewEventListenersOnIOThread(
    void* profile,
    const std::string& extension_id,
    int embedder_process_id,
    int view_instance_id) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  ExtensionWebRequestEventRouter::GetInstance()->RemoveWebViewEventListeners(
      profile,
      extension_id,
      embedder_process_id,
      view_instance_id);
}

void AttachWebViewHelpers(WebContents* contents) {
  FaviconTabHelper::CreateForWebContents(contents);
  extensions::ChromeExtensionWebContentsObserver::CreateForWebContents(
      contents);
#if defined(ENABLE_PLUGINS)
  PluginPermissionHelper::CreateForWebContents(contents);
#endif
#if defined(ENABLE_PRINTING)
#if defined(ENABLE_FULL_PRINTING)
  printing::PrintViewManager::CreateForWebContents(contents);
  printing::PrintPreviewMessageHandler::CreateForWebContents(contents);
#else
  printing::PrintViewManagerBasic::CreateForWebContents(contents);
#endif  // defined(ENABLE_FULL_PRINTING)
#endif  // defined(ENABLE_PRINTING)
}

}  // namespace

WebViewGuest::WebViewGuest(int guest_instance_id,
                           WebContents* guest_web_contents,
                           const std::string& embedder_extension_id)
   :  GuestView<WebViewGuest>(guest_instance_id,
                              guest_web_contents,
                              embedder_extension_id),
      WebContentsObserver(guest_web_contents),
      script_executor_(new extensions::ScriptExecutor(guest_web_contents,
                                                      &script_observers_)),
      pending_context_menu_request_id_(0),
      next_permission_request_id_(0),
      is_overriding_user_agent_(false),
      pending_reload_on_attachment_(false),
      main_frame_id_(0),
      chromevox_injected_(false),
      find_helper_(this),
      javascript_dialog_helper_(this) {
  notification_registrar_.Add(
      this, content::NOTIFICATION_LOAD_COMPLETED_MAIN_FRAME,
      content::Source<WebContents>(guest_web_contents));

  notification_registrar_.Add(
      this, content::NOTIFICATION_RESOURCE_RECEIVED_REDIRECT,
      content::Source<WebContents>(guest_web_contents));

#if defined(OS_CHROMEOS)
  chromeos::AccessibilityManager* accessibility_manager =
      chromeos::AccessibilityManager::Get();
  CHECK(accessibility_manager);
  accessibility_subscription_ = accessibility_manager->RegisterCallback(
      base::Bind(&WebViewGuest::OnAccessibilityStatusChanged,
                 base::Unretained(this)));
#endif

  AttachWebViewHelpers(guest_web_contents);
}

// static
const char WebViewGuest::Type[] = "webview";

// static.
int WebViewGuest::GetViewInstanceId(WebContents* contents) {
  WebViewGuest* guest = FromWebContents(contents);
  if (!guest)
    return guestview::kInstanceIDNone;

  return guest->view_instance_id();
}

// static
void WebViewGuest::RecordUserInitiatedUMA(const PermissionResponseInfo& info,
                                          bool allow) {
  if (allow) {
    // Note that |allow| == true means the embedder explicitly allowed the
    // request. For some requests they might still fail. An example of such
    // scenario would be: an embedder allows geolocation request but doesn't
    // have geolocation access on its own.
    switch (info.permission_type) {
      case WEB_VIEW_PERMISSION_TYPE_DOWNLOAD:
        content::RecordAction(
            UserMetricsAction("WebView.PermissionAllow.Download"));
        break;
      case WEB_VIEW_PERMISSION_TYPE_GEOLOCATION:
        content::RecordAction(
            UserMetricsAction("WebView.PermissionAllow.Geolocation"));
        break;
      case WEB_VIEW_PERMISSION_TYPE_JAVASCRIPT_DIALOG:
        content::RecordAction(
            UserMetricsAction("WebView.PermissionAllow.JSDialog"));
        break;
      case WEB_VIEW_PERMISSION_TYPE_LOAD_PLUGIN:
        content::RecordAction(
            UserMetricsAction("WebView.Guest.PermissionAllow.PluginLoad"));
      case WEB_VIEW_PERMISSION_TYPE_MEDIA:
        content::RecordAction(
            UserMetricsAction("WebView.PermissionAllow.Media"));
        break;
      case WEB_VIEW_PERMISSION_TYPE_NEW_WINDOW:
        content::RecordAction(
            UserMetricsAction("BrowserPlugin.PermissionAllow.NewWindow"));
        break;
      case WEB_VIEW_PERMISSION_TYPE_POINTER_LOCK:
        content::RecordAction(
            UserMetricsAction("WebView.PermissionAllow.PointerLock"));
        break;
      default:
        break;
    }
  } else {
    switch (info.permission_type) {
      case WEB_VIEW_PERMISSION_TYPE_DOWNLOAD:
        content::RecordAction(
            UserMetricsAction("WebView.PermissionDeny.Download"));
        break;
      case WEB_VIEW_PERMISSION_TYPE_GEOLOCATION:
        content::RecordAction(
            UserMetricsAction("WebView.PermissionDeny.Geolocation"));
        break;
      case WEB_VIEW_PERMISSION_TYPE_JAVASCRIPT_DIALOG:
        content::RecordAction(
            UserMetricsAction("WebView.PermissionDeny.JSDialog"));
        break;
      case WEB_VIEW_PERMISSION_TYPE_LOAD_PLUGIN:
        content::RecordAction(
            UserMetricsAction("WebView.Guest.PermissionDeny.PluginLoad"));
      case WEB_VIEW_PERMISSION_TYPE_MEDIA:
        content::RecordAction(
            UserMetricsAction("WebView.PermissionDeny.Media"));
        break;
      case WEB_VIEW_PERMISSION_TYPE_NEW_WINDOW:
        content::RecordAction(
            UserMetricsAction("BrowserPlugin.PermissionDeny.NewWindow"));
        break;
      case WEB_VIEW_PERMISSION_TYPE_POINTER_LOCK:
        content::RecordAction(
            UserMetricsAction("WebView.PermissionDeny.PointerLock"));
        break;
      default:
        break;
    }
  }
}

// static
scoped_ptr<base::ListValue> WebViewGuest::MenuModelToValue(
    const ui::SimpleMenuModel& menu_model) {
  scoped_ptr<base::ListValue> items(new base::ListValue());
  for (int i = 0; i < menu_model.GetItemCount(); ++i) {
    base::DictionaryValue* item_value = new base::DictionaryValue();
    // TODO(lazyboy): We need to expose some kind of enum equivalent of
    // |command_id| instead of plain integers.
    item_value->SetInteger(webview::kMenuItemCommandId,
                           menu_model.GetCommandIdAt(i));
    item_value->SetString(webview::kMenuItemLabel, menu_model.GetLabelAt(i));
    items->Append(item_value);
  }
  return items.Pass();
}

void WebViewGuest::Attach(WebContents* embedder_web_contents,
                          const base::DictionaryValue& args) {
  std::string name;
  args.GetString(webview::kName, &name);
  // If the guest window's name is empty, then the WebView tag's name is
  // assigned. Otherwise, the guest window's name takes precedence over the
  // WebView tag's name.
  if (name_.empty())
    name_ = name;
  ReportFrameNameChange(name_);

  std::string user_agent_override;
  if (args.GetString(webview::kParameterUserAgentOverride,
                     &user_agent_override)) {
    SetUserAgentOverride(user_agent_override);
  } else {
    SetUserAgentOverride("");
  }

  GuestViewBase::Attach(embedder_web_contents, args);

  AddWebViewToExtensionRendererState();
}

bool WebViewGuest::HandleContextMenu(
    const content::ContextMenuParams& params) {
  ContextMenuDelegate* menu_delegate =
      ContextMenuDelegate::FromWebContents(guest_web_contents());
  DCHECK(menu_delegate);

  pending_menu_ = menu_delegate->BuildMenu(guest_web_contents(), params);

  // Pass it to embedder.
  int request_id = ++pending_context_menu_request_id_;
  scoped_ptr<base::DictionaryValue> args(new base::DictionaryValue());
  scoped_ptr<base::ListValue> items =
      MenuModelToValue(pending_menu_->menu_model());
  args->Set(webview::kContextMenuItems, items.release());
  args->SetInteger(webview::kRequestId, request_id);
  DispatchEvent(new GuestViewBase::Event(webview::kEventContextMenu,
                                         args.Pass()));
  return true;
}

bool WebViewGuest::AddMessageToConsole(WebContents* source,
                                       int32 level,
                                       const base::string16& message,
                                       int32 line_no,
                                       const base::string16& source_id) {
  scoped_ptr<base::DictionaryValue> args(new base::DictionaryValue());
  // Log levels are from base/logging.h: LogSeverity.
  args->SetInteger(webview::kLevel, level);
  args->SetString(webview::kMessage, message);
  args->SetInteger(webview::kLine, line_no);
  args->SetString(webview::kSourceId, source_id);
  DispatchEvent(
      new GuestViewBase::Event(webview::kEventConsoleMessage, args.Pass()));
  return true;
}

void WebViewGuest::CloseContents(WebContents* source) {
  scoped_ptr<base::DictionaryValue> args(new base::DictionaryValue());
  DispatchEvent(new GuestViewBase::Event(webview::kEventClose, args.Pass()));
}

void WebViewGuest::DidAttach() {
  if (GetOpener()) {
    // We need to do a navigation here if the target URL has changed between
    // the time the WebContents was created and the time it was attached.
    // We also need to do an initial navigation if a RenderView was never
    // created for the new window in cases where there is no referrer.
    PendingWindowMap::iterator it =
        GetOpener()->pending_new_windows_.find(this);
    if (it != GetOpener()->pending_new_windows_.end()) {
      const NewWindowInfo& new_window_info = it->second;
      NavigateGuest(new_window_info.url.spec());
    } else {
      NOTREACHED();
    }

    // Once a new guest is attached to the DOM of the embedder page, then the
    // lifetime of the new guest is no longer managed by the opener guest.
    GetOpener()->pending_new_windows_.erase(this);
  }

  if (pending_reload_on_attachment_) {
    pending_reload_on_attachment_ = false;
    guest_web_contents()->GetController().Reload(false);
  }
}

void WebViewGuest::EmbedderDestroyed() {
  // TODO(fsamuel): WebRequest event listeners for <webview> should survive
  // reparenting of a <webview> within a single embedder. Right now, we keep
  // around the browser state for the listener for the lifetime of the embedder.
  // Ideally, the lifetime of the listeners should match the lifetime of the
  // <webview> DOM node. Once http://crbug.com/156219 is resolved we can move
  // the call to RemoveWebViewEventListenersOnIOThread back to
  // WebViewGuest::WebContentsDestroyed.
  content::BrowserThread::PostTask(
      content::BrowserThread::IO,
      FROM_HERE,
      base::Bind(
          &RemoveWebViewEventListenersOnIOThread,
          browser_context(), embedder_extension_id(),
          embedder_render_process_id(),
          view_instance_id()));
}

void WebViewGuest::FindReply(WebContents* source,
                             int request_id,
                             int number_of_matches,
                             const gfx::Rect& selection_rect,
                             int active_match_ordinal,
                             bool final_update) {
  find_helper_.FindReply(request_id, number_of_matches, selection_rect,
                         active_match_ordinal, final_update);
}

void WebViewGuest::HandleKeyboardEvent(
    WebContents* source,
    const content::NativeWebKeyboardEvent& event) {
  if (!attached())
    return;

  if (HandleKeyboardShortcuts(event))
    return;

  // Send the unhandled keyboard events back to the embedder to reprocess them.
  // TODO(fsamuel): This introduces the possibility of out-of-order keyboard
  // events because the guest may be arbitrarily delayed when responding to
  // keyboard events. In that time, the embedder may have received and processed
  // additional key events. This needs to be fixed as soon as possible.
  // See http://crbug.com/229882.
  embedder_web_contents()->GetDelegate()->HandleKeyboardEvent(
      web_contents(), event);
}

bool WebViewGuest::IsDragAndDropEnabled() {
  return true;
}

void WebViewGuest::LoadProgressChanged(content::WebContents* source,
                                       double progress) {
  scoped_ptr<base::DictionaryValue> args(new base::DictionaryValue());
  args->SetString(guestview::kUrl, guest_web_contents()->GetURL().spec());
  args->SetDouble(webview::kProgress, progress);
  DispatchEvent(
      new GuestViewBase::Event(webview::kEventLoadProgress, args.Pass()));
}

void WebViewGuest::LoadAbort(bool is_top_level,
                             const GURL& url,
                             const std::string& error_type) {
  scoped_ptr<base::DictionaryValue> args(new base::DictionaryValue());
  args->SetBoolean(guestview::kIsTopLevel, is_top_level);
  args->SetString(guestview::kUrl, url.possibly_invalid_spec());
  args->SetString(guestview::kReason, error_type);
  DispatchEvent(
      new GuestViewBase::Event(webview::kEventLoadAbort, args.Pass()));
}

void WebViewGuest::OnUpdateFrameName(bool is_top_level,
                                     const std::string& name) {
  if (!is_top_level)
    return;

  if (name_ == name)
    return;

  ReportFrameNameChange(name);
}

WebViewGuest* WebViewGuest::CreateNewGuestWindow(
    const content::OpenURLParams& params) {
  GuestViewManager* guest_manager =
      GuestViewManager::FromBrowserContext(browser_context());
  // Allocate a new instance ID for the new guest.
  int instance_id = guest_manager->GetNextInstanceID();

  // Set the attach params to use the same partition as the opener.
  // We pull the partition information from the site's URL, which is of the
  // form guest://site/{persist}?{partition_name}.
  const GURL& site_url = guest_web_contents()->GetSiteInstance()->GetSiteURL();

  scoped_ptr<base::DictionaryValue> create_params(extra_params()->DeepCopy());
  const std::string& storage_partition_id = site_url.query();
  bool persist_storage =
      site_url.path().find("persist") != std::string::npos;
  WebContents* new_guest_web_contents =
      guest_manager->CreateGuest(guest_web_contents()->GetSiteInstance(),
                                 instance_id,
                                 storage_partition_id,
                                 persist_storage,
                                 create_params.Pass());
  WebViewGuest* new_guest =
      WebViewGuest::FromWebContents(new_guest_web_contents);
  new_guest->SetOpener(this);

  // Take ownership of |new_guest|.
  pending_new_windows_.insert(
      std::make_pair(new_guest, NewWindowInfo(params.url, std::string())));

  // Request permission to show the new window.
  RequestNewWindowPermission(params.disposition, gfx::Rect(),
                             params.user_gesture,
                             new_guest->guest_web_contents());

  return new_guest;
}

// TODO(fsamuel): Find a reliable way to test the 'responsive' and
// 'unresponsive' events.
void WebViewGuest::RendererResponsive(content::WebContents* source) {
  scoped_ptr<base::DictionaryValue> args(new base::DictionaryValue());
  args->SetInteger(webview::kProcessId,
      guest_web_contents()->GetRenderProcessHost()->GetID());
  DispatchEvent(
      new GuestViewBase::Event(webview::kEventResponsive, args.Pass()));
}

void WebViewGuest::RendererUnresponsive(content::WebContents* source) {
  scoped_ptr<base::DictionaryValue> args(new base::DictionaryValue());
  args->SetInteger(webview::kProcessId,
      guest_web_contents()->GetRenderProcessHost()->GetID());
  DispatchEvent(
      new GuestViewBase::Event(webview::kEventUnresponsive, args.Pass()));
}

void WebViewGuest::Observe(int type,
                           const content::NotificationSource& source,
                           const content::NotificationDetails& details) {
  switch (type) {
    case content::NOTIFICATION_LOAD_COMPLETED_MAIN_FRAME: {
      DCHECK_EQ(content::Source<WebContents>(source).ptr(),
                guest_web_contents());
      if (content::Source<WebContents>(source).ptr() == guest_web_contents())
        LoadHandlerCalled();
      break;
    }
    case content::NOTIFICATION_RESOURCE_RECEIVED_REDIRECT: {
      DCHECK_EQ(content::Source<WebContents>(source).ptr(),
                guest_web_contents());
      content::ResourceRedirectDetails* resource_redirect_details =
          content::Details<content::ResourceRedirectDetails>(details).ptr();
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

void WebViewGuest::SetZoom(double zoom_factor) {
  double zoom_level = content::ZoomFactorToZoomLevel(zoom_factor);
  guest_web_contents()->SetZoomLevel(zoom_level);

  scoped_ptr<base::DictionaryValue> args(new base::DictionaryValue());
  args->SetDouble(webview::kOldZoomFactor, current_zoom_factor_);
  args->SetDouble(webview::kNewZoomFactor, zoom_factor);
  DispatchEvent(
      new GuestViewBase::Event(webview::kEventZoomChange, args.Pass()));

  current_zoom_factor_ = zoom_factor;
}

double WebViewGuest::GetZoom() {
  return current_zoom_factor_;
}

void WebViewGuest::Find(
    const base::string16& search_text,
    const blink::WebFindOptions& options,
    scoped_refptr<extensions::WebviewFindFunction> find_function) {
  find_helper_.Find(guest_web_contents(), search_text, options, find_function);
}

void WebViewGuest::StopFinding(content::StopFindAction action) {
  find_helper_.CancelAllFindSessions();
  guest_web_contents()->StopFinding(action);
}

void WebViewGuest::Go(int relative_index) {
  guest_web_contents()->GetController().GoToOffset(relative_index);
}

void WebViewGuest::Reload() {
  // TODO(fsamuel): Don't check for repost because we don't want to show
  // Chromium's repost warning. We might want to implement a separate API
  // for registering a callback if a repost is about to happen.
  guest_web_contents()->GetController().Reload(false);
}

void WebViewGuest::RequestGeolocationPermission(
    int bridge_id,
    const GURL& requesting_frame,
    bool user_gesture,
    const base::Callback<void(bool)>& callback) {
  base::DictionaryValue request_info;
  request_info.Set(guestview::kUrl,
                   base::Value::CreateStringValue(requesting_frame.spec()));
  request_info.Set(guestview::kUserGesture,
                   base::Value::CreateBooleanValue(user_gesture));

  // It is safe to hold an unretained pointer to WebViewGuest because this
  // callback is called from WebViewGuest::SetPermission.
  const PermissionResponseCallback permission_callback =
      base::Bind(&WebViewGuest::OnWebViewGeolocationPermissionResponse,
                 base::Unretained(this),
                 bridge_id,
                 user_gesture,
                 callback);
  int request_id = RequestPermission(
      WEB_VIEW_PERMISSION_TYPE_GEOLOCATION,
      request_info,
      permission_callback,
      false /* allowed_by_default */);
  bridge_id_to_request_id_map_[bridge_id] = request_id;
}

void WebViewGuest::OnWebViewGeolocationPermissionResponse(
    int bridge_id,
    bool user_gesture,
    const base::Callback<void(bool)>& callback,
    bool allow,
    const std::string& user_input) {
  // The <webview> embedder has allowed the permission. We now need to make sure
  // that the embedder has geolocation permission.
  RemoveBridgeID(bridge_id);

  if (!allow || !attached()) {
    callback.Run(false);
    return;
  }

  content::GeolocationPermissionContext* geolocation_context =
      browser_context()->GetGeolocationPermissionContext();

  DCHECK(geolocation_context);
  geolocation_context->RequestGeolocationPermission(
      embedder_web_contents(),
      // The geolocation permission request here is not initiated
      // through WebGeolocationPermissionRequest. We are only interested
      // in the fact whether the embedder/app has geolocation
      // permission. Therefore we use an invalid |bridge_id|.
      -1 /* bridge_id */,
      embedder_web_contents()->GetLastCommittedURL(),
      user_gesture,
      callback);
}

void WebViewGuest::CancelGeolocationPermissionRequest(int bridge_id) {
  int request_id = RemoveBridgeID(bridge_id);
  RequestMap::iterator request_itr =
      pending_permission_requests_.find(request_id);

  if (request_itr == pending_permission_requests_.end())
    return;

  pending_permission_requests_.erase(request_itr);
}

void WebViewGuest::OnWebViewMediaPermissionResponse(
    const content::MediaStreamRequest& request,
    const content::MediaResponseCallback& callback,
    bool allow,
    const std::string& user_input) {
  if (!allow || !attached()) {
    // Deny the request.
    callback.Run(content::MediaStreamDevices(),
                 content::MEDIA_DEVICE_INVALID_STATE,
                 scoped_ptr<content::MediaStreamUI>());
    return;
  }
  if (!embedder_web_contents()->GetDelegate())
    return;

  embedder_web_contents()->GetDelegate()->
      RequestMediaAccessPermission(embedder_web_contents(), request, callback);
}

void WebViewGuest::OnWebViewDownloadPermissionResponse(
    const base::Callback<void(bool)>& callback,
    bool allow,
    const std::string& user_input) {
  callback.Run(allow && attached());
}

void WebViewGuest::OnWebViewPointerLockPermissionResponse(
    const base::Callback<void(bool)>& callback,
    bool allow,
    const std::string& user_input) {
  callback.Run(allow && attached());
}

WebViewGuest::SetPermissionResult WebViewGuest::SetPermission(
    int request_id,
    PermissionResponseAction action,
    const std::string& user_input) {
  RequestMap::iterator request_itr =
      pending_permission_requests_.find(request_id);

  if (request_itr == pending_permission_requests_.end())
    return SET_PERMISSION_INVALID;

  const PermissionResponseInfo& info = request_itr->second;
  bool allow = (action == ALLOW) ||
      ((action == DEFAULT) && info.allowed_by_default);

  info.callback.Run(allow, user_input);

  // Only record user initiated (i.e. non-default) actions.
  if (action != DEFAULT)
    RecordUserInitiatedUMA(info, allow);

  pending_permission_requests_.erase(request_itr);

  return allow ? SET_PERMISSION_ALLOWED : SET_PERMISSION_DENIED;
}

void WebViewGuest::SetUserAgentOverride(
    const std::string& user_agent_override) {
  is_overriding_user_agent_ = !user_agent_override.empty();
  if (is_overriding_user_agent_) {
    content::RecordAction(UserMetricsAction("WebView.Guest.OverrideUA"));
  }
  guest_web_contents()->SetUserAgentOverride(user_agent_override);
}

void WebViewGuest::Stop() {
  guest_web_contents()->Stop();
}

void WebViewGuest::Terminate() {
  content::RecordAction(UserMetricsAction("WebView.Guest.Terminate"));
  base::ProcessHandle process_handle =
      guest_web_contents()->GetRenderProcessHost()->GetHandle();
  if (process_handle)
    base::KillProcess(process_handle, content::RESULT_CODE_KILLED, false);
}

bool WebViewGuest::ClearData(const base::Time remove_since,
                             uint32 removal_mask,
                             const base::Closure& callback) {
  content::RecordAction(UserMetricsAction("WebView.Guest.ClearData"));
  content::StoragePartition* partition =
      content::BrowserContext::GetStoragePartition(
          guest_web_contents()->GetBrowserContext(),
          guest_web_contents()->GetSiteInstance());

  if (!partition)
    return false;

  partition->ClearData(
      removal_mask,
      content::StoragePartition::QUOTA_MANAGED_STORAGE_MASK_ALL,
      GURL(),
      content::StoragePartition::OriginMatcherFunction(),
      remove_since,
      base::Time::Now(),
      callback);
  return true;
}

WebViewGuest::~WebViewGuest() {
}

void WebViewGuest::DidCommitProvisionalLoadForFrame(
    int64 frame_id,
    const base::string16& frame_unique_name,
    bool is_main_frame,
    const GURL& url,
    content::PageTransition transition_type,
    content::RenderViewHost* render_view_host) {
  find_helper_.CancelAllFindSessions();

  scoped_ptr<base::DictionaryValue> args(new base::DictionaryValue());
  args->SetString(guestview::kUrl, url.spec());
  args->SetBoolean(guestview::kIsTopLevel, is_main_frame);
  args->SetInteger(webview::kInternalCurrentEntryIndex,
      guest_web_contents()->GetController().GetCurrentEntryIndex());
  args->SetInteger(webview::kInternalEntryCount,
      guest_web_contents()->GetController().GetEntryCount());
  args->SetInteger(webview::kInternalProcessId,
      guest_web_contents()->GetRenderProcessHost()->GetID());
  DispatchEvent(
      new GuestViewBase::Event(webview::kEventLoadCommit, args.Pass()));

  // Update the current zoom factor for the new page.
  current_zoom_factor_ = content::ZoomLevelToZoomFactor(
      guest_web_contents()->GetZoomLevel());

  if (is_main_frame) {
    chromevox_injected_ = false;
    main_frame_id_ = frame_id;
  }
}

void WebViewGuest::DidFailProvisionalLoad(
    int64 frame_id,
    const base::string16& frame_unique_name,
    bool is_main_frame,
    const GURL& validated_url,
    int error_code,
    const base::string16& error_description,
    content::RenderViewHost* render_view_host) {
  // Translate the |error_code| into an error string.
  std::string error_type;
  base::RemoveChars(net::ErrorToString(error_code), "net::", &error_type);
  LoadAbort(is_main_frame, validated_url, error_type);
}

void WebViewGuest::DidStartProvisionalLoadForFrame(
    int64 frame_id,
    int64 parent_frame_id,
    bool is_main_frame,
    const GURL& validated_url,
    bool is_error_page,
    bool is_iframe_srcdoc,
    content::RenderViewHost* render_view_host) {
  scoped_ptr<base::DictionaryValue> args(new base::DictionaryValue());
  args->SetString(guestview::kUrl, validated_url.spec());
  args->SetBoolean(guestview::kIsTopLevel, is_main_frame);
  DispatchEvent(
      new GuestViewBase::Event(webview::kEventLoadStart, args.Pass()));
}

void WebViewGuest::DocumentLoadedInFrame(
    int64 frame_id,
    content::RenderViewHost* render_view_host) {
  if (frame_id == main_frame_id_)
    InjectChromeVoxIfNeeded(render_view_host);
}

void WebViewGuest::DidStopLoading(content::RenderViewHost* render_view_host) {
  scoped_ptr<base::DictionaryValue> args(new base::DictionaryValue());
  DispatchEvent(new GuestViewBase::Event(webview::kEventLoadStop, args.Pass()));
}

bool WebViewGuest::OnMessageReceived(const IPC::Message& message,
                                     RenderFrameHost* render_frame_host) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(WebViewGuest, message)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_UpdateFrameName, OnUpdateFrameName)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void WebViewGuest::RenderProcessGone(base::TerminationStatus status) {
  // Cancel all find sessions in progress.
  find_helper_.CancelAllFindSessions();

  scoped_ptr<base::DictionaryValue> args(new base::DictionaryValue());
  args->SetInteger(webview::kProcessId,
                   guest_web_contents()->GetRenderProcessHost()->GetID());
  args->SetString(webview::kReason, TerminationStatusToString(status));
  DispatchEvent(new GuestViewBase::Event(webview::kEventExit, args.Pass()));
}

void WebViewGuest::WebContentsDestroyed() {
  // Clean up custom context menu items for this guest.
  extensions::MenuManager* menu_manager = extensions::MenuManager::Get(
      Profile::FromBrowserContext(browser_context()));
  menu_manager->RemoveAllContextItems(extensions::MenuItem::ExtensionKey(
      embedder_extension_id(), view_instance_id()));

  RemoveWebViewFromExtensionRendererState(web_contents());
}

void WebViewGuest::UserAgentOverrideSet(const std::string& user_agent) {
  content::NavigationController& controller =
      guest_web_contents()->GetController();
  content::NavigationEntry* entry = controller.GetVisibleEntry();
  if (!entry)
    return;
  entry->SetIsOverridingUserAgent(!user_agent.empty());
  if (!attached()) {
    // We cannot reload now because all resource loads are suspended until
    // attachment.
    pending_reload_on_attachment_ = true;
    return;
  }
  guest_web_contents()->GetController().Reload(false);
}

void WebViewGuest::RenderViewReady() {
  Send(new ChromeViewMsg_SetName(guest_web_contents()->GetRoutingID(), name_));
}

void WebViewGuest::ReportFrameNameChange(const std::string& name) {
  name_ = name;
  scoped_ptr<base::DictionaryValue> args(new base::DictionaryValue());
  args->SetString(webview::kName, name);
  DispatchEvent(
      new GuestViewBase::Event(webview::kEventFrameNameChanged, args.Pass()));
}

void WebViewGuest::LoadHandlerCalled() {
  scoped_ptr<base::DictionaryValue> args(new base::DictionaryValue());
  DispatchEvent(
      new GuestViewBase::Event(webview::kEventContentLoad, args.Pass()));
}

void WebViewGuest::LoadRedirect(const GURL& old_url,
                                const GURL& new_url,
                                bool is_top_level) {
  scoped_ptr<base::DictionaryValue> args(new base::DictionaryValue());
  args->SetBoolean(guestview::kIsTopLevel, is_top_level);
  args->SetString(webview::kNewURL, new_url.spec());
  args->SetString(webview::kOldURL, old_url.spec());
  DispatchEvent(
      new GuestViewBase::Event(webview::kEventLoadRedirect, args.Pass()));
}

void WebViewGuest::AddWebViewToExtensionRendererState() {
  const GURL& site_url = guest_web_contents()->GetSiteInstance()->GetSiteURL();
  std::string partition_domain;
  std::string partition_id;
  bool in_memory;
  if (!GetGuestPartitionConfigForSite(
          site_url, &partition_domain, &partition_id, &in_memory)) {
    NOTREACHED();
    return;
  }
  DCHECK(embedder_extension_id() == partition_domain);

  ExtensionRendererState::WebViewInfo webview_info;
  webview_info.embedder_process_id = embedder_render_process_id();
  webview_info.instance_id = view_instance_id();
  webview_info.partition_id =  partition_id;
  webview_info.embedder_extension_id = embedder_extension_id();

  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::Bind(
          &ExtensionRendererState::AddWebView,
          base::Unretained(ExtensionRendererState::GetInstance()),
          guest_web_contents()->GetRenderProcessHost()->GetID(),
          guest_web_contents()->GetRoutingID(),
          webview_info));
}

// static
void WebViewGuest::RemoveWebViewFromExtensionRendererState(
    WebContents* web_contents) {
  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::Bind(
          &ExtensionRendererState::RemoveWebView,
          base::Unretained(ExtensionRendererState::GetInstance()),
          web_contents->GetRenderProcessHost()->GetID(),
          web_contents->GetRoutingID()));
}

void WebViewGuest::SizeChanged(const gfx::Size& old_size,
                               const gfx::Size& new_size) {
  scoped_ptr<base::DictionaryValue> args(new base::DictionaryValue());
  args->SetInteger(webview::kOldHeight, old_size.height());
  args->SetInteger(webview::kOldWidth, old_size.width());
  args->SetInteger(webview::kNewHeight, new_size.height());
  args->SetInteger(webview::kNewWidth, new_size.width());
  DispatchEvent(
      new GuestViewBase::Event(webview::kEventSizeChanged, args.Pass()));
}

void WebViewGuest::RequestMediaAccessPermission(
    content::WebContents* source,
    const content::MediaStreamRequest& request,
    const content::MediaResponseCallback& callback) {
  base::DictionaryValue request_info;
  request_info.Set(
      guestview::kUrl,
      base::Value::CreateStringValue(request.security_origin.spec()));
  RequestPermission(WEB_VIEW_PERMISSION_TYPE_MEDIA,
                    request_info,
                    base::Bind(&WebViewGuest::OnWebViewMediaPermissionResponse,
                               base::Unretained(this),
                               request,
                               callback),
                    false /* allowed_by_default */);
}

void WebViewGuest::CanDownload(
    content::RenderViewHost* render_view_host,
    const GURL& url,
    const std::string& request_method,
    const base::Callback<void(bool)>& callback) {
  base::DictionaryValue request_info;
  request_info.Set(
      guestview::kUrl,
      base::Value::CreateStringValue(url.spec()));
  RequestPermission(
      WEB_VIEW_PERMISSION_TYPE_DOWNLOAD,
      request_info,
      base::Bind(&WebViewGuest::OnWebViewDownloadPermissionResponse,
                 base::Unretained(this),
                 callback),
      false /* allowed_by_default */);
}

void WebViewGuest::RequestPointerLockPermission(
    bool user_gesture,
    bool last_unlocked_by_target,
    const base::Callback<void(bool)>& callback) {
  base::DictionaryValue request_info;
  request_info.Set(guestview::kUserGesture,
                   base::Value::CreateBooleanValue(user_gesture));
  request_info.Set(webview::kLastUnlockedBySelf,
                   base::Value::CreateBooleanValue(last_unlocked_by_target));
  request_info.Set(guestview::kUrl,
                   base::Value::CreateStringValue(
                       guest_web_contents()->GetLastCommittedURL().spec()));

  RequestPermission(
      WEB_VIEW_PERMISSION_TYPE_POINTER_LOCK,
      request_info,
      base::Bind(&WebViewGuest::OnWebViewPointerLockPermissionResponse,
                 base::Unretained(this),
                 callback),
      false /* allowed_by_default */);
}

content::JavaScriptDialogManager*
    WebViewGuest::GetJavaScriptDialogManager() {
  return &javascript_dialog_helper_;
}

content::ColorChooser* WebViewGuest::OpenColorChooser(
    WebContents* web_contents,
    SkColor color,
    const std::vector<content::ColorSuggestion>& suggestions) {
  if (!attached() || !embedder_web_contents()->GetDelegate())
    return NULL;
  return embedder_web_contents()->GetDelegate()->OpenColorChooser(
      web_contents, color, suggestions);
}

void WebViewGuest::RunFileChooser(WebContents* web_contents,
                                  const content::FileChooserParams& params) {
  if (!attached() || !embedder_web_contents()->GetDelegate())
    return;

  embedder_web_contents()->GetDelegate()->RunFileChooser(web_contents, params);
}

void WebViewGuest::NavigateGuest(const std::string& src) {
  GURL url = ResolveURL(src);

  // Do not allow navigating a guest to schemes other than known safe schemes.
  // This will block the embedder trying to load unwanted schemes, e.g.
  // chrome://settings.
  bool scheme_is_blocked =
      (!content::ChildProcessSecurityPolicy::GetInstance()->IsWebSafeScheme(
          url.scheme()) &&
      !url.SchemeIs(content::kAboutScheme)) ||
      url.SchemeIs(content::kJavaScriptScheme);
  if (scheme_is_blocked || !url.is_valid()) {
    std::string error_type;
    base::RemoveChars(net::ErrorToString(net::ERR_ABORTED), "net::",
                      &error_type);
    LoadAbort(true /* is_top_level */, url, error_type);
    return;
  }

  GURL validated_url(url);
  guest_web_contents()->GetRenderProcessHost()->
      FilterURL(false, &validated_url);
  // As guests do not swap processes on navigation, only navigations to
  // normal web URLs are supported.  No protocol handlers are installed for
  // other schemes (e.g., WebUI or extensions), and no permissions or bindings
  // can be granted to the guest process.
  LoadURLWithParams(validated_url,
                    content::Referrer(),
                    content::PAGE_TRANSITION_AUTO_TOPLEVEL,
                    guest_web_contents());
}

#if defined(OS_CHROMEOS)
void WebViewGuest::OnAccessibilityStatusChanged(
    const chromeos::AccessibilityStatusEventDetails& details) {
  if (details.notification_type == chromeos::ACCESSIBILITY_MANAGER_SHUTDOWN) {
    accessibility_subscription_.reset();
  } else if (details.notification_type ==
      chromeos::ACCESSIBILITY_TOGGLE_SPOKEN_FEEDBACK) {
    if (details.enabled)
      InjectChromeVoxIfNeeded(guest_web_contents()->GetRenderViewHost());
    else
      chromevox_injected_ = false;
  }
}
#endif

void WebViewGuest::InjectChromeVoxIfNeeded(
    content::RenderViewHost* render_view_host) {
#if defined(OS_CHROMEOS)
  if (!chromevox_injected_) {
    chromeos::AccessibilityManager* manager =
        chromeos::AccessibilityManager::Get();
    if (manager && manager->IsSpokenFeedbackEnabled()) {
      manager->InjectChromeVox(render_view_host);
      chromevox_injected_ = true;
    }
  }
#endif
}

int WebViewGuest::RemoveBridgeID(int bridge_id) {
  std::map<int, int>::iterator bridge_itr =
      bridge_id_to_request_id_map_.find(bridge_id);
  if (bridge_itr == bridge_id_to_request_id_map_.end())
    return webview::kInvalidPermissionRequestID;

  int request_id = bridge_itr->second;
  bridge_id_to_request_id_map_.erase(bridge_itr);
  return request_id;
}

int WebViewGuest::RequestPermission(
    WebViewPermissionType permission_type,
    const base::DictionaryValue& request_info,
    const PermissionResponseCallback& callback,
    bool allowed_by_default) {
  // If there are too many pending permission requests then reject this request.
  if (pending_permission_requests_.size() >=
      webview::kMaxOutstandingPermissionRequests) {
    // Let the stack unwind before we deny the permission request so that
    // objects held by the permission request are not destroyed immediately
    // after creation. This is to allow those same objects to be accessed again
    // in the same scope without fear of use after freeing.
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&PermissionResponseCallback::Run,
                  base::Owned(new PermissionResponseCallback(callback)),
                  allowed_by_default,
                  std::string()));
    return webview::kInvalidPermissionRequestID;
  }

  int request_id = next_permission_request_id_++;
  pending_permission_requests_[request_id] =
      PermissionResponseInfo(callback, permission_type, allowed_by_default);
  scoped_ptr<base::DictionaryValue> args(request_info.DeepCopy());
  args->SetInteger(webview::kRequestId, request_id);
  switch (permission_type) {
    case WEB_VIEW_PERMISSION_TYPE_NEW_WINDOW: {
      DispatchEvent(
          new GuestViewBase::Event(webview::kEventNewWindow, args.Pass()));
      break;
    }
    case WEB_VIEW_PERMISSION_TYPE_JAVASCRIPT_DIALOG: {
      DispatchEvent(
          new GuestViewBase::Event(webview::kEventDialog, args.Pass()));
      break;
    }
    default: {
      args->SetString(webview::kPermission,
                      PermissionTypeToString(permission_type));
      DispatchEvent(new GuestViewBase::Event(webview::kEventPermissionRequest,
                                             args.Pass()));
      break;
    }
  }
  return request_id;
}

bool WebViewGuest::HandleKeyboardShortcuts(
    const content::NativeWebKeyboardEvent& event) {
  if (event.type != blink::WebInputEvent::RawKeyDown)
    return false;

  // If the user hits the escape key without any modifiers then unlock the
  // mouse if necessary.
  if ((event.windowsKeyCode == ui::VKEY_ESCAPE) &&
      !(event.modifiers & blink::WebInputEvent::InputModifiers)) {
    return guest_web_contents()->GotResponseToLockMouseRequest(false);
  }

#if defined(OS_MACOSX)
  if (event.modifiers != blink::WebInputEvent::MetaKey)
    return false;

  if (event.windowsKeyCode == ui::VKEY_OEM_4) {
    Go(-1);
    return true;
  }

  if (event.windowsKeyCode == ui::VKEY_OEM_6) {
    Go(1);
    return true;
  }
#else
  if (event.windowsKeyCode == ui::VKEY_BROWSER_BACK) {
    Go(-1);
    return true;
  }

  if (event.windowsKeyCode == ui::VKEY_BROWSER_FORWARD) {
    Go(1);
    return true;
  }
#endif

  return false;
}

WebViewGuest::PermissionResponseInfo::PermissionResponseInfo()
    : permission_type(WEB_VIEW_PERMISSION_TYPE_UNKNOWN),
      allowed_by_default(false) {
}

WebViewGuest::PermissionResponseInfo::PermissionResponseInfo(
    const PermissionResponseCallback& callback,
    WebViewPermissionType permission_type,
    bool allowed_by_default)
    : callback(callback),
      permission_type(permission_type),
      allowed_by_default(allowed_by_default) {
}

WebViewGuest::PermissionResponseInfo::~PermissionResponseInfo() {
}

void WebViewGuest::ShowContextMenu(int request_id,
                                   const MenuItemVector* items) {
  if (!pending_menu_.get())
    return;

  // Make sure this was the correct request.
  if (request_id != pending_context_menu_request_id_)
    return;

  // TODO(lazyboy): Implement.
  DCHECK(!items);

  ContextMenuDelegate* menu_delegate =
      ContextMenuDelegate::FromWebContents(guest_web_contents());
  menu_delegate->ShowMenu(pending_menu_.Pass());
}

void WebViewGuest::SetName(const std::string& name) {
  if (name_ == name)
    return;
  name_ = name;

  Send(new ChromeViewMsg_SetName(routing_id(), name_));
}

void WebViewGuest::Destroy() {
  if (!attached() && GetOpener())
    GetOpener()->pending_new_windows_.erase(this);
  DestroyUnattachedWindows();
  GuestViewBase::Destroy();
}

void WebViewGuest::AddNewContents(content::WebContents* source,
                                  content::WebContents* new_contents,
                                  WindowOpenDisposition disposition,
                                  const gfx::Rect& initial_pos,
                                  bool user_gesture,
                                  bool* was_blocked) {
  if (was_blocked)
    *was_blocked = false;
  RequestNewWindowPermission(disposition,
                             initial_pos,
                             user_gesture,
                             new_contents);
}

content::WebContents* WebViewGuest::OpenURLFromTab(
    content::WebContents* source,
    const content::OpenURLParams& params) {
  // If the guest wishes to navigate away prior to attachment then we save the
  // navigation to perform upon attachment. Navigation initializes a lot of
  // state that assumes an embedder exists, such as RenderWidgetHostViewGuest.
  // Navigation also resumes resource loading which we don't want to allow
  // until attachment.
  if (!attached()) {
    WebViewGuest* opener = GetOpener();
    PendingWindowMap::iterator it =
        opener->pending_new_windows_.find(this);
    if (it == opener->pending_new_windows_.end())
      return NULL;
    const NewWindowInfo& old_target_url = it->second;
    NewWindowInfo new_window_info(params.url, old_target_url.name);
    it->second = new_window_info;
    return NULL;
  }
  if (params.disposition == CURRENT_TAB) {
    // This can happen for cross-site redirects.
    LoadURLWithParams(params.url, params.referrer, params.transition, source);
    return source;
  }

  return CreateNewGuestWindow(params)->guest_web_contents();
}

void WebViewGuest::WebContentsCreated(WebContents* source_contents,
                                      int opener_render_frame_id,
                                      const base::string16& frame_name,
                                      const GURL& target_url,
                                      content::WebContents* new_contents) {
  WebViewGuest* guest = WebViewGuest::FromWebContents(new_contents);
  CHECK(guest);
  guest->SetOpener(this);
  std::string guest_name = base::UTF16ToUTF8(frame_name);
  guest->name_ = guest_name;
  pending_new_windows_.insert(
      std::make_pair(guest, NewWindowInfo(target_url, guest_name)));
}

void WebViewGuest::LoadURLWithParams(const GURL& url,
                                     const content::Referrer& referrer,
                                     content::PageTransition transition_type,
                                     content::WebContents* web_contents) {
  content::NavigationController::LoadURLParams load_url_params(url);
  load_url_params.referrer = referrer;
  load_url_params.transition_type = transition_type;
  load_url_params.extra_headers = std::string();
  if (is_overriding_user_agent_) {
    load_url_params.override_user_agent =
        content::NavigationController::UA_OVERRIDE_TRUE;
  }
  web_contents->GetController().LoadURLWithParams(load_url_params);
}

void WebViewGuest::RequestNewWindowPermission(
    WindowOpenDisposition disposition,
    const gfx::Rect& initial_bounds,
    bool user_gesture,
    content::WebContents* new_contents) {
  WebViewGuest* guest = WebViewGuest::FromWebContents(new_contents);
  if (!guest)
    return;
  PendingWindowMap::iterator it = pending_new_windows_.find(guest);
  if (it == pending_new_windows_.end())
    return;
  const NewWindowInfo& new_window_info = it->second;

  base::DictionaryValue request_info;
  request_info.Set(webview::kInitialHeight,
                   base::Value::CreateIntegerValue(initial_bounds.height()));
  request_info.Set(webview::kInitialWidth,
                   base::Value::CreateIntegerValue(initial_bounds.width()));
  request_info.Set(webview::kTargetURL,
                   base::Value::CreateStringValue(new_window_info.url.spec()));
  request_info.Set(webview::kName,
                   base::Value::CreateStringValue(new_window_info.name));
  request_info.Set(webview::kWindowID,
                   base::Value::CreateIntegerValue(guest->guest_instance_id()));
  request_info.Set(webview::kWindowOpenDisposition,
                   base::Value::CreateStringValue(
                       WindowOpenDispositionToString(disposition)));

  RequestPermission(WEB_VIEW_PERMISSION_TYPE_NEW_WINDOW,
                    request_info,
                    base::Bind(&WebViewGuest::OnWebViewNewWindowResponse,
                               base::Unretained(this),
                               guest->guest_instance_id()),
                               false /* allowed_by_default */);
}

void WebViewGuest::DestroyUnattachedWindows() {
  // Destroy() reaches in and removes the WebViewGuest from its opener's
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
  DCHECK(pending_new_windows_.empty());
}

GURL WebViewGuest::ResolveURL(const std::string& src) {
  if (!in_extension()) {
    NOTREACHED();
    return GURL(src);
  }

  GURL default_url(base::StringPrintf("%s://%s/",
                                      extensions::kExtensionScheme,
                                      embedder_extension_id().c_str()));
  return default_url.Resolve(src);
}

void WebViewGuest::OnWebViewNewWindowResponse(
    int new_window_instance_id,
    bool allow,
    const std::string& user_input) {
  WebViewGuest* guest =
      WebViewGuest::From(embedder_render_process_id(), new_window_instance_id);
  if (!guest)
    return;

  if (!allow)
    guest->Destroy();
}
