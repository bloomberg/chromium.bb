// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/bookmarks/bookmark_last_visit_utils.h"

#include <algorithm>
#include <string>
#include <utility>

#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "url/gurl.h"

using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;

namespace ntp_snippets {

namespace {

const char kBookmarkLastVisitDateKey[] = "last_visited";
const char kBookmarkDismissedFromNTP[] = "dismissed_from_ntp";

base::Time ParseLastVisitDate(const std::string& date_string) {
  int64_t date = 0;
  if (!base::StringToInt64(date_string, &date))
    return base::Time::UnixEpoch();
  return base::Time::FromInternalValue(date);
}

std::string FormatLastVisitDate(const base::Time& date) {
  return base::Int64ToString(date.ToInternalValue());
}

bool CompareBookmarksByLastVisitDate(const BookmarkNode* a,
                                     const BookmarkNode* b) {
  return GetLastVisitDateForBookmark(a) > GetLastVisitDateForBookmark(b);
}

}  // namespace

void UpdateBookmarkOnURLVisitedInMainFrame(BookmarkModel* bookmark_model,
                                           const GURL& url) {
  std::vector<const BookmarkNode*> bookmarks_for_url;
  bookmark_model->GetNodesByURL(url, &bookmarks_for_url);
  if (bookmarks_for_url.empty())
    return;

  // If there are bookmarks for |url|, set their last visit date to now.
  std::string now = FormatLastVisitDate(base::Time::Now());
  for (const BookmarkNode* node : bookmarks_for_url)
    bookmark_model->SetNodeMetaInfo(node, kBookmarkLastVisitDateKey, now);
}

base::Time GetLastVisitDateForBookmark(const BookmarkNode* node) {
  if (!node)
    return base::Time::UnixEpoch();

  std::string last_visit_date_string;
  node->GetMetaInfo(kBookmarkLastVisitDateKey, &last_visit_date_string);
  return ParseLastVisitDate(last_visit_date_string);
}

base::Time GetLastVisitDateForBookmarkIfNotDismissed(const BookmarkNode* node) {
  if (IsDismissedFromNTPForBookmark(node))
    return base::Time::UnixEpoch();

  return GetLastVisitDateForBookmark(node);
}

void MarkBookmarksDismissed(BookmarkModel* bookmark_model, const GURL& url) {
  std::vector<const BookmarkNode*> nodes;
  bookmark_model->GetNodesByURL(url, &nodes);
  for (const BookmarkNode* node : nodes)
    bookmark_model->SetNodeMetaInfo(node, kBookmarkDismissedFromNTP, "1");
}

bool IsDismissedFromNTPForBookmark(const BookmarkNode* node) {
  if (!node)
    return false;

  std::string dismissed_from_ntp;
  bool result =
      node->GetMetaInfo(kBookmarkDismissedFromNTP, &dismissed_from_ntp);
  DCHECK(!result || dismissed_from_ntp == "1");
  return result;
}

void MarkAllBookmarksUndismissed(BookmarkModel* bookmark_model) {
  // Get all the bookmark URLs.
  std::vector<BookmarkModel::URLAndTitle> bookmarks;
  bookmark_model->GetBookmarks(&bookmarks);

  // Remove dismissed flag from all bookmarks
  for (const BookmarkModel::URLAndTitle& bookmark : bookmarks) {
    std::vector<const BookmarkNode*> nodes;
    bookmark_model->GetNodesByURL(bookmark.url, &nodes);
    for (const BookmarkNode* node : nodes)
      bookmark_model->DeleteNodeMetaInfo(node, kBookmarkDismissedFromNTP);
  }
}

std::vector<const BookmarkNode*> GetRecentlyVisitedBookmarks(
    BookmarkModel* bookmark_model,
    int max_count,
    const base::Time& min_visit_time) {
  // Get all the bookmark URLs.
  std::vector<BookmarkModel::URLAndTitle> bookmarks;
  bookmark_model->GetBookmarks(&bookmarks);

  // Remove the URLs for that no bookmark has a recent visit (more recent than
  // |min_visit_time|). Recent visits to dismissed bookmarks are not considered.
  bookmarks.erase(
      std::remove_if(bookmarks.begin(), bookmarks.end(),
                     [&bookmark_model, &min_visit_time](
                         const BookmarkModel::URLAndTitle& bookmark) {
                       // Get all bookmarks for the given URL.
                       std::vector<const BookmarkNode*> bookmarks_for_url;
                       bookmark_model->GetNodesByURL(bookmark.url,
                                                     &bookmarks_for_url);

                       // Keep if at least one (non-dismissed) bookmark has been
                       // recently visited.
                       for (const BookmarkNode* node : bookmarks_for_url) {
                         if (GetLastVisitDateForBookmarkIfNotDismissed(node) >
                             min_visit_time)
                           return false;
                       }
                       // Otherwise erase this URL.
                       return true;
                     }),
      bookmarks.end());

  // Sort the remaining bookmarks by date.
  std::sort(bookmarks.begin(), bookmarks.end(),
            [&bookmark_model](const BookmarkModel::URLAndTitle& a,
                              const BookmarkModel::URLAndTitle& b) {
              return CompareBookmarksByLastVisitDate(
                  bookmark_model->GetMostRecentlyAddedUserNodeForURL(a.url),
                  bookmark_model->GetMostRecentlyAddedUserNodeForURL(b.url));
            });

  // Insert the first |max_count| items from |bookmarks| into |result|.
  std::vector<const BookmarkNode*> result;
  for (const BookmarkModel::URLAndTitle& bookmark : bookmarks) {
    result.push_back(
        bookmark_model->GetMostRecentlyAddedUserNodeForURL(bookmark.url));
    if (result.size() >= static_cast<size_t>(max_count))
      break;
  }
  return result;
}

std::vector<const BookmarkNode*> GetDismissedBookmarksForDebugging(
    BookmarkModel* bookmark_model) {
  // Get all the bookmark URLs.
  std::vector<BookmarkModel::URLAndTitle> bookmarks;
  bookmark_model->GetBookmarks(&bookmarks);

  // Remove the bookmark URLs which have at least one non-dismissed bookmark.
  bookmarks.erase(
      std::remove_if(
          bookmarks.begin(), bookmarks.end(),
          [&bookmark_model](const BookmarkModel::URLAndTitle& bookmark) {
            std::vector<const BookmarkNode*> bookmarks_for_url;
            bookmark_model->GetNodesByURL(bookmark.url, &bookmarks_for_url);

            for (const BookmarkNode* node : bookmarks_for_url) {
              if (!IsDismissedFromNTPForBookmark(node))
                return true;
            }
            return false;
          }),
      bookmarks.end());

  // Insert into |result|.
  std::vector<const BookmarkNode*> result;
  for (const BookmarkModel::URLAndTitle& bookmark : bookmarks) {
    result.push_back(
        bookmark_model->GetMostRecentlyAddedUserNodeForURL(bookmark.url));
  }
  return result;
}

}  // namespace ntp_snippets
