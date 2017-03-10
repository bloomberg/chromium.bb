// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_COLLECTION_VIEW_H_
#define IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_COLLECTION_VIEW_H_

#import <UIKit/UIKit.h>

#include <set>
#include <vector>

#import "ios/chrome/browser/ui/bookmarks/bookmark_collection_cells.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_home_primary_view.h"
#include "ios/chrome/browser/ui/bookmarks/bookmark_model_bridge_observer.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_utils_ios.h"

@class BookmarkCollectionView;
class GURL;
@protocol UrlLoader;

namespace bookmarks {
class BookmarkModel;
class BookmarkNode;
}  // namespace bookmarks

namespace ios {
class ChromeBrowserState;
}  // namespace ios

// This protocol is provided for subclasses, and is not used in this class.
@protocol BookmarkCollectionViewDelegate<NSObject>

// This method tells the delegate to add the node and cell
// to the list of those being edited.
- (void)bookmarkCollectionView:(BookmarkCollectionView*)view
                          cell:(UICollectionViewCell*)cell
             addNodeForEditing:(const bookmarks::BookmarkNode*)node;

// This method tells the delegate to remove the node and cell from the list of
// those being edited.
- (void)bookmarkCollectionView:(BookmarkCollectionView*)view
                          cell:(UICollectionViewCell*)cell
          removeNodeForEditing:(const bookmarks::BookmarkNode*)node;

// The delegate is responsible for keeping track of the nodes being edited.
- (const std::set<const bookmarks::BookmarkNode*>&)nodesBeingEdited;

// This method tells the delegate that the collection view was scrolled.
- (void)bookmarkCollectionViewDidScroll:(BookmarkCollectionView*)view;
- (void)bookmarkCollectionView:(BookmarkCollectionView*)view
      selectedUrlForNavigation:(const GURL&)url;

// The user selected the menu button at the index path.
- (void)bookmarkCollectionView:(BookmarkCollectionView*)view
          wantsMenuForBookmark:(const bookmarks::BookmarkNode*)node
                        onView:(UIView*)view
                       forCell:(BookmarkItemCell*)cell;

// The user performed a long press on the bookmark.
- (void)bookmarkCollectionView:(BookmarkCollectionView*)view
              didLongPressCell:(UICollectionViewCell*)cell
                   forBookmark:(const bookmarks::BookmarkNode*)node;

// Returns true if a bookmarks promo cell should be shown.
- (BOOL)bookmarkCollectionViewShouldShowPromoCell:(BookmarkCollectionView*)view;

// Shows a sign-in view controller.
- (void)bookmarkCollectionViewShowSignIn:(BookmarkCollectionView*)view;

// Dismisses the promo.
- (void)bookmarkCollectionViewDismissPromo:(BookmarkCollectionView*)view;

@end

// This is an abstract class.
// It contains a collection view specific to bookmarks.
// This class is responsible for the UI of the collection view.
// Subclasses are responsible for handling the model layer.
//
// Note about the implementation of the |BookmarkHomePrimaryView| in this class:
// * |contentPositionInPortraitOrientation|: Regardless of the current
//       orientation, returns the y of the content offset of the collection view
//       if it were to have portrait orientation.
// * |applyContentPosition:|: Given a content position from portrait
//       orientation, change the content offset of the collection view to match
//       that position.
// * |changeOrientation:|: Calls |updateCollectionView|.
// * |setScrollsToTop:|: Applies |scrollsToTop| to the collection view.
// * |setEditing:animated:|: This method updates the editing property, but has
//       no other effect. Subclasses must provide the actual functionality.
@interface BookmarkCollectionView
    : UIView<BookmarkHomePrimaryView, BookmarkModelBridgeObserver>

// Designated initializer.
- (instancetype)initWithBrowserState:(ios::ChromeBrowserState*)browserState
                               frame:(CGRect)frame;

#pragma mark - Methods that subclasses can override

// Callback whenever the collection view is scrolled.
- (void)collectionViewScrolled;

#pragma mark - Methods that subclasses must override (non-UI)

// BookmarkModelBridgeObserver Callbacks
// Instances of this class automatically observe the bookmark model.
// The bookmark model has loaded.
- (void)bookmarkModelLoaded;
// The node has changed, but not its children.
- (void)bookmarkNodeChanged:(const bookmarks::BookmarkNode*)bookmarkNode;
// The node has not changed, but its children have.
- (void)bookmarkNodeChildrenChanged:
    (const bookmarks::BookmarkNode*)bookmarkNode;
// The node has moved to a new parent folder.
- (void)bookmarkNode:(const bookmarks::BookmarkNode*)bookmarkNode
     movedFromParent:(const bookmarks::BookmarkNode*)oldParent
            toParent:(const bookmarks::BookmarkNode*)newParent;
// |node| was deleted from |folder|.
- (void)bookmarkNodeDeleted:(const bookmarks::BookmarkNode*)node
                 fromFolder:(const bookmarks::BookmarkNode*)folder;
// All non-permanent nodes have been removed.
- (void)bookmarkModelRemovedAllNodes;

// Called when a user is attempting to select a cell.
// Returning NO prevents the cell from being selected.
- (BOOL)shouldSelectCellAtIndexPath:(NSIndexPath*)indexPath;
// Called when a cell is tapped outside of editing mode.
- (void)didTapCellAtIndexPath:(NSIndexPath*)indexPath;
// Called when a user selected a cell in the editing state.
- (void)didAddCellForEditingAtIndexPath:(NSIndexPath*)indexPath;
- (void)didRemoveCellForEditingAtIndexPath:(NSIndexPath*)indexPath;
// Called when a user taps the menu button on a cell.
- (void)didTapMenuButtonAtIndexPath:(NSIndexPath*)indexPath
                             onView:(UIView*)view
                            forCell:(BookmarkItemCell*)cell;

// Whether a cell should show a button and of which type.
- (bookmark_cell::ButtonType)buttonTypeForCellAtIndexPath:
    (NSIndexPath*)indexPath;

// Whether a long press at the cell at |indexPath| should be allowed.
- (BOOL)allowLongPressForCellAtIndexPath:(NSIndexPath*)indexPath;
// The |cell| at |indexPath| received a long press.
- (void)didLongPressCell:(UICollectionViewCell*)cell
             atIndexPath:(NSIndexPath*)indexPath;

// Whether the cell has been selected in editing mode.
- (BOOL)cellIsSelectedForEditingAtIndexPath:(NSIndexPath*)indexPath;

// Updates the collection view based on the current state of all models and
// contextual parameters, such as the interface orientation.
- (void)updateCollectionView;

// Returns the bookmark node associated with |indexPath|.
- (const bookmarks::BookmarkNode*)nodeAtIndexPath:(NSIndexPath*)indexPath;

#pragma mark - Methods that subclasses must override (UI)

// The size of the header for |section|. A return value of CGSizeZero prevents
// a header from showing.
- (CGSize)headerSizeForSection:(NSInteger)section;
// Create a cell for display at |indexPath|.
- (UICollectionViewCell*)cellAtIndexPath:(NSIndexPath*)indexPath;
// Create a header view for the element at |indexPath|.
- (UICollectionReusableView*)headerAtIndexPath:(NSIndexPath*)indexPath;
- (NSInteger)numberOfItemsInSection:(NSInteger)section;
- (NSInteger)numberOfSections;

#pragma mark - Methods that subclasses can override (UI)

// The inset of the section.
- (UIEdgeInsets)insetForSectionAtIndex:(NSInteger)section;
// The size of the cell at |indexPath|.
- (CGSize)cellSizeForIndexPath:(NSIndexPath*)indexPath;
// The minimal horizontal space between items to respect between cells in
// |section|.
- (CGFloat)minimumInteritemSpacingForSectionAtIndex:(NSInteger)section;
// The minimal vertical space between items to respect between cells in
// |section|.
- (CGFloat)minimumLineSpacingForSectionAtIndex:(NSInteger)section;
// The text to display when there are no items in the collection. Default is
// |IDS_IOS_BOOKMARK_NO_BOOKMARKS_LABEL|.
- (NSString*)textWhenCollectionIsEmpty;

#pragma mark - Convenience methods for subclasses

- (BookmarkItemCell*)cellForBookmark:(const bookmarks::BookmarkNode*)node
                           indexPath:(NSIndexPath*)indexPath;
- (BookmarkFolderCell*)cellForFolder:(const bookmarks::BookmarkNode*)node
                           indexPath:(NSIndexPath*)indexPath;

// |animateMenuVisibility| refers to whether the change in the visibility of the
// menu button is animated.
// |animateSelectedState| refers to whether the change in the selected state (in
// editing mode) of the cell is animated.
// This method updates the visibility of the menu button.
// This method updates the selected state of the cell (in editing mode).
- (void)updateEditingStateOfCellAtIndexPath:(NSIndexPath*)indexPath
                      animateMenuVisibility:(BOOL)animateMenuVisibility
                       animateSelectedState:(BOOL)animateSelectedState;

// Cancels all async loads of favicons. Subclasses should call this method when
// the bookmark model is going through significant changes, then manually call
// loadFaviconAtIndexPath: for everything that needs to be loaded; or
// just reload relevant cells.
- (void)cancelAllFaviconLoads;

// Asynchronously loads favicon for given index path. The loads are cancelled
// upon cell reuse automatically.
- (void)loadFaviconAtIndexPath:(NSIndexPath*)indexPath;

#pragma mark - Commonly used properties

@property(nonatomic, assign, readonly) bookmarks::BookmarkModel* bookmarkModel;
@property(nonatomic, assign, readonly) id<UrlLoader> loader;
@property(nonatomic, assign, readonly) ios::ChromeBrowserState* browserState;

#pragma mark - Editing

@property(nonatomic, assign, readonly) BOOL editing;

#pragma mark - Promo Cell

// Return true if the section at the given index is a promo section.
- (BOOL)isPromoSection:(NSInteger)section;
- (BOOL)shouldShowPromoCell;
- (BOOL)isPromoActive;

@end

#endif  // IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_COLLECTION_VIEW_H_
