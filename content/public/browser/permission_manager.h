// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_PERMISSION_MANAGER_H_
#define CONTENT_PUBLIC_BROWSER_PERMISSION_MANAGER_H_

#include "content/common/content_export.h"
#include "third_party/WebKit/public/platform/modules/permissions/permission_status.mojom.h"

class GURL;

namespace content {
enum class PermissionType;
class RenderFrameHost;

// This class allows the content layer to manipulate permissions. It has to be
// implemented by the embedder which ultimately handles the permission
// management for the content layer.
class CONTENT_EXPORT PermissionManager {
 public:
  // Constant retured when registering and subscribing if
  // cancelling/unsubscribing at a later stage would have no effect.
  static const int kNoPendingOperation = -1;

  virtual ~PermissionManager() = default;

  // Requests a permission on behalf of a frame identified by
  // render_frame_host.
  // When the permission request is handled, whether it failed, timed out or
  // succeeded, the |callback| will be run.
  // Returns a request id which can be used to cancel the permission (see
  // CancelPermissionRequest). This can be kNoPendingOperation if
  // there is no further need to cancel the permission in which case |callback|
  // was invoked.
  virtual int RequestPermission(
      PermissionType permission,
      RenderFrameHost* render_frame_host,
      const GURL& requesting_origin,
      bool user_gesture,
      const base::Callback<void(blink::mojom::PermissionStatus)>& callback) = 0;

  // Requests multiple permissions on behalf of a frame identified by
  // render_frame_host.
  // When the permission request is handled, whether it failed, timed out or
  // succeeded, the |callback| will be run. The order of statuses in the
  // returned vector will correspond to the order of requested permission
  // types.
  // Returns a request id which can be used to cancel the request (see
  // CancelPermissionRequest). This can be kNoPendingOperation if
  // there is no further need to cancel the permission in which case |callback|
  // was invoked.
  virtual int RequestPermissions(
      const std::vector<PermissionType>& permission,
      RenderFrameHost* render_frame_host,
      const GURL& requesting_origin,
      bool user_gesture,
      const base::Callback<void(
          const std::vector<blink::mojom::PermissionStatus>&)>& callback) = 0;

  // Returns the permission status of a given requesting_origin/embedding_origin
  // tuple. This is not taking a RenderFrameHost because the call might happen
  // outside of a frame context.
  virtual blink::mojom::PermissionStatus GetPermissionStatus(
      PermissionType permission,
      const GURL& requesting_origin,
      const GURL& embedding_origin) = 0;

  // Sets the permission back to its default for the requesting_origin/
  // embedding_origin tuple.
  virtual void ResetPermission(PermissionType permission,
                               const GURL& requesting_origin,
                               const GURL& embedding_origin) = 0;

  // Runs the given |callback| whenever the |permission| associated with the
  // pair { requesting_origin, embedding_origin } changes.
  // Returns the subscription_id to be used to unsubscribe. Can be
  // kNoPendingOperation if the subscribe was not successful.
  virtual int SubscribePermissionStatusChange(
      PermissionType permission,
      const GURL& requesting_origin,
      const GURL& embedding_origin,
      const base::Callback<void(blink::mojom::PermissionStatus)>& callback) = 0;

  // Unregisters from permission status change notifications.
  // The |subscription_id| must match the value returned by the
  // SubscribePermissionStatusChange call. Unsubscribing
  // an already unsubscribed |subscription_id| or providing the
  // |subscription_id| kNoPendingOperation is a no-op.
  virtual void UnsubscribePermissionStatusChange(int subscription_id) = 0;
};

}  // namespace content

#endif // CONTENT_PUBLIC_BROWSER_PERMISSION_MANAGER_H_
