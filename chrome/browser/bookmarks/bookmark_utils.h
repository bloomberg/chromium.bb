// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BOOKMARKS_BOOKMARK_UTILS_H_
#define CHROME_BROWSER_BOOKMARKS_BOOKMARK_UTILS_H_

#include <string>
#include <vector>

#include "base/string16.h"
#include "chrome/browser/bookmarks/bookmark_editor.h"
#include "chrome/browser/bookmarks/bookmark_node_data.h"
#include "chrome/browser/history/snippet.h"

class BookmarkModel;
class BookmarkNode;
class PrefRegistrySyncable;

// A collection of bookmark utility functions used by various parts of the UI
// that show bookmarks: bookmark manager, bookmark bar view ...
namespace bookmark_utils {

// Clones bookmark node, adding newly created nodes to |parent| starting at
// |index_to_add_at|.
void CloneBookmarkNode(BookmarkModel* model,
                       const std::vector<BookmarkNodeData::Element>& elements,
                       const BookmarkNode* parent,
                       int index_to_add_at);

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

// Used by GetBookmarksMatchingText to return a matching node and the location
// of the match in the title.
struct TitleMatch {
  TitleMatch();
  ~TitleMatch();

  const BookmarkNode* node;

  // Location of the matching words in the title of the node.
  Snippet::MatchPositions match_positions;
};

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

// Returns true if |node|'s url or title contains the string |text|.
// |languages| is user's accept-language setting to decode IDN.
bool DoesBookmarkContainText(const BookmarkNode* node,
                             const string16& text,
                             const std::string& languages);

// Modifies a bookmark node (assuming that there's no magic that needs to be
// done regarding moving from one folder to another).  If a new node is
// explicitly being added, returns a pointer to the new node that was created.
// Otherwise the return value is identically |node|.
const BookmarkNode* ApplyEditsWithNoFolderChange(
    BookmarkModel* model,
    const BookmarkNode* parent,
    const BookmarkEditor::EditDetails& details,
    const string16& new_title,
    const GURL& new_url);

// Modifies a bookmark node assuming that the parent of the node may have
// changed and the node will need to be removed and reinserted.  If a new node
// is explicitly being added, returns a pointer to the new node that was
// created.  Otherwise the return value is identically |node|.
const BookmarkNode* ApplyEditsWithPossibleFolderChange(
    BookmarkModel* model,
    const BookmarkNode* new_parent,
    const BookmarkEditor::EditDetails& details,
    const string16& new_title,
    const GURL& new_url);

// Register user preferences for BookmarksBar.
void RegisterUserPrefs(PrefRegistrySyncable* registry);

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

// This enum is used for the Bookmarks.EntryPoint histogram.
enum BookmarkEntryPoint {
  ENTRY_POINT_ACCELERATOR,
  ENTRY_POINT_STAR_GESTURE,
  ENTRY_POINT_STAR_KEY,
  ENTRY_POINT_STAR_MOUSE,

  ENTRY_POINT_LIMIT // Keep this last.
};

// This enum is used for the Bookmarks.LaunchLocation histogram.
enum BookmarkLaunchLocation {
  LAUNCH_NONE,
  LAUNCH_ATTACHED_BAR = 0,
  LAUNCH_DETACHED_BAR,
  // These two are kind of sub-categories of the bookmark bar. Generally
  // a launch from a context menu or subfolder could be classified in one of
  // the other two bar buckets, but doing so is difficult because the menus
  // don't know of their greater place in Chrome.
  LAUNCH_BAR_SUBFOLDER,
  LAUNCH_CONTEXT_MENU,

  // Bookmarks menu within wrench menu.
  LAUNCH_WRENCH_MENU,
  // Bookmark manager.
  LAUNCH_MANAGER,
  // Autocomplete suggestion.
  LAUNCH_OMNIBOX,

  LAUNCH_LIMIT  // Keep this last.
};

// Records the launch of a bookmark for UMA purposes.
void RecordBookmarkLaunch(BookmarkLaunchLocation location);

// Records the user opening a folder of bookmarks for UMA purposes.
void RecordBookmarkFolderOpen(BookmarkLaunchLocation location);

// Records the user opening the apps page for UMA purposes.
void RecordAppsPageOpen(BookmarkLaunchLocation location);

}  // namespace bookmark_utils

#endif  // CHROME_BROWSER_BOOKMARKS_BOOKMARK_UTILS_H_
