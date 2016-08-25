// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_OFFLINE_PAGES_OFFLINE_PAGE_TAB_HELPER_H_
#define CHROME_BROWSER_ANDROID_OFFLINE_PAGES_OFFLINE_PAGE_TAB_HELPER_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}

namespace offline_pages {

struct OfflinePageItem;

// Per-tab class that monitors the navigations and stores the necessary info
// to facilitate the synchronous access to offline information.
class OfflinePageTabHelper :
    public content::WebContentsObserver,
    public content::WebContentsUserData<OfflinePageTabHelper> {
 public:
  ~OfflinePageTabHelper() override;

  const OfflinePageItem* offline_page() { return offline_page_.get(); }
  void SetOfflinePage(const OfflinePageItem& offline_page,
                      bool is_offline_preview);

  // Whether the page is an offline preview.
  bool is_offline_preview() const { return is_offline_preview_; }

 private:
  friend class content::WebContentsUserData<OfflinePageTabHelper>;

  explicit OfflinePageTabHelper(content::WebContents* web_contents);

  // Overridden from content::WebContentsObserver:
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  void SelectPageForOnlineURLDone(const OfflinePageItem* offline_page);

  // The cached copy of OfflinePageItem if offline page is loaded for current
  // tab. This can be used to by the Tab to synchronously ask about the offline
  // info.
  std::unique_ptr<OfflinePageItem> offline_page_;

  bool reloading_url_on_net_error_ = false;

  // Whether the page is an offline preview. Offline page previews are shown
  // when a user's effective connection type is prohibitively slow.
  bool is_offline_preview_;

  base::WeakPtrFactory<OfflinePageTabHelper> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(OfflinePageTabHelper);
};

}  // namespace offline_pages

#endif  // CHROME_BROWSER_ANDROID_OFFLINE_PAGES_OFFLINE_PAGE_TAB_HELPER_H_
