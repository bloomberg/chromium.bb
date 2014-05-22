// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BOOKMARKS_BROWSER_BOOKMARK_CLIENT_H_
#define COMPONENTS_BOOKMARKS_BROWSER_BOOKMARK_CLIENT_H_

#include <set>
#include <utility>
#include <vector>

#include "base/callback_forward.h"
#include "base/task/cancelable_task_tracker.h"
#include "components/favicon_base/favicon_callback.h"

class BookmarkNode;
class GURL;

namespace base {
struct UserMetricsAction;
}

// This class abstracts operations that depends on the embedder's environment,
// e.g. Chrome.
class BookmarkClient {
 public:
  // Types representing a set of BookmarkNode and a mapping from BookmarkNode
  // to the number of time the corresponding URL has been typed by the user in
  // the Omnibox.
  typedef std::set<const BookmarkNode*> NodeSet;
  typedef std::pair<const BookmarkNode*, int> NodeTypedCountPair;
  typedef std::vector<NodeTypedCountPair> NodeTypedCountPairs;

  // Returns true if the embedder favors touch icons over favicons.
  virtual bool PreferTouchIcon();

  // Requests the favicon of any of |icon_types| whose pixel sizes most
  // closely match |desired_size_in_dip| (if value is 0, the largest favicon
  // is returned) and desired scale factor for |page_url|. |callback| is run
  // when the bits have been fetched. |icon_types| can be any combination of
  // IconType value, but only one icon will be returned.
  virtual base::CancelableTaskTracker::TaskId GetFaviconImageForURL(
      const GURL& page_url,
      int icon_types,
      int desired_size_in_dip,
      const favicon_base::FaviconImageCallback& callback,
      base::CancelableTaskTracker* tracker);

  // Returns true if the embedder supports typed count for URL.
  virtual bool SupportsTypedCountForNodes();

  // Retrieves the number of time each BookmarkNode URL has been typed in
  // the Omnibox by the user.
  virtual void GetTypedCountForNodes(
      const NodeSet& nodes,
      NodeTypedCountPairs* node_typed_count_pairs);

  // Returns whether the embedder wants permanent node of type |node_type|
  // to always be visible or to only show them when not empty.
  virtual bool IsPermanentNodeVisible(int node_type) = 0;

  // Wrapper around RecordAction defined in base/metrics/user_metrics.h
  // that ensure that the action is posted from the correct thread.
  virtual void RecordAction(const base::UserMetricsAction& action) = 0;

 protected:
  virtual ~BookmarkClient() {}
};

#endif  // COMPONENTS_BOOKMARKS_BROWSER_BOOKMARK_CLIENT_H_
