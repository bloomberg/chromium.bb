// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/bookmarks/browser/bookmark_client.h"

#include "base/logging.h"

bool BookmarkClient::PreferTouchIcon() {
  return false;
}

base::CancelableTaskTracker::TaskId BookmarkClient::GetFaviconImageForURL(
    const GURL& page_url,
    int icon_types,
    int desired_size_in_dip,
    const FaviconImageCallback& callback,
    base::CancelableTaskTracker* tracker) {
  return base::CancelableTaskTracker::kBadTaskId;
}

bool BookmarkClient::SupportsTypedCountForNodes() {
  return false;
}

void BookmarkClient::GetTypedCountForNodes(
    const NodeSet& nodes,
    NodeTypedCountPairs* node_typed_count_pairs) {
  NOTREACHED();
}
