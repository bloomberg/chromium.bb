// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GUEST_VIEW_WEB_VIEW_CHROME_WEB_VIEW_PERMISSION_HELPER_DELEGATE_H_
#define CHROME_BROWSER_GUEST_VIEW_WEB_VIEW_CHROME_WEB_VIEW_PERMISSION_HELPER_DELEGATE_H_

#include "extensions/browser/guest_view/web_view/web_view_permission_helper.h"
#include "extensions/browser/guest_view/web_view/web_view_permission_helper_delegate.h"

namespace extensions {
class WebViewGuest;
}

class ChromeWebViewPermissionHelperDelegate :
  public extensions::WebViewPermissionHelperDelegate {
 public:
  explicit ChromeWebViewPermissionHelperDelegate(
      extensions::WebViewPermissionHelper* web_view_permission_helper);
  virtual ~ChromeWebViewPermissionHelperDelegate();

  // WebViewPermissionHelperDelegate implementation.
  virtual void CanDownload(
      content::RenderViewHost* render_view_host,
      const GURL& url,
      const std::string& request_method,
      const base::Callback<void(bool)>& callback) OVERRIDE;
  virtual void RequestPointerLockPermission(
      bool user_gesture,
      bool last_unlocked_by_target,
      const base::Callback<void(bool)>& callback) OVERRIDE;
  virtual void RequestGeolocationPermission(
      int bridge_id,
      const GURL& requesting_frame,
      bool user_gesture,
      const base::Callback<void(bool)>& callback) OVERRIDE;
  virtual void CancelGeolocationPermissionRequest(int bridge_id) OVERRIDE;
  virtual void RequestFileSystemPermission(
      const GURL& url,
      bool allowed_by_default,
      const base::Callback<void(bool)>& callback) OVERRIDE;
  virtual void FileSystemAccessedAsync(
      int render_process_id,
      int render_frame_id,
      int request_id,
      const GURL& url,
      bool blocked_by_policy) OVERRIDE;
  virtual void FileSystemAccessedSync(
      int render_process_id,
      int render_frame_id,
      const GURL& url,
      bool blocked_by_policy,
      IPC::Message* reply_msg) OVERRIDE;
#if defined(ENABLE_PLUGINS)
  // content::WebContentsObserver implementation.
  virtual bool OnMessageReceived(
      const IPC::Message& message,
      content::RenderFrameHost* render_frame_host) OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
#endif  // defined(ENABLE_PLUGINS)

 private:
#if defined(ENABLE_PLUGINS)
  // Message handlers:
  void OnBlockedUnauthorizedPlugin(const base::string16& name,
                                   const std::string& identifier);
  void OnCouldNotLoadPlugin(const base::FilePath& plugin_path);
  void OnBlockedOutdatedPlugin(int placeholder_id,
                               const std::string& identifier);
  void OnNPAPINotSupported(const std::string& identifier);
  void OnOpenAboutPlugins();
#if defined(ENABLE_PLUGIN_INSTALLATION)
  void OnFindMissingPlugin(int placeholder_id, const std::string& mime_type);

  void OnRemovePluginPlaceholderHost(int placeholder_id);
#endif  // defined(ENABLE_PLUGIN_INSTALLATION)

  void OnPermissionResponse(const std::string& identifier,
                            bool allow,
                            const std::string& user_input);
#endif  // defined(ENABLE_PLUGINS)

  void OnGeolocationPermissionResponse(
      int bridge_id,
      bool user_gesture,
      const base::Callback<void(bool)>& callback,
      bool allow,
      const std::string& user_input);

  void OnFileSystemPermissionResponse(
      const base::Callback<void(bool)>& callback,
      bool allow,
      const std::string& user_input);

  void OnDownloadPermissionResponse(
      const base::Callback<void(bool)>& callback,
      bool allow,
      const std::string& user_input);

  void OnPointerLockPermissionResponse(
      const base::Callback<void(bool)>& callback,
      bool allow,
      const std::string& user_input);

  // Bridge IDs correspond to a geolocation request. This method will remove
  // the bookkeeping for a particular geolocation request associated with the
  // provided |bridge_id|. It returns the request ID of the geolocation request.
  int RemoveBridgeID(int bridge_id);

  void FileSystemAccessedAsyncResponse(int render_process_id,
                                       int render_frame_id,
                                       int request_id,
                                       const GURL& url,
                                       bool allowed);

  void FileSystemAccessedSyncResponse(int render_process_id,
                                      int render_frame_id,
                                      const GURL& url,
                                      IPC::Message* reply_msg,
                                      bool allowed);

  extensions::WebViewGuest* web_view_guest() {
    return web_view_permission_helper()->web_view_guest();
  }

  std::map<int, int> bridge_id_to_request_id_map_;

  base::WeakPtrFactory<ChromeWebViewPermissionHelperDelegate> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ChromeWebViewPermissionHelperDelegate);
};

#endif  // CHROME_BROWSER_GUEST_VIEW_WEB_VIEW_CHROME_WEB_VIEW_PERMISSION_HELPER_DELEGATE_H_
