// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/guest_view/web_view/web_view_permission_helper.h"

#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/geolocation/geolocation_permission_context.h"
#include "chrome/browser/geolocation/geolocation_permission_context_factory.h"
#include "chrome/browser/guest_view/web_view/web_view_constants.h"
#include "chrome/browser/guest_view/web_view/web_view_guest.h"
#include "chrome/browser/guest_view/web_view/web_view_permission_types.h"
#include "chrome/browser/plugins/chrome_plugin_service_filter.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/render_messages.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/user_metrics.h"

using content::BrowserPluginGuestDelegate;
using content::RenderViewHost;

namespace {
static std::string PermissionTypeToString(WebViewPermissionType type) {
  switch (type) {
    case WEB_VIEW_PERMISSION_TYPE_DOWNLOAD:
      return webview::kPermissionTypeDownload;
    case WEB_VIEW_PERMISSION_TYPE_FILESYSTEM:
      return webview::kPermissionTypeFileSystem;
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

// static
void RecordUserInitiatedUMA(
    const WebViewPermissionHelper::PermissionResponseInfo& info,
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
      case WEB_VIEW_PERMISSION_TYPE_FILESYSTEM:
        content::RecordAction(
            UserMetricsAction("WebView.PermissionAllow.FileSystem"));
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
      case WEB_VIEW_PERMISSION_TYPE_FILESYSTEM:
        content::RecordAction(
            UserMetricsAction("WebView.PermissionDeny.FileSystem"));
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
        break;
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

} // namespace

WebViewPermissionHelper::WebViewPermissionHelper(WebViewGuest* web_view_guest)
  : content::WebContentsObserver(web_view_guest->guest_web_contents()),
    next_permission_request_id_(guestview::kInstanceIDNone),
    web_view_guest_(web_view_guest),
    weak_factory_(this) {
}

WebViewPermissionHelper::~WebViewPermissionHelper() {
}

// static
WebViewPermissionHelper* WebViewPermissionHelper::FromFrameID(
    int render_process_id,
    int render_frame_id) {
  WebViewGuest* web_view_guest = WebViewGuest::FromFrameID(
      render_process_id, render_frame_id);
  if (!web_view_guest) {
    return NULL;
  }
  return web_view_guest->web_view_permission_helper_.get();
}

// static
WebViewPermissionHelper* WebViewPermissionHelper::FromWebContents(
      content::WebContents* web_contents) {
  WebViewGuest* web_view_guest = WebViewGuest::FromWebContents(web_contents);
  if (!web_view_guest)
      return NULL;
  return web_view_guest->web_view_permission_helper_.get();
}

#if defined(ENABLE_PLUGINS)
bool WebViewPermissionHelper::OnMessageReceived(
    const IPC::Message& message,
    content::RenderFrameHost* render_frame_host) {
  IPC_BEGIN_MESSAGE_MAP(WebViewPermissionHelper, message)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_BlockedOutdatedPlugin,
                        OnBlockedOutdatedPlugin)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_BlockedUnauthorizedPlugin,
                        OnBlockedUnauthorizedPlugin)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_NPAPINotSupported,
                        OnNPAPINotSupported)
#if defined(ENABLE_PLUGIN_INSTALLATION)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_FindMissingPlugin,
                        OnFindMissingPlugin)
#endif
    IPC_MESSAGE_UNHANDLED(return false)
  IPC_END_MESSAGE_MAP()

  return true;
}

bool WebViewPermissionHelper::OnMessageReceived(const IPC::Message& message) {
  IPC_BEGIN_MESSAGE_MAP(WebViewPermissionHelper, message)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_CouldNotLoadPlugin,
                        OnCouldNotLoadPlugin)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_OpenAboutPlugins,
                        OnOpenAboutPlugins)
#if defined(ENABLE_PLUGIN_INSTALLATION)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_RemovePluginPlaceholderHost,
                        OnRemovePluginPlaceholderHost)
#endif
    IPC_MESSAGE_UNHANDLED(return false)
  IPC_END_MESSAGE_MAP()

  return true;
}

void WebViewPermissionHelper::OnBlockedUnauthorizedPlugin(
    const base::string16& name,
    const std::string& identifier) {
  const char kPluginName[] = "name";
  const char kPluginIdentifier[] = "identifier";

  base::DictionaryValue info;
  info.SetString(std::string(kPluginName), name);
  info.SetString(std::string(kPluginIdentifier), identifier);
  RequestPermission(
      WEB_VIEW_PERMISSION_TYPE_LOAD_PLUGIN,
      info,
      base::Bind(&WebViewPermissionHelper::OnPermissionResponse,
                 weak_factory_.GetWeakPtr(),
                 identifier),
      true /* allowed_by_default */);
  content::RecordAction(
      base::UserMetricsAction("WebView.Guest.PluginLoadRequest"));
}

void WebViewPermissionHelper::OnCouldNotLoadPlugin(
    const base::FilePath& plugin_path) {
}

void WebViewPermissionHelper::OnBlockedOutdatedPlugin(
    int placeholder_id,
    const std::string& identifier) {
}

void WebViewPermissionHelper::OnNPAPINotSupported(const std::string& id) {
}

void WebViewPermissionHelper::OnOpenAboutPlugins() {
}

#if defined(ENABLE_PLUGIN_INSTALLATION)
void WebViewPermissionHelper::OnFindMissingPlugin(
    int placeholder_id,
    const std::string& mime_type) {
  Send(new ChromeViewMsg_DidNotFindMissingPlugin(placeholder_id));
}

void WebViewPermissionHelper::OnRemovePluginPlaceholderHost(
    int placeholder_id) {
}
#endif  // defined(ENABLE_PLUGIN_INSTALLATION)

void WebViewPermissionHelper::OnPermissionResponse(
    const std::string& identifier,
    bool allow,
    const std::string& input) {
  if (allow) {
    ChromePluginServiceFilter::GetInstance()->AuthorizeAllPlugins(
        web_contents(), true, identifier);
  }
}

#endif  // defined(ENABLE_PLUGINS)

void WebViewPermissionHelper::RequestMediaAccessPermission(
    content::WebContents* source,
    const content::MediaStreamRequest& request,
    const content::MediaResponseCallback& callback) {
  base::DictionaryValue request_info;
  request_info.SetString(guestview::kUrl, request.security_origin.spec());
  RequestPermission(WEB_VIEW_PERMISSION_TYPE_MEDIA,
                    request_info,
                    base::Bind(&WebViewPermissionHelper::
                               OnMediaPermissionResponse,
                               base::Unretained(this),
                               request,
                               callback),
                    false /* allowed_by_default */);
}

 void WebViewPermissionHelper::OnMediaPermissionResponse(
    const content::MediaStreamRequest& request,
    const content::MediaResponseCallback& callback,
    bool allow,
    const std::string& user_input) {
  if (!allow || !web_view_guest_->attached()) {
    // Deny the request.
    callback.Run(content::MediaStreamDevices(),
                 content::MEDIA_DEVICE_INVALID_STATE,
                 scoped_ptr<content::MediaStreamUI>());
    return;
  }
  if (!web_view_guest_->embedder_web_contents()->GetDelegate())
    return;

  web_view_guest_->embedder_web_contents()->GetDelegate()->
      RequestMediaAccessPermission(web_view_guest_->embedder_web_contents(),
                                   request,
                                   callback);
}

void WebViewPermissionHelper::CanDownload(
    content::RenderViewHost* render_view_host,
    const GURL& url,
    const std::string& request_method,
    const base::Callback<void(bool)>& callback) {
  base::DictionaryValue request_info;
  request_info.SetString(guestview::kUrl, url.spec());
  RequestPermission(
      WEB_VIEW_PERMISSION_TYPE_DOWNLOAD,
      request_info,
      base::Bind(&WebViewPermissionHelper::OnDownloadPermissionResponse,
                 base::Unretained(this),
                 callback),
      false /* allowed_by_default */);
}

void WebViewPermissionHelper::OnDownloadPermissionResponse(
    const base::Callback<void(bool)>& callback,
    bool allow,
    const std::string& user_input) {
  callback.Run(allow && web_view_guest_->attached());
}

void WebViewPermissionHelper::RequestPointerLockPermission(
    bool user_gesture,
    bool last_unlocked_by_target,
    const base::Callback<void(bool)>& callback) {
  base::DictionaryValue request_info;
  request_info.SetBoolean(guestview::kUserGesture, user_gesture);
  request_info.SetBoolean(webview::kLastUnlockedBySelf,
                          last_unlocked_by_target);
  request_info.SetString(guestview::kUrl,
                         web_contents()->GetLastCommittedURL().spec());

  RequestPermission(
      WEB_VIEW_PERMISSION_TYPE_POINTER_LOCK,
      request_info,
      base::Bind(
          &WebViewPermissionHelper::OnPointerLockPermissionResponse,
          base::Unretained(this),
          callback),
      false /* allowed_by_default */);
}

void WebViewPermissionHelper::OnPointerLockPermissionResponse(
    const base::Callback<void(bool)>& callback,
    bool allow,
    const std::string& user_input) {
  callback.Run(allow && web_view_guest_->attached());
}

void WebViewPermissionHelper::RequestGeolocationPermission(
    int bridge_id,
    const GURL& requesting_frame,
    bool user_gesture,
    const base::Callback<void(bool)>& callback) {
  base::DictionaryValue request_info;
  request_info.SetString(guestview::kUrl, requesting_frame.spec());
  request_info.SetBoolean(guestview::kUserGesture, user_gesture);

  // It is safe to hold an unretained pointer to WebViewPermissionHelper because
  // this callback is called from WebViewPermissionHelper::SetPermission.
  const PermissionResponseCallback permission_callback =
      base::Bind(
          &WebViewPermissionHelper::OnGeolocationPermissionResponse,
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

void WebViewPermissionHelper::OnGeolocationPermissionResponse(
    int bridge_id,
    bool user_gesture,
    const base::Callback<void(bool)>& callback,
    bool allow,
    const std::string& user_input) {
  // The <webview> embedder has allowed the permission. We now need to make sure
  // that the embedder has geolocation permission.
  RemoveBridgeID(bridge_id);

  if (!allow || !web_view_guest_->attached()) {
    callback.Run(false);
    return;
  }

  Profile* profile = Profile::FromBrowserContext(
      web_view_guest_->browser_context());
  GeolocationPermissionContextFactory::GetForProfile(profile)->
      RequestGeolocationPermission(
          web_view_guest_->embedder_web_contents(),
          // The geolocation permission request here is not initiated
          // through WebGeolocationPermissionRequest. We are only interested
          // in the fact whether the embedder/app has geolocation
          // permission. Therefore we use an invalid |bridge_id|.
          -1,
          web_view_guest_->embedder_web_contents()->GetLastCommittedURL(),
          user_gesture,
          callback,
          NULL);
}

void WebViewPermissionHelper::CancelGeolocationPermissionRequest(
    int bridge_id) {
  int request_id = RemoveBridgeID(bridge_id);
  RequestMap::iterator request_itr =
      pending_permission_requests_.find(request_id);

  if (request_itr == pending_permission_requests_.end())
    return;

  pending_permission_requests_.erase(request_itr);
}

int WebViewPermissionHelper::RemoveBridgeID(int bridge_id) {
  std::map<int, int>::iterator bridge_itr =
      bridge_id_to_request_id_map_.find(bridge_id);
  if (bridge_itr == bridge_id_to_request_id_map_.end())
    return webview::kInvalidPermissionRequestID;

  int request_id = bridge_itr->second;
  bridge_id_to_request_id_map_.erase(bridge_itr);
  return request_id;
}

void WebViewPermissionHelper::RequestFileSystemPermission(
    const GURL& url,
    bool allowed_by_default,
    const base::Callback<void(bool)>& callback) {
  base::DictionaryValue request_info;
  request_info.SetString(guestview::kUrl, url.spec());
  RequestPermission(
      WEB_VIEW_PERMISSION_TYPE_FILESYSTEM,
      request_info,
      base::Bind(
          &WebViewPermissionHelper::OnFileSystemPermissionResponse,
          base::Unretained(this),
          callback),
      allowed_by_default);
}

void WebViewPermissionHelper::OnFileSystemPermissionResponse(
    const base::Callback<void(bool)>& callback,
    bool allow,
    const std::string& user_input) {
  callback.Run(allow && web_view_guest_->attached());
}

void WebViewPermissionHelper::FileSystemAccessedAsync(int render_process_id,
                                                      int render_frame_id,
                                                      int request_id,
                                                      const GURL& url,
                                                      bool blocked_by_policy) {
  RequestFileSystemPermission(
      url,
      !blocked_by_policy,
      base::Bind(&WebViewPermissionHelper::FileSystemAccessedAsyncResponse,
                 base::Unretained(this),
                 render_process_id,
                 render_frame_id,
                 request_id,
                 url));
}

void WebViewPermissionHelper::FileSystemAccessedAsyncResponse(
    int render_process_id,
    int render_frame_id,
    int request_id,
    const GURL& url,
    bool allowed) {
  TabSpecificContentSettings::FileSystemAccessed(
      render_process_id, render_frame_id, url, !allowed);
  Send(new ChromeViewMsg_RequestFileSystemAccessAsyncResponse(
      render_frame_id, request_id, allowed));
}

void WebViewPermissionHelper::FileSystemAccessedSync(int render_process_id,
                                                     int render_frame_id,
                                                     const GURL& url,
                                                     bool blocked_by_policy,
                                                     IPC::Message* reply_msg) {
  RequestFileSystemPermission(
      url,
      !blocked_by_policy,
      base::Bind(&WebViewPermissionHelper::FileSystemAccessedSyncResponse,
                 base::Unretained(this),
                 render_process_id,
                 render_frame_id,
                 url,
                 reply_msg));
}

void WebViewPermissionHelper::FileSystemAccessedSyncResponse(
    int render_process_id,
    int render_frame_id,
    const GURL& url,
    IPC::Message* reply_msg,
    bool allowed) {
  TabSpecificContentSettings::FileSystemAccessed(
      render_process_id, render_frame_id, url, !allowed);
  ChromeViewHostMsg_RequestFileSystemAccessSync::WriteReplyParams(reply_msg,
                                                                  allowed);
  Send(reply_msg);
}

int WebViewPermissionHelper::RequestPermission(
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
      web_view_guest_->DispatchEventToEmbedder(
          new GuestViewBase::Event(webview::kEventNewWindow, args.Pass()));
      break;
    }
    case WEB_VIEW_PERMISSION_TYPE_JAVASCRIPT_DIALOG: {
      web_view_guest_->DispatchEventToEmbedder(
          new GuestViewBase::Event(webview::kEventDialog, args.Pass()));
      break;
    }
    default: {
      args->SetString(webview::kPermission,
                      PermissionTypeToString(permission_type));
      web_view_guest_->DispatchEventToEmbedder(new GuestViewBase::Event(
          webview::kEventPermissionRequest,
          args.Pass()));
      break;
    }
  }
  return request_id;
}

WebViewPermissionHelper::SetPermissionResult
WebViewPermissionHelper::SetPermission(
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

WebViewPermissionHelper::PermissionResponseInfo::PermissionResponseInfo()
    : permission_type(WEB_VIEW_PERMISSION_TYPE_UNKNOWN),
      allowed_by_default(false) {
}

WebViewPermissionHelper::PermissionResponseInfo::PermissionResponseInfo(
    const PermissionResponseCallback& callback,
    WebViewPermissionType permission_type,
    bool allowed_by_default)
    : callback(callback),
      permission_type(permission_type),
      allowed_by_default(allowed_by_default) {
}

WebViewPermissionHelper::PermissionResponseInfo::~PermissionResponseInfo() {
}
