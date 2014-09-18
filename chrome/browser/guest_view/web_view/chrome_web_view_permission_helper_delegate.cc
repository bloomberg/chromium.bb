// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/guest_view/web_view/chrome_web_view_permission_helper_delegate.h"

#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/geolocation/geolocation_permission_context.h"
#include "chrome/browser/geolocation/geolocation_permission_context_factory.h"
#include "chrome/browser/plugins/chrome_plugin_service_filter.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/render_messages.h"
#include "content/public/browser/user_metrics.h"
#include "extensions/browser/guest_view/web_view/web_view_constants.h"
#include "extensions/browser/guest_view/web_view/web_view_guest.h"

ChromeWebViewPermissionHelperDelegate::ChromeWebViewPermissionHelperDelegate(
    extensions::WebViewPermissionHelper* web_view_permission_helper)
    : WebViewPermissionHelperDelegate(web_view_permission_helper),
      weak_factory_(this) {
}

ChromeWebViewPermissionHelperDelegate::~ChromeWebViewPermissionHelperDelegate()
{}

#if defined(ENABLE_PLUGINS)
bool ChromeWebViewPermissionHelperDelegate::OnMessageReceived(
    const IPC::Message& message,
    content::RenderFrameHost* render_frame_host) {
  IPC_BEGIN_MESSAGE_MAP(ChromeWebViewPermissionHelperDelegate, message)
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

bool ChromeWebViewPermissionHelperDelegate::OnMessageReceived(
    const IPC::Message& message) {
  IPC_BEGIN_MESSAGE_MAP(ChromeWebViewPermissionHelperDelegate, message)
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

void ChromeWebViewPermissionHelperDelegate::OnBlockedUnauthorizedPlugin(
    const base::string16& name,
    const std::string& identifier) {
  const char kPluginName[] = "name";
  const char kPluginIdentifier[] = "identifier";

  base::DictionaryValue info;
  info.SetString(std::string(kPluginName), name);
  info.SetString(std::string(kPluginIdentifier), identifier);
  web_view_permission_helper()->RequestPermission(
      WEB_VIEW_PERMISSION_TYPE_LOAD_PLUGIN,
      info,
      base::Bind(&ChromeWebViewPermissionHelperDelegate::OnPermissionResponse,
                 weak_factory_.GetWeakPtr(),
                 identifier),
      true /* allowed_by_default */);
  content::RecordAction(
      base::UserMetricsAction("WebView.Guest.PluginLoadRequest"));
}

void ChromeWebViewPermissionHelperDelegate::OnCouldNotLoadPlugin(
    const base::FilePath& plugin_path) {
}

void ChromeWebViewPermissionHelperDelegate::OnBlockedOutdatedPlugin(
    int placeholder_id,
    const std::string& identifier) {
}

void ChromeWebViewPermissionHelperDelegate::OnNPAPINotSupported(
    const std::string& id) {
}

void ChromeWebViewPermissionHelperDelegate::OnOpenAboutPlugins() {
}

#if defined(ENABLE_PLUGIN_INSTALLATION)
void ChromeWebViewPermissionHelperDelegate::OnFindMissingPlugin(
    int placeholder_id,
    const std::string& mime_type) {
  Send(new ChromeViewMsg_DidNotFindMissingPlugin(placeholder_id));
}

void ChromeWebViewPermissionHelperDelegate::OnRemovePluginPlaceholderHost(
    int placeholder_id) {
}
#endif  // defined(ENABLE_PLUGIN_INSTALLATION)

void ChromeWebViewPermissionHelperDelegate::OnPermissionResponse(
    const std::string& identifier,
    bool allow,
    const std::string& input) {
  if (allow) {
    ChromePluginServiceFilter::GetInstance()->AuthorizeAllPlugins(
        web_contents(), true, identifier);
  }
}

#endif  // defined(ENABLE_PLUGINS)

void ChromeWebViewPermissionHelperDelegate::CanDownload(
    content::RenderViewHost* render_view_host,
    const GURL& url,
    const std::string& request_method,
    const base::Callback<void(bool)>& callback) {
  base::DictionaryValue request_info;
  request_info.SetString(guestview::kUrl, url.spec());
  web_view_permission_helper()->RequestPermission(
      WEB_VIEW_PERMISSION_TYPE_DOWNLOAD,
      request_info,
      base::Bind(
          &ChromeWebViewPermissionHelperDelegate::OnDownloadPermissionResponse,
          base::Unretained(this),
          callback),
      false /* allowed_by_default */);
}

void ChromeWebViewPermissionHelperDelegate::OnDownloadPermissionResponse(
    const base::Callback<void(bool)>& callback,
    bool allow,
    const std::string& user_input) {
  callback.Run(allow && web_view_guest()->attached());
}

void ChromeWebViewPermissionHelperDelegate::RequestPointerLockPermission(
    bool user_gesture,
    bool last_unlocked_by_target,
    const base::Callback<void(bool)>& callback) {
  base::DictionaryValue request_info;
  request_info.SetBoolean(guestview::kUserGesture, user_gesture);
  request_info.SetBoolean(webview::kLastUnlockedBySelf,
                          last_unlocked_by_target);
  request_info.SetString(guestview::kUrl,
                         web_contents()->GetLastCommittedURL().spec());

  web_view_permission_helper()->RequestPermission(
      WEB_VIEW_PERMISSION_TYPE_POINTER_LOCK,
      request_info,
      base::Bind(&ChromeWebViewPermissionHelperDelegate::
                     OnPointerLockPermissionResponse,
                 base::Unretained(this),
                 callback),
      false /* allowed_by_default */);
}

void ChromeWebViewPermissionHelperDelegate::OnPointerLockPermissionResponse(
    const base::Callback<void(bool)>& callback,
    bool allow,
    const std::string& user_input) {
  callback.Run(allow && web_view_guest()->attached());
}

void ChromeWebViewPermissionHelperDelegate::RequestGeolocationPermission(
    int bridge_id,
    const GURL& requesting_frame,
    bool user_gesture,
    const base::Callback<void(bool)>& callback) {
  base::DictionaryValue request_info;
  request_info.SetString(guestview::kUrl, requesting_frame.spec());
  request_info.SetBoolean(guestview::kUserGesture, user_gesture);

  // It is safe to hold an unretained pointer to
  // ChromeWebViewPermissionHelperDelegate because this callback is called from
  // ChromeWebViewPermissionHelperDelegate::SetPermission.
  const extensions::WebViewPermissionHelper::PermissionResponseCallback
      permission_callback =
      base::Bind(&ChromeWebViewPermissionHelperDelegate::
                     OnGeolocationPermissionResponse,
                 base::Unretained(this),
                 bridge_id,
                 user_gesture,
                 callback);
  int request_id = web_view_permission_helper()->RequestPermission(
      WEB_VIEW_PERMISSION_TYPE_GEOLOCATION,
      request_info,
      permission_callback,
      false /* allowed_by_default */);
  bridge_id_to_request_id_map_[bridge_id] = request_id;
}

void ChromeWebViewPermissionHelperDelegate::OnGeolocationPermissionResponse(
    int bridge_id,
    bool user_gesture,
    const base::Callback<void(bool)>& callback,
    bool allow,
    const std::string& user_input) {
  // The <webview> embedder has allowed the permission. We now need to make sure
  // that the embedder has geolocation permission.
  RemoveBridgeID(bridge_id);

  if (!allow || !web_view_guest()->attached()) {
    callback.Run(false);
    return;
  }

  Profile* profile = Profile::FromBrowserContext(
      web_view_guest()->browser_context());
  GeolocationPermissionContextFactory::GetForProfile(profile)->
      RequestGeolocationPermission(
          web_view_guest()->embedder_web_contents(),
          // The geolocation permission request here is not initiated
          // through WebGeolocationPermissionRequest. We are only interested
          // in the fact whether the embedder/app has geolocation
          // permission. Therefore we use an invalid |bridge_id|.
          -1,
          web_view_guest()->embedder_web_contents()->GetLastCommittedURL(),
          user_gesture,
          callback,
          NULL);
}

void ChromeWebViewPermissionHelperDelegate::CancelGeolocationPermissionRequest(
    int bridge_id) {
  int request_id = RemoveBridgeID(bridge_id);
  web_view_permission_helper()->CancelPendingPermissionRequest(request_id);
}

int ChromeWebViewPermissionHelperDelegate::RemoveBridgeID(int bridge_id) {
  std::map<int, int>::iterator bridge_itr =
      bridge_id_to_request_id_map_.find(bridge_id);
  if (bridge_itr == bridge_id_to_request_id_map_.end())
    return webview::kInvalidPermissionRequestID;

  int request_id = bridge_itr->second;
  bridge_id_to_request_id_map_.erase(bridge_itr);
  return request_id;
}

void ChromeWebViewPermissionHelperDelegate::RequestFileSystemPermission(
    const GURL& url,
    bool allowed_by_default,
    const base::Callback<void(bool)>& callback) {
  base::DictionaryValue request_info;
  request_info.SetString(guestview::kUrl, url.spec());
  web_view_permission_helper()->RequestPermission(
      WEB_VIEW_PERMISSION_TYPE_FILESYSTEM,
      request_info,
      base::Bind(&ChromeWebViewPermissionHelperDelegate::
                     OnFileSystemPermissionResponse,
                 base::Unretained(this),
                 callback),
      allowed_by_default);
}

void ChromeWebViewPermissionHelperDelegate::OnFileSystemPermissionResponse(
    const base::Callback<void(bool)>& callback,
    bool allow,
    const std::string& user_input) {
  callback.Run(allow && web_view_guest()->attached());
}

void ChromeWebViewPermissionHelperDelegate::FileSystemAccessedAsync(
    int render_process_id,
    int render_frame_id,
    int request_id,
    const GURL& url,
    bool blocked_by_policy) {
  RequestFileSystemPermission(
      url,
      !blocked_by_policy,
      base::Bind(&ChromeWebViewPermissionHelperDelegate::
                     FileSystemAccessedAsyncResponse,
                 base::Unretained(this),
                 render_process_id,
                 render_frame_id,
                 request_id,
                 url));
}

void ChromeWebViewPermissionHelperDelegate::FileSystemAccessedAsyncResponse(
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

void ChromeWebViewPermissionHelperDelegate::FileSystemAccessedSync(
    int render_process_id,
    int render_frame_id,
    const GURL& url,
    bool blocked_by_policy,
    IPC::Message* reply_msg) {
  RequestFileSystemPermission(
      url,
      !blocked_by_policy,
      base::Bind(&ChromeWebViewPermissionHelperDelegate::
                     FileSystemAccessedSyncResponse,
                 base::Unretained(this),
                 render_process_id,
                 render_frame_id,
                 url,
                 reply_msg));
}

void ChromeWebViewPermissionHelperDelegate::FileSystemAccessedSyncResponse(
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
