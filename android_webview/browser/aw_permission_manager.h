// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_AW_PERMISSION_MANAGER_H_
#define ANDROID_WEBVIEW_BROWSER_AW_PERMISSION_MANAGER_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/id_map.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/permission_manager.h"

namespace android_webview {

class LastRequestResultCache;

class AwPermissionManager : public content::PermissionManager {
 public:
  AwPermissionManager();
  ~AwPermissionManager() override;

  // PermissionManager implementation.
  int RequestPermission(
      content::PermissionType permission,
      content::RenderFrameHost* render_frame_host,
      const GURL& requesting_origin,
      const base::Callback<void(permissions::mojom::PermissionStatus)>&
          callback) override;
  int RequestPermissions(
      const std::vector<content::PermissionType>& permissions,
      content::RenderFrameHost* render_frame_host,
      const GURL& requesting_origin,
      const base::Callback<void(
          const std::vector<permissions::mojom::PermissionStatus>&)>& callback)
      override;
  void CancelPermissionRequest(int request_id) override;
  void ResetPermission(content::PermissionType permission,
                       const GURL& requesting_origin,
                       const GURL& embedding_origin) override;
  permissions::mojom::PermissionStatus GetPermissionStatus(
      content::PermissionType permission,
      const GURL& requesting_origin,
      const GURL& embedding_origin) override;
  void RegisterPermissionUsage(content::PermissionType permission,
                               const GURL& requesting_origin,
                               const GURL& embedding_origin) override;
  int SubscribePermissionStatusChange(
      content::PermissionType permission,
      const GURL& requesting_origin,
      const GURL& embedding_origin,
      const base::Callback<void(permissions::mojom::PermissionStatus)>&
          callback) override;
  void UnsubscribePermissionStatusChange(int subscription_id) override;

 private:
  struct PendingRequest;
  using PendingRequestsMap = IDMap<PendingRequest, IDMapOwnPointer>;

  // The weak pointer to this is used to clean up any information which is
  // stored in the pending request or result cache maps. However, the callback
  // should be run regardless of whether the class is still alive so the method
  // is static.
  static void OnRequestResponse(
      const base::WeakPtr<AwPermissionManager>& manager,
      int request_id,
      const base::Callback<void(permissions::mojom::PermissionStatus)>&
          callback,
      bool allowed);

  PendingRequestsMap pending_requests_;
  std::unique_ptr<LastRequestResultCache> result_cache_;

  base::WeakPtrFactory<AwPermissionManager> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(AwPermissionManager);
};

} // namespace android_webview

#endif // ANDROID_WEBVIEW_BROWSER_AW_PERMISSION_MANAGER_H_
