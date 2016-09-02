// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_SNIPPETS_BOOKMARKS_BOOKMARK_LAST_VISIT_UTILS_H_
#define COMPONENTS_NTP_SNIPPETS_BOOKMARKS_BOOKMARK_LAST_VISIT_UTILS_H_

#include <vector>

class GURL;

namespace base {
class Time;
}  // namespace base

namespace bookmarks {
class BookmarkModel;
class BookmarkNode;
}  // namespace bookmarks

namespace ntp_snippets {

// If there is a bookmark for |url|, this function updates its last visit date
// to now. If there are multiple bookmarks for a given URL, it updates all of
// them.
void UpdateBookmarkOnURLVisitedInMainFrame(
    bookmarks::BookmarkModel* bookmark_model,
    const GURL& url);

// Gets the last visit date for a given bookmark |node|. The visit when the
// bookmark is created also counts. If no info about last visit date is present
// and |creation_date_fallback| is true, creation date is used.
base::Time GetLastVisitDateForBookmark(const bookmarks::BookmarkNode* node,
                                       bool creation_date_fallback);

// Like GetLastVisitDateForBookmark, but it returns the unix epoch if the
// bookmark is dismissed from NTP.
base::Time GetLastVisitDateForBookmarkIfNotDismissed(
    const bookmarks::BookmarkNode* node,
    bool creation_date_fallback);

// Marks all bookmarks with the given URL as dismissed.
void MarkBookmarksDismissed(bookmarks::BookmarkModel* bookmark_model,
                            const GURL& url);

// Gets the dismissed flag for a given bookmark |node|. Defaults to false.
bool IsDismissedFromNTPForBookmark(const bookmarks::BookmarkNode* node);

// Removes the dismissed flag from all bookmarks (only for debugging).
void MarkAllBookmarksUndismissed(bookmarks::BookmarkModel* bookmark_model);

// Returns the list of most recently visited, non-dismissed bookmarks.
// For each bookmarked URL, it returns the most recently created bookmark.
// The result is ordered by visit time (the most recent first). Only bookmarks
// visited after |min_visit_time| are considered, at most |max_count| bookmarks
// are returned. If this results into less than |min_count| bookmarks, the list
// is filled up with older bookmarks sorted by their last visit / creation date.
std::vector<const bookmarks::BookmarkNode*> GetRecentlyVisitedBookmarks(
    bookmarks::BookmarkModel* bookmark_model,
    int min_count,
    int max_count,
    const base::Time& min_visit_time,
    bool creation_date_fallback);

// Returns the list of all dismissed bookmarks. Only used for debugging.
std::vector<const bookmarks::BookmarkNode*> GetDismissedBookmarksForDebugging(
    bookmarks::BookmarkModel* bookmark_model);

// Removes last visited date metadata for all bookmarks.
void RemoveAllLastVisitDates(bookmarks::BookmarkModel* bookmark_model);

}  // namespace ntp_snippets

#endif  // COMPONENTS_NTP_SNIPPETS_BOOKMARKS_BOOKMARK_LAST_VISIT_UTILS_H_
