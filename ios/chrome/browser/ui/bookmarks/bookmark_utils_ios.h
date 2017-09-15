// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_UTILS_IOS_H_
#define IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_UTILS_IOS_H_

#import <UIKit/UIKit.h>

#include <memory>
#include <set>
#include <vector>

#include "base/strings/string16.h"
#include "base/time/time.h"

@class BookmarkCell;
@class BookmarkMenuItem;
@class BookmarkPositionCache;
@class BookmarkPathCache;
class GURL;

namespace bookmarks {
class BookmarkModel;
class BookmarkNode;
}  // namespace bookmarks

namespace ios {
class ChromeBrowserState;
}  // namespace ios

namespace bookmark_utils_ios {

typedef std::vector<const bookmarks::BookmarkNode*> NodeVector;
typedef std::set<const bookmarks::BookmarkNode*> NodeSet;

// Finds bookmark node passed in |id|, in the |model|.
const bookmarks::BookmarkNode* FindFolderById(bookmarks::BookmarkModel* model,
                                              int64_t id);

// The iOS code is doing some munging of the bookmark folder names in order
// to display a slighly different wording for the default folders.
NSString* TitleForBookmarkNode(const bookmarks::BookmarkNode* node);

// Returns the default color for |url| when no image is available.
UIColor* DefaultColor(const GURL& url);

// Returns the subtitle relevant to the bookmark navigation ui.
NSString* subtitleForBookmarkNode(const bookmarks::BookmarkNode* node);

// This margin is designed to align with the menu button in the navigation bar.
extern const CGFloat menuMargin;
// The margin from the left of the screen for the title of the navigation bar.
extern const CGFloat titleMargin;
// The distance between the icon in hamburger menu and the menu item title.
extern const CGFloat titleToIconDistance;
// The amount of time it takes to show or hide the bookmark menu.
extern const CGFloat menuAnimationDuration;

// On iPad, background color can be transparent. Wrapper for the light grey
// background color.
UIColor* mainBackgroundColor();
// Returns the menu's background color. White when the menu is in a slide over
// panel, transparent otherwise.
UIColor* menuBackgroundColor();
// Primary title labels use this color.
UIColor* darkTextColor();
// Secondary title labels use this color.
UIColor* lightTextColor();
// The color to use if the text needs to change color when highlighted.
UIColor* highlightedDarkTextColor();
// The color used for the editing bar.
UIColor* blueColor();
// The color used for the navigation bar.
UIColor* GrayColor();
// The gray color for line separators.
UIColor* separatorColor();
// The black color for the folder labels.
UIColor* FolderLabelColor();

// Returns the current status bar height.
CGFloat StatusBarHeight();

// Returns whether the bookmark menu should be presented in a slide in panel.
BOOL bookmarkMenuIsInSlideInPanel();

// Creates a drop shadow with the given width.
UIView* dropShadowWithWidth(CGFloat width);

#pragma mark - Updating Bookmarks

// Creates the bookmark if |node| is NULL. Otherwise updates |node|.
// |folder| is the intended parent of |node|.
// A snackbar is presented, that let the user undo the changes.
void CreateOrUpdateBookmarkWithUndoToast(
    const bookmarks::BookmarkNode* node,
    NSString* title,
    const GURL& url,
    const bookmarks::BookmarkNode* folder,
    bookmarks::BookmarkModel* bookmark_model,
    ios::ChromeBrowserState* browser_state);

// Updates a bookmark node position, with undo toast.
void UpdateBookmarkPositionWithUndoToast(
    const bookmarks::BookmarkNode* node,
    const bookmarks::BookmarkNode* folder,
    int position,
    bookmarks::BookmarkModel* bookmark_model,
    ios::ChromeBrowserState* browser_state);

// Deletes all bookmarks in |model| that are in |bookmarks|, and presents a
// snackbar with an undo action.
void DeleteBookmarksWithUndoToast(
    const std::set<const bookmarks::BookmarkNode*>& bookmarks,
    bookmarks::BookmarkModel* model,
    ios::ChromeBrowserState* browser_state);

// Deletes all nodes in |bookmarks|.
void DeleteBookmarks(const std::set<const bookmarks::BookmarkNode*>& bookmarks,
                     bookmarks::BookmarkModel* model);

// Move all |bookmarks| to the given |folder|, and presents a snackbar with an
// undo action.
void MoveBookmarksWithUndoToast(
    const std::set<const bookmarks::BookmarkNode*>& bookmarks,
    bookmarks::BookmarkModel* model,
    const bookmarks::BookmarkNode* folder,
    ios::ChromeBrowserState* browser_state);

// Move all |bookmarks| to the given |folder|.
// Returns whether this method actually moved bookmarks (for example, only
// moving a folder to its parent will return |false|).
bool MoveBookmarks(const std::set<const bookmarks::BookmarkNode*>& bookmarks,
                   bookmarks::BookmarkModel* model,
                   const bookmarks::BookmarkNode* folder);

// Category name for all bookmarks related snackbars.
extern NSString* const kBookmarksSnackbarCategory;

// Returns the parent, if all the bookmarks are siblings.
// Otherwise returns the mobile_node.
const bookmarks::BookmarkNode* defaultMoveFolder(
    const std::set<const bookmarks::BookmarkNode*>& bookmarks,
    bookmarks::BookmarkModel* model);

#pragma mark - Segregation of nodes by time.

// A container for nodes which have been aggregated by some time property.
// e.g. (creation date) or (last access date).
class NodesSection {
 public:
  NodesSection();
  virtual ~NodesSection();

  // |vector| is sorted by the relevant time property.
  NodeVector vector;
  // A representative time of all nodes in vector.
  base::Time time;
  // A string representation of |time|.
  // e.g. (March 2014) or (4 March 2014).
  std::string timeRepresentation;
};

// Given the nodes in |vector|, segregates them by some time property into
// NodesSections.
// Automatically clears, populates and sorts |nodesSectionVector|.
// This method is not thread safe - it should only be called from the ui thread.
void segregateNodes(
    const NodeVector& vector,
    std::vector<std::unique_ptr<NodesSection>>& nodesSectionVector);

#pragma mark - Useful bookmark manipulation.

// Sorts a vector full of folders by title.
void SortFolders(NodeVector* vector);

// Returns a vector of root level folders and all their folder descendants,
// sorted depth-first, then alphabetically. The returned nodes are visible, and
// are guaranteed to not be descendants of any nodes in |obstructions|.
NodeVector VisibleNonDescendantNodes(const NodeSet& obstructions,
                                     bookmarks::BookmarkModel* model);

// Whether |vector1| contains only elements of |vector2| in the same order.
BOOL IsSubvectorOfNodes(const NodeVector& vector1, const NodeVector& vector2);

// Returns the indices in |vector2| of the items in |vector2| that are not
// present in |vector1|.
// |vector1| MUST be a subvector of |vector2| in the sense of |IsSubvector|.
std::vector<NodeVector::size_type> MissingNodesIndices(
    const NodeVector& vector1,
    const NodeVector& vector2);

#pragma mark - Cache position in collection view.

// Caches the active menu item, and the position in the collection view.
void CachePosition(CGFloat position, BookmarkMenuItem* item);
// Returns YES if a valid cache exists.
// |model| must be loaded.
// |item| and |position| are out variables, only populated if the return is YES.
BOOL GetPositionCache(bookmarks::BookmarkModel* model,
                      BookmarkMenuItem* __autoreleasing* item,
                      CGFloat* position);
// Method exists for testing.
void ClearPositionCache();

// Caches the bookmark UI position that the user was last viewing.
void CacheBookmarkUIPosition(BookmarkPathCache* cache);

// Returns the bookmark UI position that the user was last viewing.
BookmarkPathCache* GetBookmarkUIPositionCache(bookmarks::BookmarkModel* model);

// Creates bookmark path for |folderId| passed in. For eg: for folderId = 76,
// Root node(0) --> MobileBookmarks (3) --> Test1(76) will be returned as [0, 3,
// 76].
NSArray* CreateBookmarkPath(bookmarks::BookmarkModel* model, int64_t folderId);

// Clears the bookmark UI position cache.
void ClearBookmarkUIPositionCache();

}  // namespace bookmark_utils_ios

#endif  // IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_UTILS_IOS_H_
