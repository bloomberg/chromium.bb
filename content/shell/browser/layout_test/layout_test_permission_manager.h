// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_BROWSER_LAYOUT_TEST_LAYOUT_TEST_PERMISSION_MANAGER_H_
#define CONTENT_SHELL_BROWSER_LAYOUT_TEST_LAYOUT_TEST_PERMISSION_MANAGER_H_

#include "base/callback_forward.h"
#include "base/containers/hash_tables.h"
#include "base/macros.h"
#include "content/public/browser/permission_manager.h"
#include "url/gurl.h"

namespace content {

class LayoutTestPermissionManager : public PermissionManager {
 public:
  LayoutTestPermissionManager();
  ~LayoutTestPermissionManager() override;

  // PermissionManager overrides.
  void RequestPermission(
      PermissionType permission,
      WebContents* web_contents,
      int request_id,
      const GURL& requesting_origin,
      bool user_gesture,
      const base::Callback<void(PermissionStatus)>& callback) override;
  void CancelPermissionRequest(PermissionType permission,
                               WebContents* web_contents,
                               int request_id,
                               const GURL& requesting_origin) override;
  void ResetPermission(PermissionType permission,
                       const GURL& requesting_origin,
                       const GURL& embedding_origin) override;
  PermissionStatus GetPermissionStatus(PermissionType permission,
                                       const GURL& requesting_origin,
                                       const GURL& embedding_origin) override;
  void RegisterPermissionUsage(PermissionType permission,
                               const GURL& requesting_origin,
                               const GURL& embedding_origin) override;
  int SubscribePermissionStatusChange(
      PermissionType permission,
      const GURL& requesting_origin,
      const GURL& embedding_origin,
      const base::Callback<void(PermissionStatus)>& callback) override;
  void UnsubscribePermissionStatusChange(int subscription_id) override;

  void SetPermission(PermissionType permission,
                     PermissionStatus status,
                     const GURL& origin,
                     const GURL& embedding_origin);
  void ResetPermissions();

 private:
  // Representation of a permission for the LayoutTestPermissionManager.
  struct PermissionDescription {
    PermissionDescription(PermissionType type,
                          const GURL& origin,
                          const GURL& embedding_origin);
    bool operator==(const PermissionDescription& other) const;

    // Hash operator for hash maps.
    struct Hash {
      size_t operator()(const PermissionDescription& description) const;
    };

    PermissionType type;
    GURL origin;
    GURL embedding_origin;
  };

  using PermissionsMap = base::hash_map<PermissionDescription,
                                        PermissionStatus,
                                        PermissionDescription::Hash>;

  // List of permissions currently known by the LayoutTestPermissionManager and
  // their associated |PermissionStatus|.
  PermissionsMap permissions_;

  DISALLOW_COPY_AND_ASSIGN(LayoutTestPermissionManager);
};

}  // namespace content

#endif // CONTENT_SHELL_BROWSER_LAYOUT_TEST_LAYOUT_TEST_PERMISSION_MANAGER_H_
