// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_OFFLINE_PAGES_OFFLINE_PAGE_TAB_HELPER_H_
#define CHROME_BROWSER_ANDROID_OFFLINE_PAGES_OFFLINE_PAGE_TAB_HELPER_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/offline_pages/core/request_header/offline_page_header.h"
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

  void SetOfflinePage(const OfflinePageItem& offline_page,
                      const OfflinePageHeader& offline_header,
                      bool is_offline_preview);

  const OfflinePageItem* offline_page() {
    return offline_info_.offline_page.get();
  }

  const OfflinePageHeader& offline_header() const {
    return offline_info_.offline_header;
  }

  // Whether the page is an offline preview.
  bool IsShowingOfflinePreview() const;

  // Returns provisional offline page since actual navigation does not happen
  // during unit tests.
  const OfflinePageItem* GetOfflinePageForTest() const;

 private:
  friend class content::WebContentsUserData<OfflinePageTabHelper>;

  // Contains the info about the offline page being loaded.
  struct LoadedOfflinePageInfo {
    LoadedOfflinePageInfo();
    ~LoadedOfflinePageInfo();

    // The cached copy of OfflinePageItem.
    std::unique_ptr<OfflinePageItem> offline_page;

    // The offline header that is provided when offline page is loaded.
    OfflinePageHeader offline_header;

    // Whether the page is an offline preview. Offline page previews are shown
    // when a user's effective connection type is prohibitively slow.
    bool is_showing_offline_preview;

    void Clear();
  };

  explicit OfflinePageTabHelper(content::WebContents* web_contents);

  // Overridden from content::WebContentsObserver:
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  void SelectPageForURLDone(const OfflinePageItem* offline_page);

  // The provisional info about the offline page being loaded. This is set when
  // the offline interceptor decides to serve the offline page and it will be
  // moved to |offline_info_| once the navigation is committed without error.
  LoadedOfflinePageInfo provisional_offline_info_;

  // The info about offline page being loaded. This is set from
  // |provisional_offline_info_| when the navigation is committed without error.
  // This can be used to by the Tab to synchronously ask about the offline
  // info.
  LoadedOfflinePageInfo offline_info_;

  bool reloading_url_on_net_error_ = false;

  base::WeakPtrFactory<OfflinePageTabHelper> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(OfflinePageTabHelper);
};

}  // namespace offline_pages

#endif  // CHROME_BROWSER_ANDROID_OFFLINE_PAGES_OFFLINE_PAGE_TAB_HELPER_H_
