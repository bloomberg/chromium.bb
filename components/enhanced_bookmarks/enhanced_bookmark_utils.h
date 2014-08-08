// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENHANCED_BOOKMARKS_ENHANCED_BOOKMARK_UTILS_H_
#define COMPONENTS_ENHANCED_BOOKMARKS_ENHANCED_BOOKMARK_UTILS_H_

#include <set>
#include <string>
#include <vector>

class BookmarkModel;
class BookmarkNode;

namespace enhanced_bookmark_utils {

// Returns an ordered vector of bookmarks that are urls that match |query|.
// |query| must be UTF8 encoded.
std::vector<const BookmarkNode*> FindBookmarksWithQuery(
    BookmarkModel* bookmark_model,
    const std::string& query);

// The vector is sorted in place.
// All of the bookmarks in |nodes| must be urls.
void SortBookmarksByName(std::vector<const BookmarkNode*>& nodes);

// Returns the permanent nodes whose url children are considered uncategorized
// and whose folder children should be shown in the bookmark menu.
// |model| must be loaded.
std::vector<const BookmarkNode*> PrimaryPermanentNodes(BookmarkModel* model);

// Returns an unsorted vector of folders that are considered to be at the "root"
// level of the bookmark hierarchy. Functionally, this means all direct
// descendants of PrimaryPermanentNodes, and possibly a node for the bookmarks
// bar.
std::vector<const BookmarkNode*> RootLevelFolders(BookmarkModel* model);

// Returns whether |node| is a primary permanent node in the sense of
// |PrimaryPermanentNodes|.
bool IsPrimaryPermanentNode(const BookmarkNode* node, BookmarkModel* model);

// Returns the root level folder in which this node is directly, or indirectly
// via subfolders, located.
const BookmarkNode* RootLevelFolderForNode(const BookmarkNode* node,
                                           BookmarkModel* model);

}  // namespace enhanced_bookmark_utils

#endif  // COMPONENTS_ENHANCED_BOOKMARKS_ENHANCED_BOOKMARK_UTILS_H_
