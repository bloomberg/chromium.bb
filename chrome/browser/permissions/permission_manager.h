// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERMISSIONS_PERMISSION_MANAGER_H_
#define CHROME_BROWSER_PERMISSIONS_PERMISSION_MANAGER_H_

#include <unordered_map>

#include "base/callback_forward.h"
#include "base/id_map.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/content_settings/core/browser/content_settings_observer.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/permission_manager.h"

class PermissionContextBase;
class Profile;

namespace content {
enum class PermissionType;
class WebContents;
};  // namespace content

class PermissionManager : public KeyedService,
                          public content::PermissionManager,
                          public content_settings::Observer {
 public:
  static PermissionManager* Get(Profile* profile);

  explicit PermissionManager(Profile* profile);
  ~PermissionManager() override;

  // content::PermissionManager implementation.
  int RequestPermission(
      content::PermissionType permission,
      content::RenderFrameHost* render_frame_host,
      const GURL& requesting_origin,
      const base::Callback<void(blink::mojom::PermissionStatus)>& callback)
      override;
  int RequestPermissions(
      const std::vector<content::PermissionType>& permissions,
      content::RenderFrameHost* render_frame_host,
      const GURL& requesting_origin,
      const base::Callback<
          void(const std::vector<blink::mojom::PermissionStatus>&)>& callback)
      override;
  void CancelPermissionRequest(int request_id) override;
  void ResetPermission(content::PermissionType permission,
                       const GURL& requesting_origin,
                       const GURL& embedding_origin) override;
  blink::mojom::PermissionStatus GetPermissionStatus(
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
      const base::Callback<void(blink::mojom::PermissionStatus)>& callback)
      override;
  void UnsubscribePermissionStatusChange(int subscription_id) override;

 private:
  friend class GeolocationPermissionContextTests;
  // TODO(raymes): Refactor MediaPermission to not call GetPermissionContext.
  // See crbug.com/596786.
  friend class MediaPermission;

  class PendingRequest;
  using PendingRequestsMap = IDMap<PendingRequest, IDMapOwnPointer>;

  struct Subscription;
  using SubscriptionsMap = IDMap<Subscription, IDMapOwnPointer>;

  struct PermissionTypeHash {
    std::size_t operator()(const content::PermissionType& type) const;
  };

  PermissionContextBase* GetPermissionContext(content::PermissionType type);

  // Called when a permission was decided for a given PendingRequest. The
  // PendingRequest is identified by its |request_id| and the permission is
  // identified by its |permission_id|. If the PendingRequest contains more than
  // one permission, it will wait for the remaining permissions to be resolved.
  // When all the permissions have been resolved, the PendingRequest's callback
  // is run.
  void OnPermissionsRequestResponseStatus(
      int request_id,
      int permission_id,
      blink::mojom::PermissionStatus status);

  // Not all WebContents are able to display permission requests. If the PBM
  // is required but missing for |web_contents|, don't pass along the request.
  bool IsPermissionBubbleManagerMissing(content::WebContents* web_contents);

  // content_settings::Observer implementation.
  void OnContentSettingChanged(const ContentSettingsPattern& primary_pattern,
                               const ContentSettingsPattern& secondary_pattern,
                               ContentSettingsType content_type,
                               std::string resource_identifier) override;

  Profile* profile_;
  PendingRequestsMap pending_requests_;
  SubscriptionsMap subscriptions_;

  std::unordered_map<content::PermissionType,
                     std::unique_ptr<PermissionContextBase>,
                     PermissionTypeHash>
      permission_contexts_;

  base::WeakPtrFactory<PermissionManager> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(PermissionManager);
};

#endif // CHROME_BROWSER_PERMISSIONS_PERMISSION_MANAGER_H_
