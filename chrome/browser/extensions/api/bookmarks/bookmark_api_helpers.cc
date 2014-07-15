// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/bookmarks/bookmark_api_helpers.h"

#include <math.h>  // For floor()
#include <vector>

#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/bookmarks/chrome_bookmark_client.h"
#include "chrome/browser/extensions/api/bookmarks/bookmark_api_constants.h"
#include "chrome/common/extensions/api/bookmarks.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_utils.h"

namespace extensions {

namespace keys = bookmark_api_constants;
using api::bookmarks::BookmarkTreeNode;

namespace bookmark_api_helpers {

namespace {

void AddNodeHelper(ChromeBookmarkClient* client,
                   const BookmarkNode* node,
                   std::vector<linked_ptr<BookmarkTreeNode> >* nodes,
                   bool recurse,
                   bool only_folders) {
  if (node->IsVisible()) {
    linked_ptr<BookmarkTreeNode> new_node(GetBookmarkTreeNode(client,
                                                              node,
                                                              recurse,
                                                              only_folders));
    nodes->push_back(new_node);
  }
}

}  // namespace

BookmarkTreeNode* GetBookmarkTreeNode(ChromeBookmarkClient* client,
                                      const BookmarkNode* node,
                                      bool recurse,
                                      bool only_folders) {
  BookmarkTreeNode* bookmark_tree_node = new BookmarkTreeNode;

  bookmark_tree_node->id = base::Int64ToString(node->id());

  const BookmarkNode* parent = node->parent();
  if (parent) {
    bookmark_tree_node->parent_id.reset(new std::string(
        base::Int64ToString(parent->id())));
    bookmark_tree_node->index.reset(new int(parent->GetIndexOf(node)));
  }

  if (!node->is_folder()) {
    bookmark_tree_node->url.reset(new std::string(node->url().spec()));
  } else {
    // Javascript Date wants milliseconds since the epoch, ToDoubleT is seconds.
    base::Time t = node->date_folder_modified();
    if (!t.is_null()) {
      bookmark_tree_node->date_group_modified.reset(
          new double(floor(t.ToDoubleT() * 1000)));
    }
  }

  bookmark_tree_node->title = base::UTF16ToUTF8(node->GetTitle());
  if (!node->date_added().is_null()) {
    // Javascript Date wants milliseconds since the epoch, ToDoubleT is seconds.
    bookmark_tree_node->date_added.reset(
        new double(floor(node->date_added().ToDoubleT() * 1000)));
  }

  if (client->IsDescendantOfManagedNode(node))
    bookmark_tree_node->unmodifiable = BookmarkTreeNode::UNMODIFIABLE_MANAGED;

  if (recurse && node->is_folder()) {
    std::vector<linked_ptr<BookmarkTreeNode> > children;
    for (int i = 0; i < node->child_count(); ++i) {
      const BookmarkNode* child = node->GetChild(i);
      if (child->IsVisible() && (!only_folders || child->is_folder())) {
        linked_ptr<BookmarkTreeNode> child_node(
            GetBookmarkTreeNode(client, child, true, only_folders));
        children.push_back(child_node);
      }
    }
    bookmark_tree_node->children.reset(
        new std::vector<linked_ptr<BookmarkTreeNode> >(children));
  }
  return bookmark_tree_node;
}

void AddNode(ChromeBookmarkClient* client,
             const BookmarkNode* node,
             std::vector<linked_ptr<BookmarkTreeNode> >* nodes,
             bool recurse) {
  return AddNodeHelper(client, node, nodes, recurse, false);
}

void AddNodeFoldersOnly(ChromeBookmarkClient* client,
                        const BookmarkNode* node,
                        std::vector<linked_ptr<BookmarkTreeNode> >* nodes,
                        bool recurse) {
  return AddNodeHelper(client, node, nodes, recurse, true);
}

bool RemoveNode(BookmarkModel* model,
                ChromeBookmarkClient* client,
                int64 id,
                bool recursive,
                std::string* error) {
  const BookmarkNode* node = bookmarks::GetBookmarkNodeByID(model, id);
  if (!node) {
    *error = keys::kNoNodeError;
    return false;
  }
  if (model->is_permanent_node(node)) {
    *error = keys::kModifySpecialError;
    return false;
  }
  if (client->IsDescendantOfManagedNode(node)) {
    *error = keys::kModifyManagedError;
    return false;
  }
  if (node->is_folder() && !node->empty() && !recursive) {
    *error = keys::kFolderNotEmptyError;
    return false;
  }

  const BookmarkNode* parent = node->parent();
  model->Remove(parent, parent->GetIndexOf(node));
  return true;
}

void GetMetaInfo(const BookmarkNode& node,
                 base::DictionaryValue* id_to_meta_info_map) {
  if (!node.IsVisible())
    return;

  const BookmarkNode::MetaInfoMap* meta_info = node.GetMetaInfoMap();
  base::DictionaryValue* value = new base::DictionaryValue();
  if (meta_info) {
    BookmarkNode::MetaInfoMap::const_iterator itr;
    for (itr = meta_info->begin(); itr != meta_info->end(); ++itr) {
      value->SetStringWithoutPathExpansion(itr->first, itr->second);
    }
  }
  id_to_meta_info_map->Set(base::Int64ToString(node.id()), value);

  if (node.is_folder()) {
    for (int i = 0; i < node.child_count(); ++i) {
      GetMetaInfo(*(node.GetChild(i)), id_to_meta_info_map);
    }
  }
}

}  // namespace bookmark_api_helpers
}  // namespace extensions
