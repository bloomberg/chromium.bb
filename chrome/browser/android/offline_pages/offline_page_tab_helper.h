// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_OFFLINE_PAGES_OFFLINE_PAGE_TAB_HELPER_H_
#define CHROME_BROWSER_ANDROID_OFFLINE_PAGES_OFFLINE_PAGE_TAB_HELPER_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/offline_pages/offline_page_types.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}

namespace offline_pages {

struct OfflinePageItem;

// Per-tab class to manage switch between online version and offline version.
class OfflinePageTabHelper :
    public content::WebContentsObserver,
    public content::WebContentsUserData<OfflinePageTabHelper> {
 public:
  // Delegate that is used to better handle external dependencies.
  // Default implementation is in .cc file, while tests provide an override.
  class Delegate {
   public:
    virtual ~Delegate() {}
    virtual bool GetTabId(content::WebContents* web_contents,
                          std::string* tab_id) const = 0;
  };

  ~OfflinePageTabHelper() override;

  const OfflinePageItem* offline_page() { return offline_page_.get(); }

 private:
  enum class RedirectReason {
    DISCONNECTED_NETWORK,
    FLAKY_NETWORK,
    FLAKY_NETWORK_FORWARD_BACK
  };

  friend class content::WebContentsUserData<OfflinePageTabHelper>;
  friend class OfflinePageTabHelperTest;
  FRIEND_TEST_ALL_PREFIXES(OfflinePageTabHelperTest,
                           NewNavigationCancelsPendingRedirects);

  explicit OfflinePageTabHelper(content::WebContents* web_contents);

  void SetDelegateForTesting(std::unique_ptr<Delegate> delegate);

  // Overridden from content::WebContentsObserver:
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  void RedirectToOnline(const GURL& from_url,
                        const OfflinePageItem* offline_page);

  // 3 step redirection to the offline page. First getting all the pages, then
  // selecting appropriate page to redirect to and finally attempting to
  // redirect to that offline page, and caching metadata of that page locally.
  void GetPagesForRedirectToOffline(const GURL& online_url,
                                    RedirectReason reason);
  void SelectBestPageForRedirectToOffline(
      const GURL& online_url,
      RedirectReason reason,
      const MultipleOfflinePageItemResult& pages);
  void TryRedirectToOffline(RedirectReason redirect_reason,
                            const GURL& from_url,
                            const OfflinePageItem& offline_page);

  void Redirect(const GURL& from_url, const GURL& to_url);

  // Iff the tab we are associated with is redirected to an offline page,
  // |offline_page_| will be non-null.  This can be used to synchronously ask
  // about the offline state of the current web contents.
  std::unique_ptr<OfflinePageItem> offline_page_;
  std::unique_ptr<Delegate> delegate_;
  base::WeakPtrFactory<OfflinePageTabHelper> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(OfflinePageTabHelper);
};

}  // namespace offline_pages

#endif  // CHROME_BROWSER_ANDROID_OFFLINE_PAGES_OFFLINE_PAGE_TAB_HELPER_H_
