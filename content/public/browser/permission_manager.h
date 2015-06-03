// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_PERMISSION_MANAGER_H_
#define CONTENT_PUBLIC_BROWSER_PERMISSION_MANAGER_H_

#include "content/common/content_export.h"
#include "content/public/common/permission_status.mojom.h"

class GURL;

namespace content {
enum class PermissionType;
class RenderFrameHost;

class CONTENT_EXPORT PermissionManager {
 public:
  virtual ~PermissionManager() = default;

  virtual void RequestPermission(
      PermissionType permission,
      RenderFrameHost* render_frame_host,
      int request_id,
      const GURL& requesting_origin,
      bool user_gesture,
      const base::Callback<void(PermissionStatus)>& callback) = 0;

  virtual void CancelPermissionRequest(PermissionType permission,
                                       RenderFrameHost* render_frame_host,
                                       int request_id,
                                       const GURL& requesting_origin) = 0;

  virtual PermissionStatus GetPermissionStatus(
      PermissionType permission,
      const GURL& requesting_origin,
      const GURL& embedding_origin) = 0;

  virtual void ResetPermission(PermissionType permission,
                               const GURL& requesting_origin,
                               const GURL& embedding_origin) = 0;

  virtual void RegisterPermissionUsage(PermissionType permission,
                                       const GURL& requesting_origin,
                                       const GURL& embedding_origin) = 0;

  // Runs the given |callback| whenever the |permission| associated with the
  // pair { requesting_origin, embedding_origin } changes.
  // Returns the subscription_id to be used to unsubscribe.
  virtual int SubscribePermissionStatusChange(
      PermissionType permission,
      const GURL& requesting_origin,
      const GURL& embedding_origin,
      const base::Callback<void(PermissionStatus)>& callback) = 0;

  virtual void UnsubscribePermissionStatusChange(int subscription_id) = 0;
};

}  // namespace content

#endif // CONTENT_PUBLIC_BROWSER_PERMISSION_MANAGER_H_
