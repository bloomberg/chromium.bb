// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BOOKMARKS_BOOKMARK_UTILS_H_
#define CHROME_BROWSER_BOOKMARKS_BOOKMARK_UTILS_H_
#pragma once

#include <string>
#include <vector>

#include "base/string16.h"
#include "chrome/browser/bookmarks/bookmark_node_data.h"
#include "chrome/browser/bookmarks/bookmark_editor.h"
#include "chrome/browser/history/snippet.h"
#include "ui/gfx/native_widget_types.h"
#include "webkit/glue/window_open_disposition.h"

class BookmarkModel;
class BookmarkNode;
class Browser;
class PageNavigator;
class PrefService;
class Profile;
class TabContents;

namespace views {
class DropTargetEvent;
}

// A collection of bookmark utility functions used by various parts of the UI
// that show bookmarks: bookmark manager, bookmark bar view ...
namespace bookmark_utils {

// Calculates the drop operation given |source_operations| and the ideal
// set of drop operations (|operations|). This prefers the following ordering:
// COPY, LINK then MOVE.
int PreferredDropOperation(int source_operations, int operations);

// Returns the drag operations for the specified node.
int BookmarkDragOperation(const BookmarkNode* node);

// Returns the preferred drop operation on a bookmark menu/bar.
// |parent| is the parent node the drop is to occur on and |index| the index the
// drop is over.
int BookmarkDropOperation(Profile* profile,
                          const views::DropTargetEvent& event,
                          const BookmarkNodeData& data,
                          const BookmarkNode* parent,
                          int index);

// Performs a drop of bookmark data onto |parent_node| at |index|. Returns the
// type of drop the resulted.
int PerformBookmarkDrop(Profile* profile,
                        const BookmarkNodeData& data,
                        const BookmarkNode* parent_node,
                        int index);

// Returns true if the bookmark data can be dropped on |drop_parent| at
// |index|. A drop from a separate profile is always allowed, where as
// a drop from the same profile is only allowed if none of the nodes in
// |data| are an ancestor of |drop_parent| and one of the nodes isn't already
// a child of |drop_parent| at |index|.
bool IsValidDropLocation(Profile* profile,
                         const BookmarkNodeData& data,
                         const BookmarkNode* drop_parent,
                         int index);

// Clones bookmark node, adding newly created nodes to |parent| starting at
// |index_to_add_at|.
void CloneBookmarkNode(BookmarkModel* model,
                       const std::vector<BookmarkNodeData::Element>& elements,
                       const BookmarkNode* parent,
                       int index_to_add_at);

// Begins dragging a group of bookmarks.
void DragBookmarks(Profile* profile,
                   const std::vector<const BookmarkNode*>& nodes,
                   gfx::NativeView view);

// Recursively opens all bookmarks. |initial_disposition| dictates how the
// first URL is opened, all subsequent URLs are opened as background tabs.
// |navigator| is used to open the URLs. If |navigator| is NULL the last
// tabbed browser with the profile |profile| is used. If there is no browser
// with the specified profile a new one is created.
void OpenAll(gfx::NativeWindow parent,
             Profile* profile,
             PageNavigator* navigator,
             const std::vector<const BookmarkNode*>& nodes,
             WindowOpenDisposition initial_disposition);

// Convenience for opening a single BookmarkNode.
void OpenAll(gfx::NativeWindow parent,
             Profile* profile,
             PageNavigator* navigator,
             const BookmarkNode* node,
             WindowOpenDisposition initial_disposition);

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

// Returns a name for the given URL. Used for drags into bookmark areas when
// the source doesn't specify a title.
string16 GetNameForURL(const GURL& url);

// Returns a vector containing up to |max_count| of the most recently modified
// groups. This never returns an empty vector.
std::vector<const BookmarkNode*> GetMostRecentlyModifiedGroups(
    BookmarkModel* model, size_t max_count);

// Returns the most recently added bookmarks. This does not return groups,
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
const BookmarkNode* ApplyEditsWithNoGroupChange(
    BookmarkModel* model,
    const BookmarkNode* parent,
    const BookmarkEditor::EditDetails& details,
    const string16& new_title,
    const GURL& new_url);

// Modifies a bookmark node assuming that the parent of the node may have
// changed and the node will need to be removed and reinserted.  If a new node
// is explicitly being added, returns a pointer to the new node that was
// created.  Otherwise the return value is identically |node|.
const BookmarkNode* ApplyEditsWithPossibleGroupChange(
    BookmarkModel* model,
    const BookmarkNode* new_parent,
    const BookmarkEditor::EditDetails& details,
    const string16& new_title,
    const GURL& new_url);

// Toggles whether the bookmark bar is shown only on the new tab page or on
// all tabs.  This is a preference modifier, not a visual modifier.
void ToggleWhenVisible(Profile* profile);

// Register user preferences for BookmarksBar.
void RegisterUserPrefs(PrefService* prefs);

// Fills in the URL and title for a bookmark of |tab_contents|.
void GetURLAndTitleToBookmark(TabContents* tab_contents,
                              GURL* url,
                              string16* title);

// Returns, by reference in |urls|, the url and title pairs for each open
// tab in browser.
void GetURLsForOpenTabs(Browser* browser,
                        std::vector<std::pair<GURL, string16> >* urls);

// Returns the parent for newly created folders/bookmarks. If |selection| has
// one element and it is a folder, |selection[0]| is returned, otherwise
// |parent| is returned. If |index| is non-null it is set to the index newly
// added nodes should be added at.
const BookmarkNode* GetParentForNewNodes(
    const BookmarkNode* parent,
    const std::vector<const BookmarkNode*>& selection,
    int* index);

// Returns true if the specified node is of type URL, or has a descendant
// of type URL.
bool NodeHasURLs(const BookmarkNode* node);

// Number of bookmarks we'll open before prompting the user to see if they
// really want to open all.
//
// NOTE: treat this as a const. It is not const as various tests change the
// value.
extern int num_urls_before_prompting;

}  // namespace bookmark_utils

#endif  // CHROME_BROWSER_BOOKMARKS_BOOKMARK_UTILS_H_
