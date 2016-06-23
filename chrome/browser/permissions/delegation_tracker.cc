// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/delegation_tracker.h"

#include <unordered_set>

#include "base/memory/ptr_util.h"
#include "chrome/browser/permissions/permission_util.h"
#include "content/public/browser/permission_type.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"

class DelegationTracker::DelegatedForChild : content::WebContentsObserver {
 public:
  DelegatedForChild(content::RenderFrameHost* child_rfh,
                    const std::vector<content::PermissionType>& permissions,
                    const base::Callback<void(content::RenderFrameHost*)>&
                        rfh_destroyed_callback)
      : content::WebContentsObserver(
            content::WebContents::FromRenderFrameHost(child_rfh)),
        child_rfh_(child_rfh),
        permissions_(permissions.begin(), permissions.end()),
        rfh_destroyed_callback_(rfh_destroyed_callback) {}

  ~DelegatedForChild() override {}

  bool HasPermission(const content::PermissionType& permission) {
    return permissions_.count(permission) == 1;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DelegatedForChild);

  void ClearPermissions(content::RenderFrameHost* render_frame_host) {
    if (render_frame_host == child_rfh_)
      rfh_destroyed_callback_.Run(render_frame_host);  // Will delete |this|.
  }

  // WebContentsObserver
  void RenderFrameHostChanged(content::RenderFrameHost* old_host,
                              content::RenderFrameHost* new_host) override {
    ClearPermissions(old_host);
  }

  void FrameDeleted(content::RenderFrameHost* render_frame_host) override {
    ClearPermissions(render_frame_host);
  }

  void DidNavigateAnyFrame(
      content::RenderFrameHost* render_frame_host,
      const content::LoadCommittedDetails& details,
      const content::FrameNavigateParams& params) override {
    ClearPermissions(render_frame_host);
  }

  content::RenderFrameHost* child_rfh_;

  std::unordered_set<content::PermissionType, PermissionTypeHash> permissions_;

  base::Callback<void(content::RenderFrameHost*)> rfh_destroyed_callback_;
};

DelegationTracker::DelegationTracker() {}

DelegationTracker::~DelegationTracker() {}

void DelegationTracker::SetDelegatedPermissions(
    content::RenderFrameHost* child_rfh,
    const std::vector<content::PermissionType>& permissions) {
  DCHECK(child_rfh && child_rfh->GetParent());
  delegated_permissions_[child_rfh] = base::WrapUnique(new DelegatedForChild(
      child_rfh, permissions,
      base::Bind(&DelegationTracker::RenderFrameHostChanged,
                 base::Unretained(this))));
}

bool DelegationTracker::IsGranted(content::RenderFrameHost* requesting_rfh,
                                  const content::PermissionType& permission) {
  content::RenderFrameHost* child_rfh = requesting_rfh;
  while (child_rfh->GetParent()) {
    // Parents with unique origins can't delegate permission.
    url::Origin parent_origin =
        child_rfh->GetParent()->GetLastCommittedOrigin();
    if (parent_origin.unique())
      return false;

    if (!child_rfh->GetLastCommittedOrigin().IsSameOriginWith(parent_origin)) {
      const auto& it = delegated_permissions_.find(child_rfh);
      if (it == delegated_permissions_.end())
        return false;
      if (!it->second->HasPermission(permission))
        return false;
    }
    child_rfh = child_rfh->GetParent();
  }
  return true;
}

void DelegationTracker::RenderFrameHostChanged(content::RenderFrameHost* rfh) {
  delegated_permissions_.erase(rfh);
}
