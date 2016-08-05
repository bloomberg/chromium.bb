// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NTP_SNIPPETS_BOOKMARK_LAST_VISIT_UPDATER_H_
#define CHROME_BROWSER_NTP_SNIPPETS_BOOKMARK_LAST_VISIT_UPDATER_H_

#include <memory>

#include "base/macros.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace bookmarks {
class BookmarkModel;
}  // namespace bookmarks

namespace content {
class NavigationHandle;
class WebContents;
}  // namespace content

// A bridge class between platform-specific content::WebContentsObserver and the
// generic function ntp_snippets::UpdateBookmarkOnURLVisitedInMainFrame().
class BookmarkLastVisitUpdater
    : public content::WebContentsObserver,
      public content::WebContentsUserData<BookmarkLastVisitUpdater> {
 public:
  ~BookmarkLastVisitUpdater() override;

  static void CreateForWebContentsWithBookmarkModel(
      content::WebContents* web_contents,
      bookmarks::BookmarkModel* bookmark_model);

 private:
  friend class content::WebContentsUserData<BookmarkLastVisitUpdater>;

  BookmarkLastVisitUpdater(content::WebContents* web_contents,
                           bookmarks::BookmarkModel* bookmark_model);

  // Overridden from content::WebContentsObserver:
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;

  bookmarks::BookmarkModel* bookmark_model_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkLastVisitUpdater);
};

#endif  // CHROME_BROWSER_NTP_SNIPPETS_BOOKMARK_LAST_VISIT_UPDATER_H_
