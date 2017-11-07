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

// TODO(crbug.com/753599) : Delete this file after new bookmarks ui is launched.

@class BookmarkCollectionView;
class GURL;
@protocol SigninPresenter;
@protocol UrlLoader;

namespace bookmarks {
class BookmarkNode;
}  // namespace bookmarks
namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

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

// Tells the delegate that a folder was selected for navigation.
- (void)bookmarkCollectionView:(BookmarkCollectionView*)view
    selectedFolderForNavigation:(const bookmarks::BookmarkNode*)folder;
@end

// Shows all sub-folders and sub-urls of a folder node in a collection view.
// Note: This class intentionally does not try to maintain state through a
// folder transition. Depending on the type of animation that the designers
// choose, we may require multiple instances of this view.
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

// Registers the feature preferences.
+ (void)registerBrowserStatePrefs:(user_prefs::PrefRegistrySyncable*)registry;

// Designated initializer.
- (instancetype)initWithBrowserState:(ios::ChromeBrowserState*)browserState
                               frame:(CGRect)frame
                           presenter:(id<SigninPresenter>)presenter;

// Callback whenever the collection view is scrolled.
- (void)collectionViewScrolled;

// Refreshes the entire view to reflect |folder|.
- (void)resetFolder:(const bookmarks::BookmarkNode*)folder;

// Called when something outside the view causes the promo state to change.
- (void)promoStateChangedAnimated:(BOOL)animated;

@property(nonatomic, assign, readonly) bookmarks::BookmarkModel* bookmarkModel;
@property(nonatomic, weak, readonly) id<UrlLoader> loader;
@property(nonatomic, assign, readonly) ios::ChromeBrowserState* browserState;
@property(nonatomic, weak) id<BookmarkCollectionViewDelegate> delegate;
@property(nonatomic, assign, readonly) const bookmarks::BookmarkNode* folder;

// Called when the bookmark view becomes visible.
- (void)wasShown;

// Called when the bookmark view becomes hidden.
- (void)wasHidden;

#pragma mark - Editing

@property(nonatomic, assign, readonly) BOOL editing;

#pragma mark - Promo Cell

// Return true if the section at the given index is a promo section.
- (BOOL)isPromoSection:(NSInteger)section;
- (BOOL)shouldShowPromoCell;
- (BOOL)isPromoActive;

@end

#endif  // IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_COLLECTION_VIEW_H_
