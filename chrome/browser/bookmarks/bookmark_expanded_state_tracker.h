// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BOOKMARKS_BOOKMARK_EXPANDED_STATE_TRACKER_H_
#define CHROME_BROWSER_BOOKMARKS_BOOKMARK_EXPANDED_STATE_TRACKER_H_

#include <set>

#include "chrome/browser/bookmarks/base_bookmark_model_observer.h"

class BookmarkModel;
class BookmarkNode;
class PrefService;

// BookmarkExpandedStateTracker is used to track a set of expanded nodes. The
// nodes are persisted in preferences. If an expanded node is removed from the
// model BookmarkExpandedStateTracker removes the node.
class BookmarkExpandedStateTracker : public BaseBookmarkModelObserver {
 public:
  typedef std::set<const BookmarkNode*> Nodes;

  BookmarkExpandedStateTracker(BookmarkModel* bookmark_model,
                               PrefService* pref_service);
  virtual ~BookmarkExpandedStateTracker();

  // The set of expanded nodes.
  void SetExpandedNodes(const Nodes& nodes);
  Nodes GetExpandedNodes();

 private:
  // BaseBookmarkModelObserver:
  virtual void Loaded(BookmarkModel* model, bool ids_reassigned) OVERRIDE;
  virtual void BookmarkModelChanged() OVERRIDE;
  virtual void BookmarkModelBeingDeleted(BookmarkModel* model) OVERRIDE;
  virtual void BookmarkNodeRemoved(BookmarkModel* model,
                                   const BookmarkNode* parent,
                                   int old_index,
                                   const BookmarkNode* node) OVERRIDE;
  virtual void BookmarkAllNodesRemoved(BookmarkModel* model) OVERRIDE;

  // Updates the value for |prefs::kBookmarkEditorExpandedNodes| from
  // GetExpandedNodes().
  void UpdatePrefs(const Nodes& nodes);

  BookmarkModel* bookmark_model_;
  PrefService* pref_service_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkExpandedStateTracker);
};

#endif  // CHROME_BROWSER_BOOKMARKS_BOOKMARK_EXPANDED_STATE_TRACKER_H_
