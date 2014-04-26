// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bookmarks/test_bookmark_client.h"

#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_storage.h"

namespace test {

scoped_ptr<BookmarkModel> TestBookmarkClient::CreateModel(bool index_urls) {
  scoped_ptr<BookmarkModel> bookmark_model(new BookmarkModel(this, index_urls));
  bookmark_model->DoneLoading(bookmark_model->CreateLoadDetails(std::string()));
  return bookmark_model.Pass();
}

base::CancelableTaskTracker::TaskId TestBookmarkClient::GetFaviconImageForURL(
    const GURL& page_url,
    int icon_types,
    int desired_size_in_dip,
    const FaviconImageCallback& callback,
    base::CancelableTaskTracker* tracker) {
  return base::CancelableTaskTracker::kBadTaskId;
}

bool TestBookmarkClient::SupportsTypedCountForNodes() {
  return false;
}

void TestBookmarkClient::GetTypedCountForNodes(
    const NodeSet& nodes,
    NodeTypedCountPairs* node_typed_count_pairs) {
  NOTREACHED();
}

void TestBookmarkClient::RecordAction(const base::UserMetricsAction& action) {
}

void TestBookmarkClient::NotifyHistoryAboutRemovedBookmarks(
    const std::set<GURL>& removed_bookmark_urls) {
}

}  // namespace test
