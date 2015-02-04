// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENHANCED_BOOKMARKS_ENHANCED_BOOKMARK_MODEL_OBSERVER_H_
#define COMPONENTS_ENHANCED_BOOKMARKS_ENHANCED_BOOKMARK_MODEL_OBSERVER_H_

#include <string>

namespace bookmarks {
class BookmarkNode;
}

namespace enhanced_bookmarks {

class EnhancedBookmarkModelObserver {
 public:
  // Called when the model has finished loading.
  virtual void EnhancedBookmarkModelLoaded() = 0;

  // Called from EnhancedBookmarkModel::ShutDown.
  virtual void EnhancedBookmarkModelShuttingDown() = 0;

  // Called when a node is added to the model.
  virtual void EnhancedBookmarkAdded(const bookmarks::BookmarkNode* node) = 0;

  // Called when a node is removed from the model.
  virtual void EnhancedBookmarkRemoved(const bookmarks::BookmarkNode* node) = 0;

  // Called when a node has changed.
  virtual void EnhancedBookmarkNodeChanged(
      const bookmarks::BookmarkNode* node) = 0;

  // Called when all user editable nodes are removed from the model.
  virtual void EnhancedBookmarkAllUserNodesRemoved() = 0;

  // Called when the remote id of a node changes. If |remote_id| is empty, the
  // remote id has been cleared. This could happen if multiple nodes with the
  // same remote id has been detected.
  virtual void EnhancedBookmarkRemoteIdChanged(
      const bookmarks::BookmarkNode* node,
      const std::string& old_remote_id,
      const std::string& remote_id) {}

 protected:
  virtual ~EnhancedBookmarkModelObserver() {}
};

}  // namespace enhanced_bookmarks

#endif  // COMPONENTS_ENHANCED_BOOKMARKS_ENHANCED_BOOKMARK_MODEL_OBSERVER_H_
