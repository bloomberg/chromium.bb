// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PERMISSIONS_PERMISSION_SERVICE_IMPL_H_
#define CONTENT_BROWSER_PERMISSIONS_PERMISSION_SERVICE_IMPL_H_

#include "base/id_map.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/permissions/permission_service_context.h"
#include "content/common/permission_service.mojom.h"
#include "content/public/browser/permission_type.h"

namespace content {

// Implements the PermissionService Mojo interface.
// This service can be created from a RenderFrameHost or a RenderProcessHost.
// It is owned by a PermissionServiceContext.
// It receives at PermissionServiceContext instance when created which allows it
// to have some information about the current context. That enables the service
// to know whether it can show UI and have knowledge of the associated
// WebContents for example.
class PermissionServiceImpl : public mojo::InterfaceImpl<PermissionService> {
 public:
  ~PermissionServiceImpl() override;

  // Clear pending permissions associated with a given frame and request the
  // browser to cancel the permission requests.
  void CancelPendingRequests();

 protected:
  friend PermissionServiceContext;

  PermissionServiceImpl(PermissionServiceContext* context);

 private:
  struct PendingRequest {
    PendingRequest(PermissionType permission, const GURL& origin);
    PermissionType permission;
    GURL origin;
  };
  typedef IDMap<PendingRequest, IDMapOwnPointer> RequestsMap;

  // PermissionService.
  void HasPermission(
      PermissionName permission,
      const mojo::String& origin,
      const mojo::Callback<void(PermissionStatus)>& callback) override;
  void RequestPermission(
      PermissionName permission,
      const mojo::String& origin,
      bool user_gesture,
      const mojo::Callback<void(PermissionStatus)>& callback) override;
  void RevokePermission(
      PermissionName permission,
      const mojo::String& origin,
      const mojo::Callback<void(PermissionStatus)>& callback) override;

  // mojo::InterfaceImpl.
  void OnConnectionError() override;

  void OnRequestPermissionResponse(
    const mojo::Callback<void(PermissionStatus)>& callback,
    int request_id,
    PermissionStatus status);

  PermissionStatus GetPermissionStatus(PermissionType type, GURL origin);
  void ResetPermissionStatus(PermissionType type, GURL origin);

  RequestsMap pending_requests_;
  // context_ owns |this|.
  PermissionServiceContext* context_;
  base::WeakPtrFactory<PermissionServiceImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(PermissionServiceImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_PERMISSIONS_PERMISSION_SERVICE_IMPL_H_
