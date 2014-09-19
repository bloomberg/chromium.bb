// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_NAVIGATION_OBSERVER_H_
#define CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_NAVIGATION_OBSERVER_H_

#include <set>
#include <vector>

#include "base/memory/scoped_vector.h"
#include "base/values.h"
#include "chrome/browser/supervised_user/supervised_users.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

class SupervisedUserService;
class SupervisedUserURLFilter;

namespace content {
class NavigationEntry;
}

namespace infobars {
class InfoBar;
}

class SupervisedUserNavigationObserver
    : public content::WebContentsObserver,
      public content::WebContentsUserData<SupervisedUserNavigationObserver> {
 public:
  virtual ~SupervisedUserNavigationObserver();

  // Sets the specific infobar as dismissed.
  void WarnInfoBarDismissed();

  const std::vector<const content::NavigationEntry*>*
      blocked_navigations() const {
    return &blocked_navigations_.get();
  }

  // Called when a network request to |url| is blocked.
  static void OnRequestBlocked(int render_process_host_id,
                               int render_view_id,
                               const GURL& url,
                               const base::Callback<void(bool)>& callback);

 private:
  friend class content::WebContentsUserData<SupervisedUserNavigationObserver>;

  explicit SupervisedUserNavigationObserver(content::WebContents* web_contents);

  // content::WebContentsObserver implementation.
  virtual void DidCommitProvisionalLoadForFrame(
      content::RenderFrameHost* render_frame_host,
      const GURL& url,
      ui::PageTransition transition_type) OVERRIDE;

  void OnRequestBlockedInternal(const GURL& url);

  // Owned by the profile, so outlives us.
  SupervisedUserService* supervised_user_service_;

  // Owned by SupervisedUserService.
  const SupervisedUserURLFilter* url_filter_;

  // Owned by the InfoBarService, which has the same lifetime as this object.
  infobars::InfoBar* warn_infobar_;

  ScopedVector<const content::NavigationEntry> blocked_navigations_;

  DISALLOW_COPY_AND_ASSIGN(SupervisedUserNavigationObserver);
};

#endif  // CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_NAVIGATION_OBSERVER_H_
