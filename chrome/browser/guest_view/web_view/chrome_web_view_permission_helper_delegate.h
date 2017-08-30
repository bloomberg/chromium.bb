// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GUEST_VIEW_WEB_VIEW_CHROME_WEB_VIEW_PERMISSION_HELPER_DELEGATE_H_
#define CHROME_BROWSER_GUEST_VIEW_WEB_VIEW_CHROME_WEB_VIEW_PERMISSION_HELPER_DELEGATE_H_

#include "base/macros.h"
#include "chrome/common/features.h"
#include "components/content_settings/core/common/content_settings.h"
#include "extensions/browser/guest_view/web_view/web_view_permission_helper.h"
#include "extensions/browser/guest_view/web_view/web_view_permission_helper_delegate.h"
#include "ppapi/features/features.h"
#include "third_party/WebKit/public/platform/modules/permissions/permission_status.mojom.h"

namespace extensions {
class WebViewGuest;

class ChromeWebViewPermissionHelperDelegate :
  public WebViewPermissionHelperDelegate {
 public:
  explicit ChromeWebViewPermissionHelperDelegate(
      WebViewPermissionHelper* web_view_permission_helper);
  ~ChromeWebViewPermissionHelperDelegate() override;

  // WebViewPermissionHelperDelegate implementation.
  void CanDownload(const GURL& url,
                   const std::string& request_method,
                   const base::Callback<void(bool)>& callback) override;
  void RequestPointerLockPermission(
      bool user_gesture,
      bool last_unlocked_by_target,
      const base::Callback<void(bool)>& callback) override;
  void RequestGeolocationPermission(
      int bridge_id,
      const GURL& requesting_frame,
      bool user_gesture,
      const base::Callback<void(bool)>& callback) override;
  void CancelGeolocationPermissionRequest(int bridge_id) override;
  void RequestFileSystemPermission(
      const GURL& url,
      bool allowed_by_default,
      const base::Callback<void(bool)>& callback) override;
  void FileSystemAccessedAsync(int render_process_id,
                               int render_frame_id,
                               int request_id,
                               const GURL& url,
                               bool blocked_by_policy) override;
#if BUILDFLAG(ENABLE_PLUGINS)
  // content::WebContentsObserver implementation.
  bool OnMessageReceived(const IPC::Message& message,
                         content::RenderFrameHost* render_frame_host) override;
#endif  // BUILDFLAG(ENABLE_PLUGINS)

 private:
#if BUILDFLAG(ENABLE_PLUGINS)
  // Message handlers:
  void OnBlockedUnauthorizedPlugin(const base::string16& name,
                                   const std::string& identifier);
  void OnCouldNotLoadPlugin(const base::FilePath& plugin_path);
  void OnBlockedOutdatedPlugin(int placeholder_id,
                               const std::string& identifier);
  void OnRemovePluginPlaceholderHost(int placeholder_id);
  void OnPermissionResponse(const std::string& identifier,
                            bool allow,
                            const std::string& user_input);
#endif  // BUILDFLAG(ENABLE_PLUGINS)

  void OnOpenPDF(const GURL& url);

  void OnGeolocationPermissionResponse(
      int bridge_id,
      bool user_gesture,
      const base::Callback<void(ContentSetting)>& callback,
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

  WebViewGuest* web_view_guest() {
    return web_view_permission_helper()->web_view_guest();
  }

  std::map<int, int> bridge_id_to_request_id_map_;

  base::WeakPtrFactory<ChromeWebViewPermissionHelperDelegate> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ChromeWebViewPermissionHelperDelegate);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_GUEST_VIEW_WEB_VIEW_CHROME_WEB_VIEW_PERMISSION_HELPER_DELEGATE_H_
