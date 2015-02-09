// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enhanced_bookmarks/enhanced_bookmark_utils.h"

#include "base/i18n/string_compare.h"
#include "components/bookmarks/browser/bookmark_model.h"

using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;

namespace enhanced_bookmarks {

// Comparator used to sort bookmarks. No folders are allowed.
class BookmarkNameComparator : public std::binary_function<const BookmarkNode*,
                                                           const BookmarkNode*,
                                                           bool> {
 public:
  explicit BookmarkNameComparator(icu::Collator* collator)
      : collator_(collator) {}

  // Returns true if |n1| preceeds |n2|.
  bool operator()(const BookmarkNode* n1, const BookmarkNode* n2) {
    DCHECK(!n1->is_folder());
    DCHECK(!n2->is_folder());
    if (!collator_)
      return n1->GetTitle() < n2->GetTitle();
    return base::i18n::CompareString16WithCollator(
               collator_, n1->GetTitle(), n2->GetTitle()) == UCOL_LESS;
  }

 private:
  icu::Collator* collator_;
};

void SortBookmarksByName(std::vector<const BookmarkNode*>& nodes) {
  UErrorCode error = U_ZERO_ERROR;
  scoped_ptr<icu::Collator> collator(icu::Collator::createInstance(error));
  if (U_FAILURE(error))
    collator.reset(NULL);
  std::sort(nodes.begin(), nodes.end(), BookmarkNameComparator(collator.get()));
}

std::vector<const BookmarkNode*> PrimaryPermanentNodes(BookmarkModel* model) {
  DCHECK(model->loaded());
  std::vector<const BookmarkNode*> nodes;
  nodes.push_back(model->mobile_node());
  nodes.push_back(model->bookmark_bar_node());
  nodes.push_back(model->other_node());
  return nodes;
}

std::vector<const BookmarkNode*> RootLevelFolders(BookmarkModel* model) {
  std::vector<const BookmarkNode*> root_level_folders;

  // Find the direct folder children of the primary permanent nodes.
  std::vector<const BookmarkNode*> primary_permanent_nodes =
      PrimaryPermanentNodes(model);
  for (const BookmarkNode* parent : primary_permanent_nodes) {
    int child_count = parent->child_count();
    for (int i = 0; i < child_count; ++i) {
      const BookmarkNode* node = parent->GetChild(i);
      if (node->is_folder() && node->IsVisible())
        root_level_folders.push_back(node);
    }
  }
  return root_level_folders;
}

bool IsPrimaryPermanentNode(const BookmarkNode* node, BookmarkModel* model) {
  std::vector<const BookmarkNode*> primary_nodes(PrimaryPermanentNodes(model));
  if (std::find(primary_nodes.begin(), primary_nodes.end(), node) !=
      primary_nodes.end()) {
    return true;
  }
  return false;
}

const BookmarkNode* RootLevelFolderForNode(const BookmarkNode* node,
                                           BookmarkModel* model) {
  // This helper function doesn't work for managed bookmarks. This checks that
  // |node| is editable by the user, which currently covers all the other
  // bookmarks except the managed bookmarks.
  DCHECK(model->client()->CanBeEditedByUser(node));

  const std::vector<const BookmarkNode*> root_folders(RootLevelFolders(model));
  const BookmarkNode* top = node;
  while (top &&
         std::find(root_folders.begin(), root_folders.end(), top) ==
             root_folders.end()) {
    top = top->parent();
  }
  return top;
}

}  // namespace enhanced_bookmarks
