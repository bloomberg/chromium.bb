// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/bookmarks/test/test_bookmark_client.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/browser/bookmark_storage.h"

namespace test {

TestBookmarkClient::TestBookmarkClient() {}

TestBookmarkClient::~TestBookmarkClient() {}

scoped_ptr<BookmarkModel> TestBookmarkClient::CreateModel(bool index_urls) {
  scoped_ptr<BookmarkModel> bookmark_model(new BookmarkModel(this, index_urls));
  scoped_ptr<bookmarks::BookmarkLoadDetails> details =
      bookmark_model->CreateLoadDetails(std::string());
  details->LoadExtraNodes();
  bookmark_model->DoneLoading(details.Pass());
  return bookmark_model.Pass();
}

void TestBookmarkClient::SetExtraNodesToLoad(
    bookmarks::BookmarkPermanentNodeList extra_nodes) {
  extra_nodes_to_load_ = extra_nodes.Pass();
  // Keep a copy in |extra_nodes_| for the acessor.
  extra_nodes_ = extra_nodes_to_load_.get();
}

bool TestBookmarkClient::IsExtraNodeRoot(const BookmarkNode* node) {
  for (size_t i = 0; i < extra_nodes_.size(); ++i) {
    if (node == extra_nodes_[i])
      return true;
  }
  return false;
}

bool TestBookmarkClient::IsAnExtraNode(const BookmarkNode* node) {
  if (!node)
    return false;
  for (size_t i = 0; i < extra_nodes_.size(); ++i) {
    if (node->HasAncestor(extra_nodes_[i]))
      return true;
  }
  return false;
}

bool TestBookmarkClient::IsPermanentNodeVisible(
    const BookmarkPermanentNode* node) {
  DCHECK(node->type() == BookmarkNode::BOOKMARK_BAR ||
         node->type() == BookmarkNode::OTHER_NODE ||
         node->type() == BookmarkNode::MOBILE ||
         IsExtraNodeRoot(node));
  return node->type() != BookmarkNode::MOBILE && !IsExtraNodeRoot(node);
}

void TestBookmarkClient::RecordAction(const base::UserMetricsAction& action) {
}

bookmarks::LoadExtraCallback TestBookmarkClient::GetLoadExtraNodesCallback() {
  return base::Bind(&TestBookmarkClient::LoadExtraNodes,
                    base::Passed(&extra_nodes_to_load_));
}

bool TestBookmarkClient::CanSetPermanentNodeTitle(
    const BookmarkNode* permanent_node) {
  return IsExtraNodeRoot(permanent_node);
}

bool TestBookmarkClient::CanSyncNode(const BookmarkNode* node) {
  return !IsAnExtraNode(node);
}

bool TestBookmarkClient::CanBeEditedByUser(const BookmarkNode* node) {
  return !IsAnExtraNode(node);
}

// static
bookmarks::BookmarkPermanentNodeList TestBookmarkClient::LoadExtraNodes(
    bookmarks::BookmarkPermanentNodeList extra_nodes,
    int64* next_id) {
  return extra_nodes.Pass();
}

}  // namespace test
