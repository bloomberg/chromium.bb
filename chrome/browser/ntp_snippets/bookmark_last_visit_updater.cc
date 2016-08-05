// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ntp_snippets/bookmark_last_visit_updater.h"

#include "components/ntp_snippets/bookmarks/bookmark_last_visit_utils.h"
#include "content/public/browser/navigation_handle.h"

DEFINE_WEB_CONTENTS_USER_DATA_KEY(BookmarkLastVisitUpdater);

BookmarkLastVisitUpdater::~BookmarkLastVisitUpdater() {}

// static
void BookmarkLastVisitUpdater::CreateForWebContentsWithBookmarkModel(
    content::WebContents* web_contents,
    bookmarks::BookmarkModel* bookmark_model) {
  web_contents->SetUserData(UserDataKey(), new BookmarkLastVisitUpdater(
                                               web_contents, bookmark_model));
}

BookmarkLastVisitUpdater::BookmarkLastVisitUpdater(
    content::WebContents* web_contents,
    bookmarks::BookmarkModel* bookmark_model)
    : content::WebContentsObserver(web_contents),
      bookmark_model_(bookmark_model) {}

void BookmarkLastVisitUpdater::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInMainFrame() || navigation_handle->IsErrorPage())
    return;
  ntp_snippets::UpdateBookmarkOnURLVisitedInMainFrame(
      bookmark_model_, navigation_handle->GetURL());
}
