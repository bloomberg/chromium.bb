// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugins/flash_temporary_permission_tracker.h"

#include "base/memory/ptr_util.h"
#include "base/stl_util.h"
#include "chrome/browser/plugins/flash_temporary_permission_tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/base/page_transition_types.h"

class FlashTemporaryPermissionTracker::GrantObserver
    : content::WebContentsObserver {
 public:
  GrantObserver(content::WebContents* web_contents,
                const GURL& origin,
                FlashTemporaryPermissionTracker* owner);

 private:
  // content::WebContentsObserver
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void FrameDeleted(content::RenderFrameHost* render_frame_host) override;
  void RenderFrameHostChanged(content::RenderFrameHost* old_host,
                              content::RenderFrameHost* new_host) override;

  GURL origin_;
  content::RenderFrameHost* original_rfh_;

  bool refreshed_;
  FlashTemporaryPermissionTracker* owner_;

  DISALLOW_COPY_AND_ASSIGN(GrantObserver);
};

// static
scoped_refptr<FlashTemporaryPermissionTracker>
FlashTemporaryPermissionTracker::Get(Profile* profile) {
  return FlashTemporaryPermissionTrackerFactory::GetForProfile(profile);
}

FlashTemporaryPermissionTracker::FlashTemporaryPermissionTracker(
    Profile* profile)
    : profile_(profile) {}

FlashTemporaryPermissionTracker::~FlashTemporaryPermissionTracker() {}

bool FlashTemporaryPermissionTracker::IsFlashEnabled(const GURL& url) {
  base::AutoLock lock(granted_origins_lock_);
  return base::ContainsKey(granted_origins_, url.GetOrigin());
}

void FlashTemporaryPermissionTracker::FlashEnabledForWebContents(
    content::WebContents* web_contents) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // In some infrequent circumstances (for example, if two permission bubbles
  // were being displayed and answered) we may receive a grant for an origin
  // that's already enabled. We just ignore the grant in this case.
  GURL origin = web_contents->GetLastCommittedURL().GetOrigin();
  {
    base::AutoLock lock(granted_origins_lock_);
    if (!base::ContainsKey(granted_origins_, origin)) {
      granted_origins_[origin] =
          base::MakeUnique<GrantObserver>(web_contents, origin, this);
    }
  }
  content::PluginService::GetInstance()->PurgePluginListCache(profile_, false);
}

void FlashTemporaryPermissionTracker::RevokeAccess(const GURL& origin) {
  {
    base::AutoLock lock(granted_origins_lock_);
    granted_origins_.erase(origin);
  }
  content::PluginService::GetInstance()->PurgePluginListCache(profile_, false);
}

void FlashTemporaryPermissionTracker::ShutdownOnUIThread() {
  DCHECK(granted_origins_.empty());
}

FlashTemporaryPermissionTracker::GrantObserver::GrantObserver(
    content::WebContents* web_contents,
    const GURL& origin,
    FlashTemporaryPermissionTracker* owner)
    : content::WebContentsObserver(web_contents),
      origin_(origin),
      original_rfh_(web_contents->GetMainFrame()),
      refreshed_(false),
      owner_(owner) {}

void FlashTemporaryPermissionTracker::GrantObserver::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  // Ignore navigations not associated with the main frame.
  if (!navigation_handle->IsInMainFrame())
    return;

  // Ignore unsuccesful navigations.
  if (!navigation_handle->HasCommitted())
    return;

  // If the page has already refreshed once, revoke access.
  if (refreshed_) {
    owner_->RevokeAccess(origin_);
    return;
  }

  // Don't revoke access to the page if it is being refreshed. Refreshing the
  // page happens as a part of answering the permission prompt so we don't
  // want that to revoke access. We verify the refresh is happening on the
  // same origin for security purposes.
  if (navigation_handle->GetPageTransition() == ui::PAGE_TRANSITION_RELOAD &&
      origin_ == navigation_handle->GetURL().GetOrigin()) {
    refreshed_ = true;
  } else {
    // All other navigation should revoke access.
    owner_->RevokeAccess(origin_);
  }
}

void FlashTemporaryPermissionTracker::GrantObserver::FrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  if (render_frame_host == original_rfh_)
    owner_->RevokeAccess(origin_);
}

void FlashTemporaryPermissionTracker::GrantObserver::RenderFrameHostChanged(
    content::RenderFrameHost* old_host,
    content::RenderFrameHost* new_host) {
  if (old_host == original_rfh_)
    owner_->RevokeAccess(origin_);
}
