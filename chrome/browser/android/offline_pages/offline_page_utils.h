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
struct OfflinePageItem;

class OfflinePageUtils {
 public:
  // Returns true if |url| might point to an offline page.
  static bool MightBeOfflineURL(const GURL& url);

  // Gets an offline URL of an offline page with |online_url| if one exists.
  static void GetOfflineURLForOnlineURL(
      content::BrowserContext* browser_context,
      const GURL& online_url,
      const base::Callback<void(const GURL&)>& callback);

  // Gets an online URL of an offline page with |offline_url| if one exists.
  // Deprecated.  Use |GetOnlineURLForOfflineURL|.
  static GURL MaybeGetOnlineURLForOfflineURL(
      content::BrowserContext* browser_context,
      const GURL& offline_url);

  static void GetOnlineURLForOfflineURL(
      content::BrowserContext* browser_context,
      const GURL& offline_url,
      const base::Callback<void(const GURL&)>& callback);

  // Checks whether |offline_url| points to an offline page.
  // Deprecated.  Use something else.
  static bool IsOfflinePage(content::BrowserContext* browser_context,
                            const GURL& offline_url);

  // Checks whether offline page for |online_url| exists.
  static bool HasOfflinePageForOnlineURL(
      content::BrowserContext* browser_context,
      const GURL& online_url);

  // Marks that the offline page related to the |offline_url| has been accessed.
  static void MarkPageAccessed(content::BrowserContext* browser_context,
                               const GURL& offline_url);

  // Gets the offline page corresponding to the given web contents.  The
  // returned pointer is owned by the web_contents and may be deleted by user
  // navigation, so it is unsafe to store a copy of the returned pointer.
  static const OfflinePageItem* GetOfflinePageFromWebContents(
      content::WebContents* web_contents);

  // Gets an Android Tab ID from a tab containing |web_contents|. Returns false,
  // when tab is not available. Returns true otherwise and sets |tab_id| to the
  // ID of the tab.
  static bool GetTabId(content::WebContents* web_contents, int* tab_id);
};

}  // namespace offline_pages

#endif  // CHROME_BROWSER_ANDROID_OFFLINE_PAGES_OFFLINE_PAGE_UTILS_H_
