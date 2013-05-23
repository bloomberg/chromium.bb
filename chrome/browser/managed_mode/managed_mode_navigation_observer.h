// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MANAGED_MODE_MANAGED_MODE_NAVIGATION_OBSERVER_H_
#define CHROME_BROWSER_MANAGED_MODE_MANAGED_MODE_NAVIGATION_OBSERVER_H_

#include <set>

#include "base/values.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

class InfoBarDelegate;
class ManagedModeURLFilter;
class ManagedUserService;

class ManagedModeNavigationObserver
    : public content::WebContentsObserver,
      public content::WebContentsUserData<ManagedModeNavigationObserver> {
 public:
  virtual ~ManagedModeNavigationObserver();

  // Sets the specific infobar as dismissed.
  void WarnInfobarDismissed();

  // Returns the elevation state for the corresponding WebContents.
  bool is_elevated() const;

  // Set the elevation state for the corresponding WebContents.
  void set_elevated(bool is_elevated);

  // Adds a special history entry for the visit attempt and shows the
  // interstitial.
  static void OnRequestBlocked(int render_process_host_id,
                               int render_view_id,
                               const GURL& url,
                               const base::Callback<void(bool)>& callback);

 private:
  friend class content::WebContentsUserData<ManagedModeNavigationObserver>;

  explicit ManagedModeNavigationObserver(content::WebContents* web_contents);

  // content::WebContentsObserver implementation.
  virtual void ProvisionalChangeToMainFrameUrl(
      const GURL& url,
      content::RenderViewHost* render_view_host) OVERRIDE;
  virtual void DidCommitProvisionalLoadForFrame(
      int64 frame_id,
      bool is_main_frame,
      const GURL& url,
      content::PageTransition transition_type,
      content::RenderViewHost* render_view_host) OVERRIDE;

  // Owned by the profile, so outlives us.
  ManagedUserService* managed_user_service_;

  // Owned by ManagedUserService.
  const ManagedModeURLFilter* url_filter_;

  // Owned by the InfoBarService, which has the same lifetime as this object.
  InfoBarDelegate* warn_infobar_delegate_;

  // The elevation state corresponding to the current WebContents.
  // Will be set to true for non-managed users.
  bool is_elevated_;

  DISALLOW_COPY_AND_ASSIGN(ManagedModeNavigationObserver);
};

#endif  // CHROME_BROWSER_MANAGED_MODE_MANAGED_MODE_NAVIGATION_OBSERVER_H_
