// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_OFFLINE_PAGES_OFFLINE_PAGE_UTILS_H_
#define CHROME_BROWSER_ANDROID_OFFLINE_PAGES_OFFLINE_PAGE_UTILS_H_

#include <stdint.h>

#include "base/callback.h"

class GURL;

namespace content {
class BrowserContext;
class WebContents;
}

namespace offline_pages {
struct OfflinePageHeader;
struct OfflinePageItem;

class OfflinePageUtils {
 public:
  // Returns via callback an offline page saved for |online_url|, if any. The
  // page is chosen based on creation date; a more recently created offline
  // page will be preferred over an older one. The offline page captured from
  // last visit in the tab will not be considered if its tab id does not match
  // the provided |tab_id|.
  static void SelectPageForOnlineURL(
      content::BrowserContext* browser_context,
      const GURL& online_url,
      int tab_id,
      const base::Callback<void(const OfflinePageItem*)>& callback);

  // Gets the offline page corresponding to the given web contents.  The
  // returned pointer is owned by the web_contents and may be deleted by user
  // navigation, so it is unsafe to store a copy of the returned pointer.
  static const OfflinePageItem* GetOfflinePageFromWebContents(
      content::WebContents* web_contents);

  // Gets the offline header provided when loading the offline page for the
  // given web contents.
  static const OfflinePageHeader* GetOfflineHeaderFromWebContents(
      content::WebContents* web_contents);

  // Returns true if the offline page is shown for previewing purpose.
  static bool IsShowingOfflinePreview(content::WebContents* web_contents);

  // Gets an Android Tab ID from a tab containing |web_contents|. Returns false,
  // when tab is not available. Returns true otherwise and sets |tab_id| to the
  // ID of the tab.
  static bool GetTabId(content::WebContents* web_contents, int* tab_id);

  static void CheckExistenceOfPagesWithURL(
      content::BrowserContext* browser_context,
      const std::string name_space,
      const GURL& offline_url,
      const base::Callback<void(bool)>& callback);

  static void CheckExistenceOfRequestsWithURL(
      content::BrowserContext* browser_context,
      const std::string name_space,
      const GURL& offline_page_url,
      const base::Callback<void(bool)>& callback);

  static bool EqualsIgnoringFragment(const GURL& lhs, const GURL& rhs);
};

}  // namespace offline_pages

#endif  // CHROME_BROWSER_ANDROID_OFFLINE_PAGES_OFFLINE_PAGE_UTILS_H_
