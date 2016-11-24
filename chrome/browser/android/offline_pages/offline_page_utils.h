// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_OFFLINE_PAGES_OFFLINE_PAGE_UTILS_H_
#define CHROME_BROWSER_ANDROID_OFFLINE_PAGES_OFFLINE_PAGE_UTILS_H_

#include <stdint.h>

#include "base/callback.h"
#include "components/offline_pages/offline_page_model.h"

class GURL;

namespace base {
class Time;
}

namespace content {
class BrowserContext;
class WebContents;
}

namespace offline_pages {
struct OfflinePageHeader;
struct OfflinePageItem;

// Callback used for checking if specific |pages_exist| and what is the
// |latest_saved_time| for those pages. The former being a basic return type,
// while the latter is meant to be used as a helper to report UMA.
using PagesExistCallback =
    base::Callback<void(bool /* pages_exist */,
                        const base::Time& /* latest_saved_time */)>;

class OfflinePageUtils {
 public:
  // Returns via callback an offline page related to |url|, if any. The
  // page is chosen based on creation date; a more recently created offline
  // page will be preferred over an older one. The offline page captured from
  // last visit in the tab will not be considered if its tab id does not match
  // the provided |tab_id|.
  static void SelectPageForURL(
      content::BrowserContext* browser_context,
      const GURL& url,
      OfflinePageModel::URLSearchMode url_search_mode,
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

  // Performs a check, whether pages with specified |url| and |name_space|
  // already exist. Result is returned in a |callback|, where first parameter
  // indicates whether offline pages exist, while second is a helper value to
  // report UMA, indicating the time the latest existing page with such
  // parameters was saved.
  static void CheckExistenceOfPagesWithURL(
      content::BrowserContext* browser_context,
      const std::string name_space,
      const GURL& url,
      const PagesExistCallback& callback);

  // Performs a check, whether requests with specified |url| and |name_space|
  // already exist. Result is returned in a |callback|, where first parameter
  // indicates whether requests exist, while second is a helper value to report
  // UMA, indicating the time the latest existing request with such parameters
  // was created.
  static void CheckExistenceOfRequestsWithURL(
      content::BrowserContext* browser_context,
      const std::string name_space,
      const GURL& url,
      const PagesExistCallback& callback);

  static bool EqualsIgnoringFragment(const GURL& lhs, const GURL& rhs);
};

}  // namespace offline_pages

#endif  // CHROME_BROWSER_ANDROID_OFFLINE_PAGES_OFFLINE_PAGE_UTILS_H_
