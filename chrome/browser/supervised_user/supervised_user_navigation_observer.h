// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_NAVIGATION_OBSERVER_H_
#define CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_NAVIGATION_OBSERVER_H_

#include <vector>

#include "base/memory/scoped_vector.h"
#include "chrome/browser/supervised_user/supervised_user_url_filter.h"
#include "chrome/browser/supervised_user/supervised_users.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class NavigationEntry;
class WebContents;
}

class SupervisedUserNavigationObserver
    : public content::WebContentsUserData<SupervisedUserNavigationObserver> {
 public:
  ~SupervisedUserNavigationObserver() override;

  const std::vector<const content::NavigationEntry*>*
      blocked_navigations() const {
    return &blocked_navigations_.get();
  }

  // Called when a network request to |url| is blocked.
  static void OnRequestBlocked(
      int render_process_host_id,
      int render_view_id,
      const GURL& url,
      SupervisedUserURLFilter::FilteringBehaviorReason reason,
      const base::Callback<void(bool)>& callback);

 private:
  friend class content::WebContentsUserData<SupervisedUserNavigationObserver>;

  explicit SupervisedUserNavigationObserver(content::WebContents* web_contents);

  void OnRequestBlockedInternal(const GURL& url);

  content::WebContents* web_contents_;

  // Owned by SupervisedUserService.
  const SupervisedUserURLFilter* url_filter_;

  ScopedVector<const content::NavigationEntry> blocked_navigations_;

  DISALLOW_COPY_AND_ASSIGN(SupervisedUserNavigationObserver);
};

#endif  // CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_NAVIGATION_OBSERVER_H_
