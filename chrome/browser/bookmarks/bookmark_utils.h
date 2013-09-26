// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BOOKMARKS_BOOKMARK_UTILS_H_
#define CHROME_BROWSER_BOOKMARKS_BOOKMARK_UTILS_H_

#include <string>
#include <vector>

#include "base/strings/string16.h"
#include "chrome/browser/bookmarks/bookmark_node_data.h"

class BookmarkModel;
class BookmarkNode;

namespace user_prefs {
class PrefRegistrySyncable;
}

// A collection of bookmark utility functions used by various parts of the UI
// that show bookmarks: bookmark manager, bookmark bar view ...
namespace bookmark_utils {

// Clones bookmark node, adding newly created nodes to |parent| starting at
// |index_to_add_at|. If |reset_node_times| is true cloned bookmarks and
// folders will receive new creation times and folder modification times
// instead of using the values stored in |elements|.
void CloneBookmarkNode(BookmarkModel* model,
                       const std::vector<BookmarkNodeData::Element>& elements,
                       const BookmarkNode* parent,
                       int index_to_add_at,
                       bool reset_node_times);

// Copies nodes onto the clipboard. If |remove_nodes| is true the nodes are
// removed after copied to the clipboard. The nodes are copied in such a way
// that if pasted again copies are made.
void CopyToClipboard(BookmarkModel* model,
                     const std::vector<const BookmarkNode*>& nodes,
                     bool remove_nodes);

// Pastes from the clipboard. The new nodes are added to |parent|, unless
// |parent| is null in which case this does nothing. The nodes are inserted
// at |index|. If |index| is -1 the nodes are added to the end.
void PasteFromClipboard(BookmarkModel* model,
                        const BookmarkNode* parent,
                        int index);

// Returns true if the user can copy from the pasteboard.
bool CanPasteFromClipboard(const BookmarkNode* node);

// Returns a vector containing up to |max_count| of the most recently modified
// folders. This never returns an empty vector.
std::vector<const BookmarkNode*> GetMostRecentlyModifiedFolders(
    BookmarkModel* model, size_t max_count);

// Returns the most recently added bookmarks. This does not return folders,
// only nodes of type url.
void GetMostRecentlyAddedEntries(BookmarkModel* model,
                                 size_t count,
                                 std::vector<const BookmarkNode*>* nodes);

// Returns true if |n1| was added more recently than |n2|.
bool MoreRecentlyAdded(const BookmarkNode* n1, const BookmarkNode* n2);

// Returns up to |max_count| bookmarks from |model| whose url or title contains
// the text |text|.  |languages| is user's accept-language setting to decode
// IDN.
void GetBookmarksContainingText(BookmarkModel* model,
                                const string16& text,
                                size_t max_count,
                                const std::string& languages,
                                std::vector<const BookmarkNode*>* nodes);

// Register user preferences for Bookmarks Bar.
void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

// Returns the parent for newly created folders/bookmarks. If |selection| has
// one element and it is a folder, |selection[0]| is returned, otherwise
// |parent| is returned. If |index| is non-null it is set to the index newly
// added nodes should be added at.
const BookmarkNode* GetParentForNewNodes(
    const BookmarkNode* parent,
    const std::vector<const BookmarkNode*>& selection,
    int* index);

// Deletes the bookmark folders for the given list of |ids|.
void DeleteBookmarkFolders(BookmarkModel* model, const std::vector<int64>& ids);

// If there are no bookmarks for url, a bookmark is created.
void AddIfNotBookmarked(BookmarkModel* model,
                        const GURL& url,
                        const string16& title);

// Removes all bookmarks for the given |url|.
void RemoveAllBookmarks(BookmarkModel* model, const GURL& url);

}  // namespace bookmark_utils

#endif  // CHROME_BROWSER_BOOKMARKS_BOOKMARK_UTILS_H_
