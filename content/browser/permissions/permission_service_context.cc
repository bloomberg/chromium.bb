// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/permissions/permission_service_context.h"

#include "content/browser/permissions/permission_service_impl.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"

namespace content {

PermissionServiceContext::PermissionServiceContext(
    RenderFrameHost* render_frame_host)
    : WebContentsObserver(WebContents::FromRenderFrameHost(render_frame_host)),
      render_frame_host_(render_frame_host),
      render_process_host_(nullptr) {
}

PermissionServiceContext::PermissionServiceContext(
    RenderProcessHost* render_process_host)
    : WebContentsObserver(nullptr),
      render_frame_host_(nullptr),
      render_process_host_(render_process_host) {
}

PermissionServiceContext::~PermissionServiceContext() {
}

void PermissionServiceContext::CreateService(
    mojo::InterfaceRequest<PermissionService> request) {
  PermissionServiceImpl* service =
      new PermissionServiceImpl(this);

  services_.push_back(service);
  mojo::WeakBindToRequest(service, &request);
}

void PermissionServiceContext::ServiceHadConnectionError(
    PermissionServiceImpl* service) {
  auto it = std::find(services_.begin(), services_.end(), service);
  DCHECK(it != services_.end());
  services_.erase(it);
}

void PermissionServiceContext::RenderFrameDeleted(
    RenderFrameHost* render_frame_host) {
  CancelPendingRequests(render_frame_host);
}

void PermissionServiceContext::DidNavigateAnyFrame(
    RenderFrameHost* render_frame_host,
    const LoadCommittedDetails& details,
    const FrameNavigateParams& params) {
  if (details.is_in_page)
    return;

  CancelPendingRequests(render_frame_host);
}

void PermissionServiceContext::CancelPendingRequests(
    RenderFrameHost* render_frame_host) const {
  if (render_frame_host != render_frame_host_)
    return;

  for (auto* service : services_)
    service->CancelPendingRequests();
}

BrowserContext* PermissionServiceContext::GetBrowserContext() const {
  if (!web_contents()) {
    DCHECK(render_process_host_);
    return render_process_host_->GetBrowserContext();
  }
  return web_contents()->GetBrowserContext();
}

GURL PermissionServiceContext::GetEmbeddingOrigin() const {
  return web_contents() ? web_contents()->GetLastCommittedURL().GetOrigin()
                        : GURL();
}

} // namespace content
