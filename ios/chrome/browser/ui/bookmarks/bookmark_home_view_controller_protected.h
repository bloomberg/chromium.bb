// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BOOKMARKS_HOME_VIEW_CONTROLLER_PROTECTED_H_
#define IOS_CHROME_BROWSER_UI_BOOKMARKS_HOME_VIEW_CONTROLLER_PROTECTED_H_

namespace bookmarks {
class BookmarkModel;
class BookmarkNode;
}  // namespace bookmarks

@class BookmarkContextBar;
@class BookmarkEditViewController;
@class BookmarkFolderEditorViewController;
@class BookmarkFolderViewController;
@class BookmarkHomeWaitingView;
@class BookmarkNavigationBar;
@class BookmarkPromoController;
@class BookmarkTableView;
@class MDCAppBar;

typedef NS_ENUM(NSInteger, BookmarksContextBarState) {
  BookmarksContextBarNone,            // No state.
  BookmarksContextBarDefault,         // No selection is possible in this state.
  BookmarksContextBarBeginSelection,  // This is the clean start state,
                                      // selection is possible, but nothing is
                                      // selected yet.
  BookmarksContextBarSingleURLSelection,       // Single URL selected state.
  BookmarksContextBarMultipleURLSelection,     // Multiple URLs selected state.
  BookmarksContextBarSingleFolderSelection,    // Single folder selected.
  BookmarksContextBarMultipleFolderSelection,  // Multiple folders selected.
  BookmarksContextBarMixedSelection,  // Multiple URL / Folders selected.
};

// TODO(crbug.com/753599) : Merge this file back to BookmarkHomeViewController.

// BookmarkHomeViewController class extension for protected read/write
// properties and methods for subclasses.
@interface BookmarkHomeViewController ()

// The bookmark model used.
@property(nonatomic, assign) bookmarks::BookmarkModel* bookmarks;

// The user's browser state model used.
@property(nonatomic, assign) ios::ChromeBrowserState* browserState;

// The main view showing all the bookmarks.
@property(nonatomic, strong) BookmarkTableView* bookmarksTableView;

// The view controller used to pick a folder in which to move the selected
// bookmarks.
@property(nonatomic, strong) BookmarkFolderViewController* folderSelector;

// Object to load URLs.
@property(nonatomic, weak) id<UrlLoader> loader;

// The app bar for the bookmarks.
@property(nonatomic, strong) MDCAppBar* appBar;

// The context bar at the bottom of the bookmarks.
@property(nonatomic, strong) BookmarkContextBar* contextBar;

// This view is created and used if the model is not fully loaded yet by the
// time this controller starts.
@property(nonatomic, strong) BookmarkHomeWaitingView* waitForModelView;

// The view controller used to view and edit a single bookmark.
@property(nonatomic, strong) BookmarkEditViewController* editViewController;

// The view controller to present when editing the current folder.
@property(nonatomic, strong) BookmarkFolderEditorViewController* folderEditor;

// The controller managing the display of the promo cell and the promo view
// controller.
@property(nonatomic, strong) BookmarkPromoController* bookmarkPromoController;

// The current state of the context bar UI.
@property(nonatomic, assign) BookmarksContextBarState contextBarState;

// When the view is first shown on the screen, this property represents the
// cached value of the y of the content offset of the table view. This
// property is set to nil after it is used.
@property(nonatomic, strong) NSNumber* cachedContentPosition;

// This method should be called at most once in the life-cycle of the class.
// It should be called at the soonest possible time after the view has been
// loaded, and the bookmark model is loaded.
- (void)loadBookmarkViews;

// This method is called if the view needs to be loaded and the model is not
// ready yet.
- (void)loadWaitingView;

// Caches the position in the table view.
- (void)cachePosition;

#pragma mark - Navigation bar.

// Callback for when navigation bar is cancelled.
- (void)navigationBarCancel:(id)sender;

@end

#endif  // IOS_CHROME_BROWSER_UI_BOOKMARKS_HOME_VIEW_CONTROLLER_PROTECTED_H_
