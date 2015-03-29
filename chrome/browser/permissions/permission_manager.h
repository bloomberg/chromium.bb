// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERMISSIONS_PERMISSION_MANAGER_H_
#define CHROME_BROWSER_PERMISSIONS_PERMISSION_MANAGER_H_

#include "base/callback_forward.h"
#include "base/macros.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/permission_manager.h"

class Profile;

namespace content {
enum class PermissionType;
};  // namespace content

class PermissionManager : public KeyedService,
                          public content::PermissionManager {
 public:
  explicit PermissionManager(Profile* profile);
  ~PermissionManager() override;

  // content::PermissionManager implementation.
  void RequestPermission(
      content::PermissionType permission,
      content::WebContents* web_contents,
      int request_id,
      const GURL& requesting_origin,
      bool user_gesture,
      const base::Callback<void(content::PermissionStatus)>& callback) override;
  void CancelPermissionRequest(content::PermissionType permission,
                               content::WebContents* web_contents,
                               int request_id,
                               const GURL& requesting_origin) override;
  void ResetPermission(content::PermissionType permission,
                       const GURL& requesting_origin,
                       const GURL& embedding_origin) override;
  content::PermissionStatus GetPermissionStatus(
      content::PermissionType permission,
      const GURL& requesting_origin,
      const GURL& embedding_origin) override;
  void RegisterPermissionUsage(content::PermissionType permission,
                               const GURL& requesting_origin,
                               const GURL& embedding_origin) override;

 private:
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(PermissionManager);
};

#endif // CHROME_BROWSER_PERMISSIONS_PERMISSION_MANAGER_H_
