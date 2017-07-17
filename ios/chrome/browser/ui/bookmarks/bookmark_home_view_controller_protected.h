// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BOOKMARKS_HOME_VIEW_CONTROLLER_PROTECTED_H_
#define IOS_CHROME_BROWSER_UI_BOOKMARKS_HOME_VIEW_CONTROLLER_PROTECTED_H_

@protocol BookmarkHomePrimaryView;

namespace bookmarks {
class BookmarkModel;
class BookmarkNode;
}  // namespace bookmarks

@class BookmarkCollectionView;
@class BookmarkEditingBar;
@class BookmarkEditViewController;
@class BookmarkFolderEditorViewController;
@class BookmarkFolderViewController;
@class BookmarkHomeWaitingView;
@class BookmarkMenuItem;
@class BookmarkMenuView;
@class BookmarkNavigationBar;
@class BookmarkPanelView;
@class BookmarkPromoController;

// BookmarkHomeViewController class extention for protected read/write
// properties and methods for subclasses.
@interface BookmarkHomeViewController ()

// The bookmark model used.
@property(nonatomic, assign) bookmarks::BookmarkModel* bookmarks;

// The user's browser state model used.
@property(nonatomic, assign) ios::ChromeBrowserState* browserState;

// The main view showing all the bookmarks.
@property(nonatomic, strong) BookmarkCollectionView* folderView;

// The view controller used to pick a folder in which to move the selected
// bookmarks.
@property(nonatomic, strong) BookmarkFolderViewController* folderSelector;

// Object to load URLs.
@property(nonatomic, weak) id<UrlLoader> loader;

// The menu with all the folders.
@property(nonatomic, strong) BookmarkMenuView* menuView;

// The navigation bar sits on top of the main content.
@property(nonatomic, strong) BookmarkNavigationBar* navigationBar;

// At any point in time, there is exactly one collection view whose view is part
// of the view hierarchy. This property determines what data is visible in the
// collection view.
@property(nonatomic, strong) BookmarkMenuItem* primaryMenuItem;

// This view holds a content view, and a menu view.
@property(nonatomic, strong) BookmarkPanelView* panelView;

// Either the menu or the primaryView can scrollToTop.
@property(nonatomic, assign) BOOL scrollToTop;

// This view is created and used if the model is not fully loaded yet by the
// time this controller starts.
@property(nonatomic, strong) BookmarkHomeWaitingView* waitForModelView;

// The view controller used to view and edit a single bookmark.
@property(nonatomic, strong) BookmarkEditViewController* editViewController;

// Whether the view controller is in editing mode.
@property(nonatomic, assign) BOOL editing;

// The set of selected index paths for edition.
@property(nonatomic, strong) NSMutableArray* editIndexPaths;

// The layout code in this class relies on the assumption that the editingBar
// has the same frame as the navigationBar.
@property(nonatomic, strong) BookmarkEditingBar* editingBar;

// The view controller to present when editing the current folder.
@property(nonatomic, strong) BookmarkFolderEditorViewController* folderEditor;

// The controller managing the display of the promo cell and the promo view
// controller.
@property(nonatomic, strong) BookmarkPromoController* bookmarkPromoController;

// Whether the panel view can be brought into view and hidden by swipe gesture.
@property(nonatomic, assign) BOOL sideSwipingPossible;

// This method should be called at most once in the life-cycle of the class.
// It should be called at the soonest possible time after the view has been
// loaded, and the bookmark model is loaded.
- (void)loadBookmarkViews;

// Returns the width of the menu.
- (CGFloat)menuWidth;

// This method is called if the view needs to be loaded and the model is not
// ready yet.
- (void)loadWaitingView;

// Updates the property 'primaryMenuItem'.
// Updates the UI to reflect the new state of 'primaryMenuItem'.
- (void)updatePrimaryMenuItem:(BookmarkMenuItem*)menuItem;

// The active collection view that corresponds to primaryMenuItem.
- (UIView<BookmarkHomePrimaryView>*)primaryView;

// Returns NSIndexPath for a given cell.
- (NSIndexPath*)indexPathForCell:(UICollectionViewCell*)cell;

#pragma mark - Navigation bar callbacks.

// Called when the edit button is pressed on the navigation bar.
- (void)navigationBarWantsEditing:(id)sender;

#pragma mark - Action sheet callbacks

// Enters into edit mode by selecting the given node corresponding to the
// given cell.
- (void)selectFirstNode:(const bookmarks::BookmarkNode*)node
               withCell:(UICollectionViewCell*)cell;

// Opens the folder move editor for the given node.
- (void)moveNodes:(const std::set<const bookmarks::BookmarkNode*>&)nodes;

// Deletes the current node.
- (void)deleteNodes:(const std::set<const bookmarks::BookmarkNode*>&)nodes;

// Opens the editor on the given node.
- (void)editNode:(const bookmarks::BookmarkNode*)node;

#pragma mark - Edit

// Replaces |_editNodes| and |_editNodesOrdered| with new container objects.
- (void)resetEditNodes;

// Adds |node| corresponding to a |cell| if it isn't already present.
- (void)insertEditNode:(const bookmarks::BookmarkNode*)node
           atIndexPath:(NSIndexPath*)indexPath;

// Removes |node| corresponding to a |cell| if it's present.
- (void)removeEditNode:(const bookmarks::BookmarkNode*)node
           atIndexPath:(NSIndexPath*)indexPath;

// This method updates the property, and resets the edit nodes.
- (void)setEditing:(BOOL)editing animated:(BOOL)animated;

// This method statelessly updates the editing top bar from |_editNodes| and
// |editing|.
- (void)updateEditingStateAnimated:(BOOL)animated;

// Shows the editing bar, this method MUST be overridden by subclass to
// tailor the behaviour according to device.
- (void)showEditingBarAnimated:(BOOL)animated;

// Hides the editing bar, this method MUST be overridden by subclass to
// tailor the behaviour according to device.
- (void)hideEditingBarAnimated:(BOOL)animated;

// Returns the frame for editingBar, MUST be overridden by subclass.
- (CGRect)editingBarFrame;

// Instaneously updates the shadow of the edit bar.
// This method should be called anytime:
//  (1)|editing| property changes.
//  (2)The primary view changes.
//  (3)The primary view's collection view is scrolled.
// (2) is not necessary right now, as it is only possible to switch primary
// views when |editing| is NO. When |editing| is NO, the shadow is never shown.
- (void)updateEditBarShadow;

#pragma mark - Editing bar callbacks
// The cancel button was tapped on the editing bar.
- (void)editingBarCancel;
// The move button was tapped on the editing bar.
- (void)editingBarMove;
// The delete button was tapped on the editing bar.
- (void)editingBarDelete;
// The edit button was tapped on the editing bar.
- (void)editingBarEdit;

@end

#endif  // IOS_CHROME_BROWSER_UI_BOOKMARKS_HOME_VIEW_CONTROLLER_PROTECTED_H_
