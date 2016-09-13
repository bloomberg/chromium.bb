// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/bookmarks/bookmark_last_visit_utils.h"

#include <algorithm>
#include <set>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "url/gurl.h"

using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;

namespace ntp_snippets {

namespace {

struct RecentBookmark {
  const bookmarks::BookmarkNode* node;
  bool visited_recently;
};

const char* kBookmarksURLBlacklist[] = {"chrome://newtab/",
                                        "chrome-native://newtab/",
                                        "chrome://bookmarks/"};

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
                                     const BookmarkNode* b,
                                     bool creation_date_fallback) {
  return GetLastVisitDateForBookmark(a, creation_date_fallback) >
         GetLastVisitDateForBookmark(b, creation_date_fallback);
}

bool IsBlacklisted(const GURL& url) {
  for (const char* blacklisted : kBookmarksURLBlacklist) {
    if (url.spec() == blacklisted)
      return true;
  }
  return false;
}

}  // namespace

void UpdateBookmarkOnURLVisitedInMainFrame(BookmarkModel* bookmark_model,
                                           const GURL& url) {
  // Skip URLs that are blacklisted.
  if (IsBlacklisted(url))
    return;

  // Skip URLs that are not bookmarked.
  std::vector<const BookmarkNode*> bookmarks_for_url;
  bookmark_model->GetNodesByURL(url, &bookmarks_for_url);
  if (bookmarks_for_url.empty())
    return;

  // If there are bookmarks for |url|, set their last visit date to now.
  std::string now = FormatLastVisitDate(base::Time::Now());
  for (const BookmarkNode* node : bookmarks_for_url) {
    bookmark_model->SetNodeMetaInfo(node, kBookmarkLastVisitDateKey, now);
    // If the bookmark has been dismissed from NTP before, a new visit overrides
    // such a dismissal.
    bookmark_model->DeleteNodeMetaInfo(node, kBookmarkDismissedFromNTP);
  }
}

base::Time GetLastVisitDateForBookmark(const BookmarkNode* node,
                                       bool creation_date_fallback) {
  if (!node)
    return base::Time::UnixEpoch();

  std::string last_visit_date_string;
  if (!node->GetMetaInfo(kBookmarkLastVisitDateKey, &last_visit_date_string) &&
      creation_date_fallback) {
    return node->date_added();
  }
  return ParseLastVisitDate(last_visit_date_string);
}

base::Time GetLastVisitDateForBookmarkIfNotDismissed(
    const BookmarkNode* node,
    bool creation_date_fallback) {
  if (IsDismissedFromNTPForBookmark(node))
    return base::Time::UnixEpoch();

  return GetLastVisitDateForBookmark(node, creation_date_fallback);
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
    int min_count,
    int max_count,
    const base::Time& min_visit_time,
    bool creation_date_fallback) {
  // Get all the bookmark URLs.
  std::vector<BookmarkModel::URLAndTitle> bookmark_urls;
  bookmark_model->GetBookmarks(&bookmark_urls);

  std::vector<RecentBookmark> bookmarks;
  int recently_visited_count = 0;
  // Find for each bookmark the most recently visited BookmarkNode and find out
  // whether it is visited since |min_visit_time|.
  for (const BookmarkModel::URLAndTitle& url_and_title : bookmark_urls) {
    // Skip URLs that are blacklisted.
    if (IsBlacklisted(url_and_title.url))
      continue;

    // Get all bookmarks for the given URL.
    std::vector<const BookmarkNode*> bookmarks_for_url;
    bookmark_model->GetNodesByURL(url_and_title.url, &bookmarks_for_url);
    DCHECK(!bookmarks_for_url.empty());

    // Find the most recent node (minimal w.r.t.
    // CompareBookmarksByLastVisitDate).
    const BookmarkNode* most_recent = *std::min_element(
        bookmarks_for_url.begin(), bookmarks_for_url.end(),
        [creation_date_fallback](const BookmarkNode* a, const BookmarkNode* b) {
          return CompareBookmarksByLastVisitDate(a, b, creation_date_fallback);
        });

    // Find out if it has been _visited_ recently enough.
    bool visited_recently =
        min_visit_time < GetLastVisitDateForBookmark(
                             most_recent, /*creation_date_fallback=*/false);
    if (visited_recently)
      recently_visited_count++;
    if (!IsDismissedFromNTPForBookmark(most_recent))
      bookmarks.push_back({most_recent, visited_recently});
  }

  if (recently_visited_count < min_count) {
    // There aren't enough recently-visited bookmarks. Fill the list up to
    // |min_count| with older bookmarks (in particular those with only a
    // creation date, if creation_date_fallback is true).
    max_count = min_count;
  } else {
    // Remove the bookmarks that are not recently visited; we do not need them.
    // (We might end up with fewer than |min_count| bookmarks if all the recent
    // ones are dismissed.)
    bookmarks.erase(
        std::remove_if(bookmarks.begin(), bookmarks.end(),
                       [](const RecentBookmark& bookmark) {
                         return !bookmark.visited_recently;
                       }),
        bookmarks.end());
  }

  // Sort the remaining entries by date.
  std::sort(bookmarks.begin(), bookmarks.end(),
            [creation_date_fallback](const RecentBookmark& a,
                                     const RecentBookmark& b) {
              return CompareBookmarksByLastVisitDate(a.node, b.node,
                                                     creation_date_fallback);
            });

  // Insert the first |max_count| items from |bookmarks| into |result|.
  std::vector<const BookmarkNode*> result;
  for (const RecentBookmark& bookmark : bookmarks) {
    if (!creation_date_fallback &&
        GetLastVisitDateForBookmark(bookmark.node, creation_date_fallback) <
            min_visit_time) {
      break;
    }

    result.push_back(bookmark.node);
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
            DCHECK(!bookmarks_for_url.empty());

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

void RemoveAllLastVisitDates(bookmarks::BookmarkModel* bookmark_model) {
  // Get all the bookmark URLs.
  std::vector<BookmarkModel::URLAndTitle> bookmark_urls;
  bookmark_model->GetBookmarks(&bookmark_urls);

  for (const BookmarkModel::URLAndTitle& url_and_title : bookmark_urls) {
    // Get all bookmarks for the given URL.
    std::vector<const BookmarkNode*> bookmarks_for_url;
    bookmark_model->GetNodesByURL(url_and_title.url, &bookmarks_for_url);

    for (const BookmarkNode* bookmark : bookmarks_for_url) {
      bookmark_model->DeleteNodeMetaInfo(bookmark, kBookmarkLastVisitDateKey);
    }
  }
}

}  // namespace ntp_snippets
