// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_NAVIGATION_OBSERVER_H_
#define CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_NAVIGATION_OBSERVER_H_

#include <vector>

#include "base/macros.h"
#include "chrome/browser/supervised_user/supervised_user_service_observer.h"
#include "chrome/browser/supervised_user/supervised_user_url_filter.h"
#include "chrome/browser/supervised_user/supervised_users.h"
#include "components/sessions/core/serialized_navigation_entry.h"
#include "components/supervised_user_error_page/supervised_user_error_page.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

class SupervisedUserService;

namespace content {
class NavigationHandle;
class WebContents;
}

class SupervisedUserNavigationObserver
    : public content::WebContentsUserData<SupervisedUserNavigationObserver>,
      public content::WebContentsObserver,
      public SupervisedUserServiceObserver {
 public:
  ~SupervisedUserNavigationObserver() override;

  const std::vector<std::unique_ptr<const sessions::SerializedNavigationEntry>>&
  blocked_navigations() const {
    return blocked_navigations_;
  }

  // Called when a network request to |url| is blocked.
  static void OnRequestBlocked(
      const content::ResourceRequestInfo::WebContentsGetter&
          web_contents_getter,
      const GURL& url,
      supervised_user_error_page::FilteringBehaviorReason reason,
      const base::Callback<void(bool)>& callback);

  // WebContentsObserver implementation.
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  // SupervisedUserServiceObserver implementation.
  void OnURLFilterChanged() override;

 private:
  friend class content::WebContentsUserData<SupervisedUserNavigationObserver>;

  explicit SupervisedUserNavigationObserver(content::WebContents* web_contents);

  void OnRequestBlockedInternal(const GURL& url);

  void URLFilterCheckCallback(
      const GURL& url,
      SupervisedUserURLFilter::FilteringBehavior behavior,
      supervised_user_error_page::FilteringBehaviorReason reason,
      bool uncertain);

  // Owned by SupervisedUserService.
  const SupervisedUserURLFilter* url_filter_;

  // Owned by SupervisedUserServiceFactory (lifetime of Profile).
  SupervisedUserService* supervised_user_service_;

  std::vector<std::unique_ptr<const sessions::SerializedNavigationEntry>>
      blocked_navigations_;

  base::WeakPtrFactory<SupervisedUserNavigationObserver> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(SupervisedUserNavigationObserver);
};

#endif  // CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_NAVIGATION_OBSERVER_H_
