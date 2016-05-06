// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_OFFLINE_PAGES_OFFLINE_PAGE_UTILS_H_
#define CHROME_BROWSER_ANDROID_OFFLINE_PAGES_OFFLINE_PAGE_UTILS_H_

#include <stdint.h>

class GURL;

namespace content {
class BrowserContext;
}

namespace offline_pages {
struct OfflinePageItem;

class OfflinePageUtils {
 public:
  // Returns true if |url| might point to an offline page.
  static bool MightBeOfflineURL(const GURL& url);

  // Gets an offline URL of an offline page with |online_url| if one exists.
  static GURL GetOfflineURLForOnlineURL(
      content::BrowserContext* browser_context,
      const GURL& online_url);

  // Gets an online URL of an offline page with |offline_url| if one exists.
  static GURL GetOnlineURLForOfflineURL(
      content::BrowserContext* browser_context,
      const GURL& offline_url);

  // Gets a bookmark ID related to the |offline_url|.
  static int64_t GetBookmarkIdForOfflineURL(
      content::BrowserContext* browser_context,
      const GURL& offline_url);

  // Checks whether |offline_url| points to an offline page.
  static bool IsOfflinePage(content::BrowserContext* browser_context,
                            const GURL& offline_url);

  // Checks whether offline page for |online_url| exists.
  static bool HasOfflinePageForOnlineURL(
      content::BrowserContext* browser_context,
      const GURL& online_url);

  // Marks that the offline page related to the |offline_url| has been accessed.
  static void MarkPageAccessed(content::BrowserContext* browser_context,
                               const GURL& offline_url);
};

}  // namespace offline_pages

#endif  // CHROME_BROWSER_ANDROID_OFFLINE_PAGES_OFFLINE_PAGE_UTILS_H_
