// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BOOKMARKS_TEST_TEST_BOOKMARK_CLIENT_H_
#define COMPONENTS_BOOKMARKS_TEST_TEST_BOOKMARK_CLIENT_H_

#include "base/memory/scoped_ptr.h"
#include "components/bookmarks/browser/bookmark_client.h"

class BookmarkModel;

namespace test {

class TestBookmarkClient : public bookmarks::BookmarkClient {
 public:
  TestBookmarkClient();
  virtual ~TestBookmarkClient();

  // Create a BookmarkModel using this object as its client. The returned
  // BookmarkModel* is owned by the caller.
  scoped_ptr<BookmarkModel> CreateModel(bool index_urls);

  // Sets the list of extra nodes to be returned by the next call to
  // CreateModel() or GetLoadExtraNodesCallback().
  void SetExtraNodesToLoad(bookmarks::BookmarkPermanentNodeList extra_nodes);

  // Returns the current extra_nodes, set via SetExtraNodesToLoad().
  const std::vector<BookmarkPermanentNode*>& extra_nodes() {
    return extra_nodes_;
  }

  // Returns true if |node| is one of the |extra_nodes_|.
  bool IsExtraNodeRoot(const BookmarkNode* node);

  // Returns true if |node| belongs to the tree of one of the |extra_nodes_|.
  bool IsAnExtraNode(const BookmarkNode* node);

 private:
  // bookmarks::BookmarkClient:
  virtual bool IsPermanentNodeVisible(
      const BookmarkPermanentNode* node) OVERRIDE;
  virtual void RecordAction(const base::UserMetricsAction& action) OVERRIDE;
  virtual bookmarks::LoadExtraCallback GetLoadExtraNodesCallback() OVERRIDE;
  virtual bool CanSetPermanentNodeTitle(
      const BookmarkNode* permanent_node) OVERRIDE;
  virtual bool CanSyncNode(const BookmarkNode* node) OVERRIDE;
  virtual bool CanBeEditedByUser(const BookmarkNode* node) OVERRIDE;

  // Helpers for GetLoadExtraNodesCallback().
  static bookmarks::BookmarkPermanentNodeList LoadExtraNodes(
      bookmarks::BookmarkPermanentNodeList extra_nodes,
      int64* next_id);

  bookmarks::BookmarkPermanentNodeList extra_nodes_to_load_;
  std::vector<BookmarkPermanentNode*> extra_nodes_;

  DISALLOW_COPY_AND_ASSIGN(TestBookmarkClient);
};

}  // namespace test

#endif  // COMPONENTS_BOOKMARKS_TEST_TEST_BOOKMARK_CLIENT_H_
