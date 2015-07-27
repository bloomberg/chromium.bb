// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENHANCED_BOOKMARKS_ENHANCED_BOOKMARK_UTILS_H_
#define COMPONENTS_ENHANCED_BOOKMARKS_ENHANCED_BOOKMARK_UTILS_H_

#include <set>
#include <string>
#include <vector>

namespace bookmarks {
class BookmarkModel;
class BookmarkNode;
}

namespace enhanced_bookmarks {

// Possible locations where a bookmark can be opened from.
// Please sync with the corresponding histograms.xml.
//
// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.enhancedbookmarks
enum LaunchLocation {
  ALL_ITEMS = 0,
  UNCATEGORIZED = 1,  // Deprecated.
  FOLDER = 2,
  FILTER = 3,
  SEARCH = 4,
  BOOKMARK_EDITOR = 5,
  COUNT = 6,
};

// Returns the permanent nodes whose url children are considered uncategorized
// and whose folder children should be shown in the bookmark menu.
// |model| must be loaded.
std::vector<const bookmarks::BookmarkNode*> PrimaryPermanentNodes(
    bookmarks::BookmarkModel* model);

// Returns an unsorted vector of folders that are considered to be at the "root"
// level of the bookmark hierarchy. Functionally, this means all direct
// descendants of PrimaryPermanentNodes.
std::vector<const bookmarks::BookmarkNode*> RootLevelFolders(
    bookmarks::BookmarkModel* model);

// Returns whether |node| is a primary permanent node in the sense of
// |PrimaryPermanentNodes|.
bool IsPrimaryPermanentNode(const bookmarks::BookmarkNode* node,
                            bookmarks::BookmarkModel* model);

// Returns the root level folder in which this node is directly, or indirectly
// via subfolders, located.
const bookmarks::BookmarkNode* RootLevelFolderForNode(
    const bookmarks::BookmarkNode* node,
    bookmarks::BookmarkModel* model);

}  // namespace enhanced_bookmarks

#endif  // COMPONENTS_ENHANCED_BOOKMARKS_ENHANCED_BOOKMARK_UTILS_H_

