// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_OFFLINE_PAGES_OFFLINE_PAGE_TAB_HELPER_H_
#define CHROME_BROWSER_ANDROID_OFFLINE_PAGES_OFFLINE_PAGE_TAB_HELPER_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}

namespace offline_pages {

// Per-tab class to manage switch between online version and offline version.
class OfflinePageTabHelper :
    public content::WebContentsObserver,
    public content::WebContentsUserData<OfflinePageTabHelper> {
 public:
  ~OfflinePageTabHelper() override;

 private:
  friend class content::WebContentsUserData<OfflinePageTabHelper>;
  friend class OfflinePageTabHelperTest;

  explicit OfflinePageTabHelper(content::WebContents* web_contents);

  // Overridden from content::WebContentsObserver:
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFailProvisionalLoad(
      content::RenderFrameHost* render_frame_host,
      const GURL& validated_url,
      int error_code,
      const base::string16& error_description,
      bool was_ignored_by_handler) override;

  void RedirectFromOfflineToOnline(const GURL& online_url);
  void RedirectFromOnlineToOffline(const GURL& offline_url);

  GURL last_redirect_from_url_;
  base::WeakPtrFactory<OfflinePageTabHelper> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(OfflinePageTabHelper);
};

}  // namespace offline_pages

#endif  // CHROME_BROWSER_ANDROID_OFFLINE_PAGES_OFFLINE_PAGE_TAB_HELPER_H_
