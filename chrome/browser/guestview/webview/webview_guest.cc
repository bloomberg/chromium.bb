// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/guestview/webview/webview_guest.h"

#include "base/command_line.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/extensions/api/web_request/web_request_api.h"
#include "chrome/browser/extensions/extension_renderer_state.h"
#include "chrome/browser/extensions/extension_web_contents_observer.h"
#include "chrome/browser/extensions/script_executor.h"
#include "chrome/browser/favicon/favicon_tab_helper.h"
#include "chrome/browser/guestview/guestview_constants.h"
#include "chrome/browser/guestview/webview/webview_constants.h"
#include "chrome/browser/guestview/webview/webview_permission_types.h"
#include "chrome/common/chrome_version_info.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/resource_request_details.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/result_codes.h"
#include "extensions/common/constants.h"
#include "net/base/net_errors.h"

#if defined(ENABLE_PLUGINS)
#include "chrome/browser/guestview/webview/plugin_permission_helper.h"
#endif

using content::WebContents;

namespace {

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
      return "crashed";
    case base::TERMINATION_STATUS_MAX_ENUM:
      break;
  }
  NOTREACHED() << "Unknown Termination Status.";
  return "unknown";
}

static std::string PermissionTypeToString(BrowserPluginPermissionType type) {
  switch (type) {
    case BROWSER_PLUGIN_PERMISSION_TYPE_DOWNLOAD:
      return webview::kPermissionTypeDownload;
    case BROWSER_PLUGIN_PERMISSION_TYPE_GEOLOCATION:
      return webview::kPermissionTypeGeolocation;
    case BROWSER_PLUGIN_PERMISSION_TYPE_MEDIA:
      return webview::kPermissionTypeMedia;
    case BROWSER_PLUGIN_PERMISSION_TYPE_NEW_WINDOW:
      return webview::kPermissionTypeNewWindow;
    case BROWSER_PLUGIN_PERMISSION_TYPE_POINTER_LOCK:
      return webview::kPermissionTypePointerLock;
    case BROWSER_PLUGIN_PERMISSION_TYPE_JAVASCRIPT_DIALOG:
      return webview::kPermissionTypeDialog;
    case BROWSER_PLUGIN_PERMISSION_TYPE_UNKNOWN:
      NOTREACHED();
      break;
    default: {
      WebViewPermissionType webview = static_cast<WebViewPermissionType>(type);
      switch (webview) {
        case WEB_VIEW_PERMISSION_TYPE_LOAD_PLUGIN:
          return webview::kPermissionTypeLoadPlugin;
      }
      NOTREACHED();
    }
  }
  return std::string();
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
  extensions::ExtensionWebContentsObserver::CreateForWebContents(contents);
#if defined(ENABLE_PLUGINS)
  PluginPermissionHelper::CreateForWebContents(contents);
#endif
}

}  // namespace

WebViewGuest::WebViewGuest(WebContents* guest_web_contents,
                           const std::string& extension_id)
    : GuestView(guest_web_contents, extension_id),
      WebContentsObserver(guest_web_contents),
      script_executor_(new extensions::ScriptExecutor(guest_web_contents,
                                                      &script_observers_)),
      next_permission_request_id_(0),
      is_overriding_user_agent_(false) {
  notification_registrar_.Add(
      this, content::NOTIFICATION_LOAD_COMPLETED_MAIN_FRAME,
      content::Source<WebContents>(guest_web_contents));

  notification_registrar_.Add(
      this, content::NOTIFICATION_RESOURCE_RECEIVED_REDIRECT,
      content::Source<WebContents>(guest_web_contents));

  AttachWebViewHelpers(guest_web_contents);
}

// static
WebViewGuest* WebViewGuest::From(int embedder_process_id,
                                 int guest_instance_id) {
  GuestView* guest = GuestView::From(embedder_process_id, guest_instance_id);
  if (!guest)
    return NULL;
  return guest->AsWebView();
}

// static
WebViewGuest* WebViewGuest::FromWebContents(WebContents* contents) {
  GuestView* guest = GuestView::FromWebContents(contents);
  return guest ? guest->AsWebView() : NULL;
}

void WebViewGuest::Attach(WebContents* embedder_web_contents,
                          const base::DictionaryValue& args) {
  std::string user_agent_override;
  if (args.GetString(webview::kParameterUserAgentOverride,
                     &user_agent_override)) {
    SetUserAgentOverride(user_agent_override);
  } else {
    SetUserAgentOverride("");
  }

  GuestView::Attach(embedder_web_contents, args);

  AddWebViewToExtensionRendererState();
}

GuestView::Type WebViewGuest::GetViewType() const {
  return GuestView::WEBVIEW;
}

WebViewGuest* WebViewGuest::AsWebView() {
  return this;
}

AdViewGuest* WebViewGuest::AsAdView() {
  return NULL;
}

void WebViewGuest::AddMessageToConsole(int32 level,
                                       const string16& message,
                                       int32 line_no,
                                       const string16& source_id) {
  scoped_ptr<DictionaryValue> args(new DictionaryValue());
  // Log levels are from base/logging.h: LogSeverity.
  args->SetInteger(webview::kLevel, level);
  args->SetString(webview::kMessage, message);
  args->SetInteger(webview::kLine, line_no);
  args->SetString(webview::kSourceId, source_id);
  DispatchEvent(
      new GuestView::Event(webview::kEventConsoleMessage, args.Pass()));
}

void WebViewGuest::Close() {
  scoped_ptr<DictionaryValue> args(new DictionaryValue());
  DispatchEvent(new GuestView::Event(webview::kEventClose, args.Pass()));
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
          browser_context(), extension_id(),
          embedder_render_process_id(),
          view_instance_id()));
}

void WebViewGuest::GuestProcessGone(base::TerminationStatus status) {
  scoped_ptr<DictionaryValue> args(new DictionaryValue());
  args->SetInteger(webview::kProcessId,
                   web_contents()->GetRenderProcessHost()->GetID());
  args->SetString(webview::kReason, TerminationStatusToString(status));
  DispatchEvent(
      new GuestView::Event(webview::kEventExit, args.Pass()));
}

bool WebViewGuest::HandleKeyboardEvent(
    const content::NativeWebKeyboardEvent& event) {
  if (event.type != WebKit::WebInputEvent::RawKeyDown)
    return false;

#if defined(OS_MACOSX)
  if (event.modifiers != WebKit::WebInputEvent::MetaKey)
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

bool WebViewGuest::IsDragAndDropEnabled() {
#if defined(OS_CHROMEOS)
  return true;
#else
  chrome::VersionInfo::Channel channel = chrome::VersionInfo::GetChannel();
  if (channel != chrome::VersionInfo::CHANNEL_STABLE &&
      channel != chrome::VersionInfo::CHANNEL_BETA) {
    // Drag and drop is enabled in canary and dev channel.
    return true;
  }

  return CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableBrowserPluginDragDrop);
#endif
}

bool WebViewGuest::IsOverridingUserAgent() const {
  return is_overriding_user_agent_;
}

void WebViewGuest::LoadProgressed(double progress) {
  scoped_ptr<DictionaryValue> args(new DictionaryValue());
  args->SetString(guestview::kUrl, web_contents()->GetURL().spec());
  args->SetDouble(webview::kProgress, progress);
  DispatchEvent(new GuestView::Event(webview::kEventLoadProgress, args.Pass()));
}

void WebViewGuest::LoadAbort(bool is_top_level,
                             const GURL& url,
                             const std::string& error_type) {
  scoped_ptr<DictionaryValue> args(new DictionaryValue());
  args->SetBoolean(guestview::kIsTopLevel, is_top_level);
  args->SetString(guestview::kUrl, url.possibly_invalid_spec());
  args->SetString(guestview::kReason, error_type);
  DispatchEvent(new GuestView::Event(webview::kEventLoadAbort, args.Pass()));
}

// TODO(fsamuel): Find a reliable way to test the 'responsive' and
// 'unresponsive' events.
void WebViewGuest::RendererResponsive() {
  scoped_ptr<DictionaryValue> args(new DictionaryValue());
  args->SetInteger(webview::kProcessId,
      guest_web_contents()->GetRenderProcessHost()->GetID());
  DispatchEvent(new GuestView::Event(webview::kEventResponsive, args.Pass()));
}

void WebViewGuest::RendererUnresponsive() {
  scoped_ptr<DictionaryValue> args(new DictionaryValue());
  args->SetInteger(webview::kProcessId,
      guest_web_contents()->GetRenderProcessHost()->GetID());
  DispatchEvent(new GuestView::Event(webview::kEventUnresponsive, args.Pass()));
}

bool WebViewGuest::RequestPermission(
    BrowserPluginPermissionType permission_type,
    const base::DictionaryValue& request_info,
    const PermissionResponseCallback& callback,
    bool allowed_by_default) {
  // If there are too many pending permission requests then reject this request.
  if (pending_permission_requests_.size() >=
      webview::kMaxOutstandingPermissionRequests) {
    callback.Run(false, std::string());
    return true;
  }

  int request_id = next_permission_request_id_++;
  pending_permission_requests_[request_id] =
      PermissionResponseInfo(callback, allowed_by_default);
  scoped_ptr<base::DictionaryValue> args(request_info.DeepCopy());
  args->SetInteger(webview::kRequestId, request_id);
  switch (permission_type) {
    case BROWSER_PLUGIN_PERMISSION_TYPE_NEW_WINDOW: {
      DispatchEvent(new GuestView::Event(webview::kEventNewWindow,
                                         args.Pass()));
      break;
    }
    case BROWSER_PLUGIN_PERMISSION_TYPE_JAVASCRIPT_DIALOG: {
      chrome::VersionInfo::Channel channel = chrome::VersionInfo::GetChannel();
      if (channel > chrome::VersionInfo::CHANNEL_DEV) {
        // 'dialog' API is not available in stable/beta.
        callback.Run(false, std::string());
        return true;
      }
      DispatchEvent(new GuestView::Event(webview::kEventDialog,
                                         args.Pass()));
      break;
    }
    default: {
      args->SetString(webview::kPermission,
                      PermissionTypeToString(permission_type));
      DispatchEvent(new GuestView::Event(webview::kEventPermissionRequest,
                                         args.Pass()));
      break;
    }
  }
  return true;
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

void WebViewGuest::Go(int relative_index) {
  guest_web_contents()->GetController().GoToOffset(relative_index);
}

void WebViewGuest::Reload() {
  // TODO(fsamuel): Don't check for repost because we don't want to show
  // Chromium's repost warning. We might want to implement a separate API
  // for registering a callback if a repost is about to happen.
  guest_web_contents()->GetController().Reload(false);
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
  pending_permission_requests_.erase(request_itr);

  return allow ? SET_PERMISSION_ALLOWED : SET_PERMISSION_DENIED;
}

void WebViewGuest::SetUserAgentOverride(
    const std::string& user_agent_override) {
  is_overriding_user_agent_ = !user_agent_override.empty();
  if (is_overriding_user_agent_) {
    content::RecordAction(content::UserMetricsAction(
                              "WebView.Guest.OverrideUA"));
  }
  guest_web_contents()->SetUserAgentOverride(user_agent_override);
}

void WebViewGuest::Stop() {
  guest_web_contents()->Stop();
}

void WebViewGuest::Terminate() {
  content::RecordAction(content::UserMetricsAction("WebView.Guest.Terminate"));
  base::ProcessHandle process_handle =
      guest_web_contents()->GetRenderProcessHost()->GetHandle();
  if (process_handle)
    base::KillProcess(process_handle, content::RESULT_CODE_KILLED, false);
}

bool WebViewGuest::ClearData(const base::Time remove_since,
                             uint32 removal_mask,
                             const base::Closure& callback) {
  content::StoragePartition* partition =
      content::BrowserContext::GetStoragePartition(
          web_contents()->GetBrowserContext(),
          web_contents()->GetSiteInstance());

  if (!partition)
    return false;

  partition->ClearData(
      removal_mask,
      content::StoragePartition::QUOTA_MANAGED_STORAGE_MASK_ALL,
      NULL,
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
    const string16& frame_unique_name,
    bool is_main_frame,
    const GURL& url,
    content::PageTransition transition_type,
    content::RenderViewHost* render_view_host) {
  scoped_ptr<DictionaryValue> args(new DictionaryValue());
  args->SetString(guestview::kUrl, url.spec());
  args->SetBoolean(guestview::kIsTopLevel, is_main_frame);
  args->SetInteger(webview::kInternalCurrentEntryIndex,
      web_contents()->GetController().GetCurrentEntryIndex());
  args->SetInteger(webview::kInternalEntryCount,
      web_contents()->GetController().GetEntryCount());
  args->SetInteger(webview::kInternalProcessId,
      web_contents()->GetRenderProcessHost()->GetID());
  DispatchEvent(new GuestView::Event(webview::kEventLoadCommit, args.Pass()));
}

void WebViewGuest::DidFailProvisionalLoad(
    int64 frame_id,
    const string16& frame_unique_name,
    bool is_main_frame,
    const GURL& validated_url,
    int error_code,
    const string16& error_description,
    content::RenderViewHost* render_view_host) {
  // Translate the |error_code| into an error string.
  std::string error_type;
  RemoveChars(net::ErrorToString(error_code), "net::", &error_type);
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
  scoped_ptr<DictionaryValue> args(new DictionaryValue());
  args->SetString(guestview::kUrl, validated_url.spec());
  args->SetBoolean(guestview::kIsTopLevel, is_main_frame);
  DispatchEvent(new GuestView::Event(webview::kEventLoadStart, args.Pass()));
}

void WebViewGuest::DidStopLoading(content::RenderViewHost* render_view_host) {
  scoped_ptr<DictionaryValue> args(new DictionaryValue());
  DispatchEvent(new GuestView::Event(webview::kEventLoadStop, args.Pass()));
}

void WebViewGuest::WebContentsDestroyed(WebContents* web_contents) {
  RemoveWebViewFromExtensionRendererState(web_contents);
}

void WebViewGuest::LoadHandlerCalled() {
  scoped_ptr<DictionaryValue> args(new DictionaryValue());
  DispatchEvent(new GuestView::Event(webview::kEventContentLoad, args.Pass()));
}

void WebViewGuest::LoadRedirect(const GURL& old_url,
                                const GURL& new_url,
                                bool is_top_level) {
  scoped_ptr<DictionaryValue> args(new DictionaryValue());
  args->SetBoolean(guestview::kIsTopLevel, is_top_level);
  args->SetString(webview::kNewURL, new_url.spec());
  args->SetString(webview::kOldURL, old_url.spec());
  DispatchEvent(new GuestView::Event(webview::kEventLoadRedirect, args.Pass()));
}

// static
bool WebViewGuest::AllowChromeExtensionURLs() {
  chrome::VersionInfo::Channel channel = chrome::VersionInfo::GetChannel();
  return channel <= chrome::VersionInfo::CHANNEL_DEV;
}

void WebViewGuest::AddWebViewToExtensionRendererState() {
  const GURL& site_url = web_contents()->GetSiteInstance()->GetSiteURL();
  ExtensionRendererState::WebViewInfo webview_info;
  webview_info.embedder_process_id = embedder_render_process_id();
  webview_info.instance_id = view_instance_id();
  // TODO(fsamuel): Partition IDs should probably be a chrome-only concept. They
  // should probably be passed in via attach args.
  webview_info.partition_id =  site_url.query();
  webview_info.allow_chrome_extension_urls = AllowChromeExtensionURLs();

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

GURL WebViewGuest::ResolveURL(const std::string& src) {
  if (extension_id().empty()) {
    NOTREACHED();
    return GURL(src);
  }
  GURL default_url(base::StringPrintf("%s://%s/",
                                      extensions::kExtensionScheme,
                                      extension_id().c_str()));
  return default_url.Resolve(src);
}

void WebViewGuest::SizeChanged(const gfx::Size& old_size,
                               const gfx::Size& new_size) {
  scoped_ptr<DictionaryValue> args(new DictionaryValue());
  args->SetInteger(webview::kOldHeight, old_size.height());
  args->SetInteger(webview::kOldWidth, old_size.width());
  args->SetInteger(webview::kNewHeight, new_size.height());
  args->SetInteger(webview::kNewWidth, new_size.width());
  DispatchEvent(new GuestView::Event(webview::kEventSizeChanged, args.Pass()));
}

WebViewGuest::PermissionResponseInfo::PermissionResponseInfo()
    : allowed_by_default(false) {
}

WebViewGuest::PermissionResponseInfo::PermissionResponseInfo(
    const PermissionResponseCallback& callback,
    bool allowed_by_default)
    : callback(callback),
      allowed_by_default(allowed_by_default) {
}

WebViewGuest::PermissionResponseInfo::~PermissionResponseInfo() {
}
