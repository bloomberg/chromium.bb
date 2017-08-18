// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PERMISSIONS_PERMISSION_SERVICE_IMPL_H_
#define CONTENT_BROWSER_PERMISSIONS_PERMISSION_SERVICE_IMPL_H_

#include "base/callback.h"
#include "base/containers/id_map.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/permissions/permission_service_context.h"
#include "content/common/content_export.h"
#include "third_party/WebKit/public/platform/modules/permissions/permission.mojom.h"
#include "url/origin.h"

namespace content {

enum class PermissionType;

// Implements the PermissionService Mojo interface.
// This service can be created from a RenderFrameHost or a RenderProcessHost.
// It is owned by a PermissionServiceContext.
// It receives at PermissionServiceContext instance when created which allows it
// to have some information about the current context. That enables the service
// to know whether it can show UI and have knowledge of the associated
// WebContents for example.
class CONTENT_EXPORT PermissionServiceImpl
    : public blink::mojom::PermissionService {
 public:
  PermissionServiceImpl(PermissionServiceContext* context);
  ~PermissionServiceImpl() override;

 private:
  friend class PermissionServiceImplTest;

  using PermissionStatusCallback =
      base::OnceCallback<void(blink::mojom::PermissionStatus)>;

  class PendingRequest;
  using RequestsMap = base::IDMap<std::unique_ptr<PendingRequest>>;

  // blink::mojom::PermissionService.
  void HasPermission(blink::mojom::PermissionDescriptorPtr permission,
                     const url::Origin& origin,
                     PermissionStatusCallback callback) override;
  void RequestPermission(blink::mojom::PermissionDescriptorPtr permission,
                         const url::Origin& origin,
                         bool user_gesture,
                         PermissionStatusCallback callback) override;
  void RequestPermissions(
      std::vector<blink::mojom::PermissionDescriptorPtr> permissions,
      const url::Origin& origin,
      bool user_gesture,
      RequestPermissionsCallback callback) override;
  void RevokePermission(blink::mojom::PermissionDescriptorPtr permission,
                        const url::Origin& origin,
                        PermissionStatusCallback callback) override;
  void AddPermissionObserver(
      blink::mojom::PermissionDescriptorPtr permission,
      const url::Origin& origin,
      blink::mojom::PermissionStatus last_known_status,
      blink::mojom::PermissionObserverPtr observer) override;

  void OnRequestPermissionsResponse(
      int pending_request_id,
      const std::vector<blink::mojom::PermissionStatus>& result);

  blink::mojom::PermissionStatus GetPermissionStatus(
      const blink::mojom::PermissionDescriptorPtr& permission,
      const url::Origin& origin);
  blink::mojom::PermissionStatus GetPermissionStatusFromType(
      PermissionType type,
      const url::Origin& origin);
  void ResetPermissionStatus(PermissionType type, const url::Origin& origin);

  RequestsMap pending_requests_;
  // context_ owns |this|.
  PermissionServiceContext* context_;
  base::WeakPtrFactory<PermissionServiceImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(PermissionServiceImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_PERMISSIONS_PERMISSION_SERVICE_IMPL_H_
