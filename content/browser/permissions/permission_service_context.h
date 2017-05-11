// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PERMISSIONS_PERMISSION_SERVICE_CONTEXT_H_
#define CONTENT_BROWSER_PERMISSIONS_PERMISSION_SERVICE_CONTEXT_H_

#include "base/macros.h"
#include "content/public/browser/permission_type.h"
#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/bindings/strong_binding_set.h"
#include "third_party/WebKit/public/platform/modules/permissions/permission.mojom.h"

namespace service_manager {
struct BindSourceInfo;
}

namespace url {
class Origin;
}

namespace content {

class RenderFrameHost;
class RenderProcessHost;

// Provides information to a PermissionService. It is used by the
// PermissionServiceImpl to handle request permission UI.
// There is one PermissionServiceContext per RenderFrameHost/RenderProcessHost
// which owns it. It then owns all PermissionServiceImpl associated to their
// owner.
class PermissionServiceContext : public WebContentsObserver {
 public:
  explicit PermissionServiceContext(RenderFrameHost* render_frame_host);
  explicit PermissionServiceContext(RenderProcessHost* render_process_host);
  ~PermissionServiceContext() override;

  void CreateService(const service_manager::BindSourceInfo& source_info,
                     blink::mojom::PermissionServiceRequest request);

  void CreateSubscription(PermissionType permission_type,
                          const url::Origin& origin,
                          blink::mojom::PermissionObserverPtr observer);

  // Called when the connection to a PermissionObserver has an error.
  void ObserverHadConnectionError(int subscription_id);

  BrowserContext* GetBrowserContext() const;
  GURL GetEmbeddingOrigin() const;

  RenderFrameHost* render_frame_host() const;

 private:
  class PermissionSubscription;

  // WebContentsObserver
  void RenderFrameHostChanged(RenderFrameHost* old_host,
                              RenderFrameHost* new_host) override;
  void FrameDeleted(RenderFrameHost* render_frame_host) override;
  void DidFinishNavigation(NavigationHandle* navigation_handle) override;

  void CloseBindings(RenderFrameHost*);

  RenderFrameHost* render_frame_host_;
  RenderProcessHost* render_process_host_;
  mojo::StrongBindingSet<blink::mojom::PermissionService> services_;
  std::unordered_map<int, std::unique_ptr<PermissionSubscription>>
      subscriptions_;

  DISALLOW_COPY_AND_ASSIGN(PermissionServiceContext);
};

}  // namespace content

#endif  // CONTENT_BROWSER_PERMISSIONS_PERMISSION_SERVICE_CONTEXT_H_
