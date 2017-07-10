// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bookmarks/bookmark_home_handset_view_controller.h"

#include <memory>

#include "base/logging.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/sys_string_conversions.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/strings/grit/components_strings.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "ios/chrome/browser/bookmarks/bookmark_model_factory.h"
#include "ios/chrome/browser/bookmarks/bookmarks_utils.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/ui/alert_coordinator/action_sheet_coordinator.h"
#import "ios/chrome/browser/ui/bookmarks/bars/bookmark_editing_bar.h"
#import "ios/chrome/browser/ui/bookmarks/bars/bookmark_navigation_bar.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_collection_cells.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_collection_view.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_edit_view_controller.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_folder_editor_view_controller.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_folder_view_controller.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_home_primary_view.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_home_waiting_view.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_menu_item.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_menu_view.h"
#include "ios/chrome/browser/ui/bookmarks/bookmark_model_bridge_observer.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_navigation_controller.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_panel_view.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_position_cache.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_promo_controller.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_utils_ios.h"
#include "ios/chrome/browser/ui/ui_util.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using bookmarks::BookmarkNode;

@interface BookmarkHomeHandsetViewController ()<
    BookmarkCollectionViewDelegate,
    BookmarkEditViewControllerDelegate,
    BookmarkFolderEditorViewControllerDelegate,
    BookmarkFolderViewControllerDelegate,
    BookmarkMenuViewDelegate,
    BookmarkModelBridgeObserver,
    BookmarkPanelViewDelegate,
    BookmarkPromoControllerDelegate> {
  // Bridge to register for bookmark changes.
  std::unique_ptr<bookmarks::BookmarkModelBridge> _bridge;
}

// This views holds the primary content of this view controller.
// Redefined to be readwrite.
@property(nonatomic, strong, readwrite) NSMutableArray* editIndexPaths;

// Returns the parent, if all the bookmarks are siblings.
// Otherwise returns the mobile_node.
+ (const BookmarkNode*)
defaultMoveFolderFromBookmarks:(const std::set<const BookmarkNode*>&)bookmarks
                         model:(bookmarks::BookmarkModel*)model;

// This views holds the primary content of this view controller.
@property(nonatomic, strong) UIView* contentView;

// When the view is first shown on the screen, this property represents the
// cached value of the y of the content offset of the primary view. This
// property is set to nil after it is used.
@property(nonatomic, strong) NSNumber* cachedContentPosition;

// The layout code in this class relies on the assumption that the editingBar
// has the same frame as the navigationBar.
@property(nonatomic, strong) BookmarkEditingBar* editingBar;

// The action sheet coordinator used when trying to edit a single bookmark.
@property(nonatomic, strong) ActionSheetCoordinator* actionSheetCoordinator;
// The view controller used to view and edit a single bookmark.
@property(nonatomic, strong) BookmarkEditViewController* editViewController;
// The view controller used to pick a folder in which to move the selected
// bookmarks.
@property(nonatomic, strong) BookmarkFolderViewController* folderSelector;
// The view controller to present when editing the current folder.
@property(nonatomic, strong) BookmarkFolderEditorViewController* folderEditor;
#pragma mark Specific to this class.

// The controller managing the display of the promo cell and the promo view
// controller.
@property(nonatomic, strong) BookmarkPromoController* bookmarkPromoController;

#pragma mark View loading and switching
// This method should be called at most once in the life-cycle of the
// class. It should be called at the soonest possible time after the
// view has been loaded, and the bookmark model is loaded.
- (void)loadBookmarkViews;
// Updates the property 'primaryMenuItem'.
// Updates the UI to reflect the new state of 'primaryMenuItem'.
- (void)updatePrimaryMenuItem:(BookmarkMenuItem*)menuItem
                     animated:(BOOL)animated;

#pragma mark Editing related methods
// This method statelessly updates the editing top bar from |_editNodes| and
// |editing|.
- (void)updateEditingStateAnimated:(BOOL)animated;
// Shows or hides the editing bar.
- (void)showEditingBarAnimated:(BOOL)animated;
- (void)hideEditingBarAnimated:(BOOL)animated;
// Instaneously updates the shadow of the edit bar.
// This method should be called anytime:
//  (1)|editing| property changes.
//  (2)The primary view changes.
//  (3)The primary view's collection view is scrolled.
// (2) is not necessary right now, as it is only possible to switch primary
// views when |editing| is NO. When |editing| is NO, the shadow is never shown.
- (void)updateEditBarShadow;

#pragma mark Editing bar callbacks
// The cancel button was tapped on the editing bar.
- (void)editingBarCancel;
// The move button was tapped on the editing bar.
- (void)editingBarMove;
// The delete button was tapped on the editing bar.
- (void)editingBarDelete;
// The edit button was tapped on the editing bar.
- (void)editingBarEdit;

#pragma mark Action sheet callbacks
// Enters into edit mode by selecting the given node and cell.
- (void)selectFirstNode:(const BookmarkNode*)node
               withCell:(UICollectionViewCell*)cell;
// Opens the editor on the given node with a transition from cell.
- (void)editNode:(const BookmarkNode*)node;
// Opens the folder move editor for the given node.
- (void)moveNodes:(const std::set<const BookmarkNode*>&)nodes;
// Deletes the current node.
- (void)deleteNodes:(const std::set<const BookmarkNode*>&)nodes;

#pragma mark private utility methods
// Deletes the nodes, and presents a toast with an undo button.
- (void)deleteSelectedNodes;

#pragma mark Navigation bar
// Updates the UI of the navigation bar with the primaryMenuItem.
// This method should be called anytime:
//  (1)The primary view changes.
//  (2)The menu is shown or hidden.
//  (3)The primary view has type folder, and the relevant folder has changed.
//  (4)The interface orientation changes.
//  (5)viewWillAppear, as the interface orientation may have changed.
- (void)updateNavigationBarAnimated:(BOOL)animated
                        orientation:(UIInterfaceOrientation)orientation;
- (void)updateNavigationBarWithDuration:(CGFloat)duration
                            orientation:(UIInterfaceOrientation)orientation;
// Whether the edit button on the navigation bar should be shown.
- (BOOL)shouldShowEditButtonWithMenuVisibility:(BOOL)visible;
// Whether the back button on the navigation bar should be preferred to the menu
// button.
- (BOOL)shouldShowBackButtonInsteadOfMenuButton;
// Called when the edit button is pressed on the navigation bar.
- (void)navigationBarWantsEditing:(id)sender;
// Called when the cancel button is pressed on the navigation bar.
- (void)navigationBarCancel:(id)sender;
// Called when the menu button is pressed on the navigation bar.
- (void)navigationBarToggledMenu:(id)sender;
// Called when the back button is pressed on the navigation bar.
- (void)navigationBarBack:(id)sender;

#pragma mark private methods
// Returns the size of the primary view.
- (CGRect)frameForPrimaryView;
// Updates the UI to reflect the given orientation, with an animation lasting
// |duration|.
- (void)updateUIForInterfaceOrientation:(UIInterfaceOrientation)orientation
                               duration:(NSTimeInterval)duration;
// Shows or hides the menu.
- (void)showMenuAnimated:(BOOL)animated;
- (void)hideMenuAnimated:(BOOL)animated updateNavigationBar:(BOOL)update;

// Saves the current position and asks the delegate to open the url.
- (void)delegateDismiss:(const GURL&)url;

- (NSIndexPath*)indexPathForCell:(UICollectionViewCell*)cell;

@end

@implementation BookmarkHomeHandsetViewController
@synthesize contentView = _contentView;

@synthesize cachedContentPosition = _cachedContentPosition;
@synthesize editingBar = _editingBar;

@synthesize actionSheetCoordinator = _actionSheetCoordinator;
@synthesize editViewController = _editViewController;
@synthesize folderSelector = _folderSelector;
@synthesize folderEditor = _folderEditor;

@synthesize bookmarkPromoController = _bookmarkPromoController;

@synthesize delegate = _delegate;
@synthesize editIndexPaths = _editIndexPaths;
@synthesize editing = _editing;

- (instancetype)initWithLoader:(id<UrlLoader>)loader
                  browserState:(ios::ChromeBrowserState*)browserState {
  self = [super initWithLoader:loader browserState:browserState];
  if (self) {
    _editIndexPaths = [[NSMutableArray alloc] init];

    [self resetEditNodes];

    _bridge.reset(new bookmarks::BookmarkModelBridge(self, self.bookmarks));
    // It is important to initialize the promo controller with the browser state
    // passed in, as it could be incognito.
    _bookmarkPromoController =
        [[BookmarkPromoController alloc] initWithBrowserState:browserState
                                                     delegate:self];
  }
  return self;
}

- (void)dealloc {
  _editViewController.delegate = nil;
  _folderSelector.delegate = nil;
}

- (void)resetEditNodes {
  _editNodes = std::set<const BookmarkNode*>();
  _editNodesOrdered = std::vector<const BookmarkNode*>();
  [self.editIndexPaths removeAllObjects];
}

- (void)insertEditNode:(const BookmarkNode*)node
           atIndexPath:(NSIndexPath*)indexPath {
  if (_editNodes.find(node) != _editNodes.end())
    return;
  _editNodes.insert(node);
  _editNodesOrdered.push_back(node);
  if (indexPath) {
    [self.editIndexPaths addObject:indexPath];
  } else {
    // We insert null if we don't have the cell to keep the index valid.
    [self.editIndexPaths addObject:[NSNull null]];
  }
}

- (void)removeEditNode:(const BookmarkNode*)node
           atIndexPath:(NSIndexPath*)indexPath {
  if (_editNodes.find(node) == _editNodes.end())
    return;

  _editNodes.erase(node);
  std::vector<const BookmarkNode*>::iterator it =
      std::find(_editNodesOrdered.begin(), _editNodesOrdered.end(), node);
  DCHECK(it != _editNodesOrdered.end());
  _editNodesOrdered.erase(it);
  if (indexPath) {
    [self.editIndexPaths removeObject:indexPath];
  } else {
    // If we don't have the cell, we remove it by using its index.
    const NSUInteger index = std::distance(_editNodesOrdered.begin(), it);
    if (index < self.editIndexPaths.count) {
      [self.editIndexPaths removeObjectAtIndex:index];
    }
  }
}

- (void)removeEditNode:(const BookmarkNode*)node
                  cell:(UICollectionViewCell*)cell {
  [self removeEditNode:node atIndexPath:[self indexPathForCell:cell]];

  if (_editNodes.size() == 0)
    [self setEditing:NO animated:YES];
  else
    [self updateEditingStateAnimated:YES];
}

#pragma mark - UIViewController methods

- (void)viewDidLoad {
  [super viewDidLoad];

  self.navigationBar.frame = [self navigationBarFrame];
  [self.navigationBar setEditTarget:self
                             action:@selector(navigationBarWantsEditing:)];
  [self.navigationBar setMenuTarget:self
                             action:@selector(navigationBarToggledMenu:)];
  [self.navigationBar setBackTarget:self action:@selector(navigationBarBack:)];
  [self.navigationBar setCancelTarget:self
                               action:@selector(navigationBarCancel:)];

  [self.view addSubview:self.navigationBar];
  [self.view bringSubviewToFront:self.navigationBar];

  if (self.bookmarks->loaded())
    [self loadBookmarkViews];
  else
    [self loadWaitingView];
}

- (BOOL)prefersStatusBarHidden {
  return NO;
}

#pragma mark - Accessibility

- (BOOL)accessibilityPerformEscape {
  [self delegateDismiss:GURL()];
  return YES;
}

#pragma mark - Methods duplicated from BookmarkHomeTabletNTPController.

- (void)loadBookmarkViews {
  [super loadBookmarkViews];
  DCHECK(self.bookmarks->loaded());
  DCHECK([self isViewLoaded]);

  self.folderView.delegate = self;
  [self.folderView setFrame:[self frameForPrimaryView]];

  [self.panelView setFrame:[self frameForPrimaryView]];
  self.panelView.delegate = self;
  [self.view insertSubview:self.panelView atIndex:0];

  self.contentView = [[UIView alloc] init];
  self.contentView.frame = self.panelView.contentView.bounds;
  self.contentView.autoresizingMask =
      UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
  [self.panelView.contentView addSubview:self.contentView];

  self.menuView.delegate = self;
  [self.panelView.menuView addSubview:self.menuView];

  // Load the last primary menu item which the user had active.
  BookmarkMenuItem* item = nil;
  CGFloat position = 0;
  BOOL found =
      bookmark_utils_ios::GetPositionCache(self.bookmarks, &item, &position);
  if (!found)
    item = [self.menuView defaultMenuItem];

  [self updatePrimaryMenuItem:item animated:NO];

  if (found) {
    // If the view has already been laid out, then immediately apply the content
    // position.
    if (self.view.window) {
      [[self primaryView] applyContentPosition:position];
    } else {
      // Otherwise, save the position to be applied once the view has been laid
      // out.
      self.cachedContentPosition = [NSNumber numberWithFloat:position];
    }
  }
}

- (void)updatePrimaryMenuItem:(BookmarkMenuItem*)menuItem
                     animated:(BOOL)animated {
  [super updatePrimaryMenuItem:menuItem];

  // Disable editing on previous primary view before dismissing it. No need to
  // animate because this view is immediately removed from hierarchy.
  if ([[self primaryMenuItem] supportsEditing])
    [self.primaryView setEditing:NO animated:NO];

  UIView* primaryView = [self primaryView];
  [self.contentView insertSubview:primaryView atIndex:0];
  primaryView.frame = self.contentView.bounds;

  [self updateNavigationBarAnimated:animated
                        orientation:GetInterfaceOrientation()];
}

#pragma mark - Editing bar methods.

- (void)updateEditingStateAnimated:(BOOL)animated {
  if (!self.editing) {
    [self hideEditingBarAnimated:animated];
    [self updateEditBarShadow];
    [self.panelView enableSideSwiping:YES];
    return;
  }

  if (!self.editingBar) {
    self.editingBar =
        [[BookmarkEditingBar alloc] initWithFrame:self.navigationBar.frame];
    [self.editingBar setCancelTarget:self action:@selector(editingBarCancel)];
    [self.editingBar setDeleteTarget:self action:@selector(editingBarDelete)];
    [self.editingBar setMoveTarget:self action:@selector(editingBarMove)];
    [self.editingBar setEditTarget:self action:@selector(editingBarEdit)];

    [self.view addSubview:self.editingBar];
    self.editingBar.alpha = 0;
  }

  int bookmarkCount = 0;
  int folderCount = 0;
  for (auto* node : _editNodes) {
    if (node->is_url())
      ++bookmarkCount;
    else
      ++folderCount;
  }
  [self.editingBar updateUIWithBookmarkCount:bookmarkCount
                                 folderCount:folderCount];

  [self showEditingBarAnimated:animated];
  [self updateEditBarShadow];
  [self.panelView enableSideSwiping:NO];
}

- (void)setEditing:(BOOL)editing animated:(BOOL)animated {
  if (_editing == editing)
    return;

  _editing = editing;

  // Only reset the editing state when leaving edit mode. This allows subclasses
  // to add nodes for editing before entering edit mode.
  if (!editing)
    [self resetEditNodes];

  [self updateEditingStateAnimated:animated];
  if ([[self primaryMenuItem] supportsEditing])
    [[self primaryView] setEditing:editing animated:animated];

  if (editing)
    self.bookmarkPromoController.promoState = NO;
  else
    [self.bookmarkPromoController updatePromoState];
}

- (void)showEditingBarAnimated:(BOOL)animated {
  CGRect frame = self.navigationBar.frame;
  self.editingBar.hidden = NO;
  if ([self respondsToSelector:@selector(setNeedsStatusBarAppearanceUpdate)])
    [self setNeedsStatusBarAppearanceUpdate];
  [UIView animateWithDuration:animated ? 0.2 : 0
      delay:0
      options:UIViewAnimationOptionBeginFromCurrentState
      animations:^{
        self.editingBar.alpha = 1;
        self.editingBar.frame = frame;
      }
      completion:^(BOOL finished) {
        if (finished) {
          self.navigationBar.hidden = YES;
        } else {
          if ([self respondsToSelector:@selector(
                                           setNeedsStatusBarAppearanceUpdate)])
            [self setNeedsStatusBarAppearanceUpdate];
        }
      }];
}

- (void)hideEditingBarAnimated:(BOOL)animated {
  CGRect frame = self.navigationBar.frame;
  self.navigationBar.hidden = NO;
  if ([self respondsToSelector:@selector(setNeedsStatusBarAppearanceUpdate)])
    [self setNeedsStatusBarAppearanceUpdate];
  [UIView animateWithDuration:animated ? 0.2 : 0
      delay:0
      options:UIViewAnimationOptionBeginFromCurrentState
      animations:^{
        self.editingBar.alpha = 0;
        self.editingBar.frame = frame;
      }
      completion:^(BOOL finished) {
        if (finished) {
          self.editingBar.hidden = YES;
        } else {
          if ([self respondsToSelector:@selector(
                                           setNeedsStatusBarAppearanceUpdate)])
            [self setNeedsStatusBarAppearanceUpdate];
        }
      }];
}

- (void)updateEditBarShadow {
  [self.editingBar showShadow:self.editing];
}

#pragma mark Editing Bar Callbacks

- (void)editingBarCancel {
  [self setEditing:NO animated:YES];
}

- (void)editingBarMove {
  [self moveNodes:_editNodes];
}

- (void)editingBarDelete {
  [self deleteSelectedNodes];
  [self setEditing:NO animated:YES];
}

- (void)editingBarEdit {
  DCHECK_EQ(_editNodes.size(), 1u);
  const BookmarkNode* node = *(_editNodes.begin());
  [self editNode:node];
}

#pragma mark - BookmarkMenuViewDelegate

- (void)bookmarkMenuView:(BookmarkMenuView*)view
        selectedMenuItem:(BookmarkMenuItem*)menuItem {
  BOOL menuItemChanged = ![[self primaryMenuItem] isEqual:menuItem];
  // If the primary menu item has changed, then updatePrimaryMenuItem: will
  // update the navigation bar, and there's no need for hideMenuAnimated: to do
  // so.
  [self hideMenuAnimated:YES updateNavigationBar:!menuItemChanged];
  if (menuItemChanged)
    [self updatePrimaryMenuItem:menuItem animated:YES];
}

#pragma mark - BookmarkCollectionViewDelegate
// This class owns multiple views that have a delegate that conforms to
// BookmarkCollectionViewDelegate, or a subprotocol of
// BookmarkCollectionViewDelegate.
- (void)bookmarkCollectionView:(BookmarkCollectionView*)view
                          cell:(UICollectionViewCell*)cell
             addNodeForEditing:(const BookmarkNode*)node {
  [self insertEditNode:node atIndexPath:[self indexPathForCell:cell]];
  [self updateEditingStateAnimated:YES];
}

- (void)bookmarkCollectionView:(BookmarkCollectionView*)view
                          cell:(UICollectionViewCell*)cell
          removeNodeForEditing:(const BookmarkNode*)node {
  [self removeEditNode:node atIndexPath:[self indexPathForCell:cell]];
  if (_editNodes.size() == 0)
    [self setEditing:NO animated:YES];
  else
    [self updateEditingStateAnimated:YES];
}

- (const std::set<const BookmarkNode*>&)nodesBeingEdited {
  return _editNodes;
}

- (void)bookmarkCollectionViewDidScroll:(BookmarkCollectionView*)view {
  [self updateEditBarShadow];
}

- (void)bookmarkCollectionView:(BookmarkCollectionView*)view
      selectedUrlForNavigation:(const GURL&)url {
  [self delegateDismiss:url];
}

- (void)bookmarkCollectionView:(BookmarkCollectionView*)collectionView
          wantsMenuForBookmark:(const BookmarkNode*)node
                        onView:(UIView*)view
                       forCell:(BookmarkItemCell*)cell {
  DCHECK(!self.editViewController);
  DCHECK(!self.actionSheetCoordinator);
  self.actionSheetCoordinator =
      [[ActionSheetCoordinator alloc] initWithBaseViewController:self
                                                           title:nil
                                                         message:nil
                                                            rect:CGRectZero
                                                            view:nil];
  __weak BookmarkHomeHandsetViewController* weakSelf = self;

  // Select action.
  [self.actionSheetCoordinator
      addItemWithTitle:l10n_util::GetNSString(IDS_IOS_BOOKMARK_ACTION_SELECT)
                action:^{
                  [weakSelf selectFirstNode:node withCell:cell];
                  weakSelf.actionSheetCoordinator = nil;
                }
                 style:UIAlertActionStyleDefault];

  // Edit action.
  [self.actionSheetCoordinator
      addItemWithTitle:l10n_util::GetNSString(IDS_IOS_BOOKMARK_ACTION_EDIT)
                action:^{
                  [weakSelf editNode:node];
                  weakSelf.actionSheetCoordinator = nil;
                }
                 style:UIAlertActionStyleDefault];

  // Move action.
  [self.actionSheetCoordinator
      addItemWithTitle:l10n_util::GetNSString(IDS_IOS_BOOKMARK_ACTION_MOVE)
                action:^{
                  std::set<const BookmarkNode*> nodes;
                  nodes.insert(node);
                  [weakSelf moveNodes:nodes];
                  weakSelf.actionSheetCoordinator = nil;
                }
                 style:UIAlertActionStyleDefault];

  // Delete action.
  [self.actionSheetCoordinator
      addItemWithTitle:l10n_util::GetNSString(IDS_IOS_BOOKMARK_ACTION_DELETE)
                action:^{
                  std::set<const BookmarkNode*> nodes;
                  nodes.insert(node);
                  [weakSelf deleteNodes:nodes];
                  weakSelf.actionSheetCoordinator = nil;
                }
                 style:UIAlertActionStyleDestructive];

  // Cancel action.
  [self.actionSheetCoordinator
      addItemWithTitle:l10n_util::GetNSString(IDS_CANCEL)
                action:^{
                  weakSelf.actionSheetCoordinator = nil;
                }
                 style:UIAlertActionStyleCancel];

  [self.actionSheetCoordinator start];
}

- (void)bookmarkCollectionView:(BookmarkCollectionView*)view
              didLongPressCell:(UICollectionViewCell*)cell
                   forBookmark:(const BookmarkNode*)node {
  [self selectFirstNode:node withCell:cell];
}

- (BOOL)bookmarkCollectionViewShouldShowPromoCell:
    (BookmarkCollectionView*)collectionView {
  return self.bookmarkPromoController.promoState;
}

- (void)bookmarkCollectionViewShowSignIn:(BookmarkCollectionView*)view {
  [self.bookmarkPromoController showSignIn];
}

- (void)bookmarkCollectionViewDismissPromo:(BookmarkCollectionView*)view {
  [self.bookmarkPromoController hidePromoCell];
}

#pragma mark Action Sheet Callbacks

- (void)selectFirstNode:(const BookmarkNode*)node
               withCell:(UICollectionViewCell*)cell {
  DCHECK(!self.editing);
  [self insertEditNode:node atIndexPath:[self indexPathForCell:cell]];
  [self setEditing:YES animated:YES];
}

- (void)editNode:(const BookmarkNode*)node {
  DCHECK(!self.editViewController);
  DCHECK(!self.folderEditor);
  UIViewController* editorController = nil;
  if (node->is_folder()) {
    BookmarkFolderEditorViewController* folderEditor =
        [BookmarkFolderEditorViewController
            folderEditorWithBookmarkModel:self.bookmarks
                                   folder:node
                             browserState:self.browserState];
    folderEditor.delegate = self;
    self.folderEditor = folderEditor;
    editorController = folderEditor;
  } else {
    BookmarkEditViewController* controller =
        [[BookmarkEditViewController alloc] initWithBookmark:node
                                                browserState:self.browserState];
    self.editViewController = controller;
    self.editViewController.delegate = self;
    editorController = self.editViewController;
  }
  DCHECK(editorController);
  UINavigationController* navController = [[BookmarkNavigationController alloc]
      initWithRootViewController:editorController];
  [navController setModalPresentationStyle:UIModalPresentationFormSheet];
  [self presentViewController:navController animated:YES completion:NULL];
}

- (void)moveNodes:(const std::set<const BookmarkNode*>&)nodes {
  DCHECK(!self.folderSelector);
  DCHECK(nodes.size() > 0);
  const BookmarkNode* editedNode = *(nodes.begin());
  const BookmarkNode* selectedFolder = editedNode->parent();
  self.folderSelector = [[BookmarkFolderViewController alloc]
      initWithBookmarkModel:self.bookmarks
           allowsNewFolders:YES
                editedNodes:nodes
               allowsCancel:YES
             selectedFolder:selectedFolder];
  self.folderSelector.delegate = self;
  UINavigationController* navController = [[BookmarkNavigationController alloc]
      initWithRootViewController:self.folderSelector];
  [navController setModalPresentationStyle:UIModalPresentationFormSheet];
  [self presentViewController:navController animated:YES completion:NULL];
}

- (void)deleteNodes:(const std::set<const BookmarkNode*>&)nodes {
  DCHECK_GE(nodes.size(), 1u);
  bookmark_utils_ios::DeleteBookmarksWithUndoToast(nodes, self.bookmarks,
                                                   self.browserState);
}

#pragma mark - BookmarkCollectionViewDelegate

- (void)bookmarkCollectionView:(BookmarkCollectionView*)view
    selectedFolderForNavigation:(const BookmarkNode*)folder {
  BookmarkMenuItem* menuItem = nil;
  if (view == self.folderView) {
    const BookmarkNode* parent = RootLevelFolderForNode(folder, self.bookmarks);
    menuItem =
        [BookmarkMenuItem folderMenuItemForNode:folder rootAncestor:parent];
  } else {
    NOTREACHED();
    return;
  }
  [self updatePrimaryMenuItem:menuItem animated:YES];
}

#pragma mark - BookmarkEditViewControllerDelegate

- (BOOL)bookmarkEditor:(BookmarkEditViewController*)controller
    shoudDeleteAllOccurencesOfBookmark:(const BookmarkNode*)bookmark {
  return NO;
}

- (void)bookmarkEditorWantsDismissal:(BookmarkEditViewController*)controller {
  self.editViewController.delegate = nil;
  self.editViewController = nil;
  [self dismissViewControllerAnimated:YES completion:NULL];

  // The editViewController can be displayed from the menu button, or from the
  // edit button in edit mode. Either way, after it's dismissed, edit mode
  // should be off.
  [self setEditing:NO animated:NO];
}

#pragma mark - BookmarkFolderViewControllerDelegate

- (void)folderPicker:(BookmarkFolderViewController*)folderPicker
    didFinishWithFolder:(const BookmarkNode*)folder {
  DCHECK(folder);
  DCHECK(!folder->is_url());
  DCHECK_GE(folderPicker.editedNodes.size(), 1u);

  bookmark_utils_ios::MoveBookmarksWithUndoToast(
      folderPicker.editedNodes, self.bookmarks, folder, self.browserState);

  [self setEditing:NO animated:NO];
  [self dismissViewControllerAnimated:YES completion:NULL];
  self.folderSelector.delegate = nil;
  self.folderSelector = nil;
}

- (void)folderPickerDidCancel:(BookmarkFolderViewController*)folderPicker {
  [self setEditing:NO animated:NO];
  [self dismissViewControllerAnimated:YES completion:NULL];
  self.folderSelector.delegate = nil;
  self.folderSelector = nil;
}

#pragma mark - BookmarkFolderEditorViewControllerDelegate

- (void)bookmarkFolderEditor:(BookmarkFolderEditorViewController*)folderEditor
      didFinishEditingFolder:(const BookmarkNode*)folder {
  DCHECK(folder);
  [self dismissViewControllerAnimated:YES completion:nil];
  self.folderEditor.delegate = nil;
  self.folderEditor = nil;
}

- (void)bookmarkFolderEditorDidDeleteEditedFolder:
    (BookmarkFolderEditorViewController*)folderEditor {
  [self dismissViewControllerAnimated:YES completion:nil];
  self.folderEditor.delegate = nil;
  self.folderEditor = nil;
}

- (void)bookmarkFolderEditorDidCancel:
    (BookmarkFolderEditorViewController*)folderEditor {
  [self dismissViewControllerAnimated:YES completion:nil];
  self.folderEditor.delegate = nil;
  self.folderEditor = nil;
}

#pragma mark - Internal Utility Methods

- (void)deleteSelectedNodes {
  [self deleteNodes:_editNodes];
}

#pragma mark - Navigation bar

- (CGRect)navigationBarFrame {
  return CGRectMake(0, 0, CGRectGetWidth(self.view.bounds),
                    [BookmarkNavigationBar expectedContentViewHeight] +
                        [self.topLayoutGuide length]);
}

- (void)updateNavigationBarAnimated:(BOOL)animated
                        orientation:(UIInterfaceOrientation)orientation {
  CGFloat duration = animated ? bookmark_utils_ios::menuAnimationDuration : 0;
  [self updateNavigationBarWithDuration:duration orientation:orientation];
}

- (void)updateNavigationBarWithDuration:(CGFloat)duration
                            orientation:(UIInterfaceOrientation)orientation {
  if ([self.panelView userDrivenAnimationInProgress])
    return;

  [self.navigationBar setTitle:[self.primaryMenuItem titleForNavigationBar]];
  BOOL isMenuVisible = self.panelView.showingMenu;
  if ([self shouldShowEditButtonWithMenuVisibility:isMenuVisible])
    [self.navigationBar showEditButtonWithAnimationDuration:duration];
  else
    [self.navigationBar hideEditButtonWithAnimationDuration:duration];

  if ([self shouldShowBackButtonInsteadOfMenuButton])
    [self.navigationBar showBackButtonInsteadOfMenuButton:duration];
  else
    [self.navigationBar showMenuButtonInsteadOfBackButton:duration];
}

- (BOOL)shouldShowEditButtonWithMenuVisibility:(BOOL)visible {
  if (visible)
    return NO;
  if (self.primaryMenuItem.type != bookmarks::MenuItemFolder)
    return NO;
  // The type is MenuItemFolder, so it is safe to access |folder|.
  return !self.bookmarks->is_permanent_node(self.primaryMenuItem.folder);
}

- (BOOL)shouldShowBackButtonInsteadOfMenuButton {
  if (self.primaryMenuItem.type != bookmarks::MenuItemFolder)
    return NO;
  // The type is MenuItemFolder, so it is safe to access |folder|.
  const BookmarkNode* folder = self.primaryMenuItem.folder;
  // Show the back button iff the folder or its immediate parent is a permanent
  // primary folder.
  BOOL isTopFolder = IsPrimaryPermanentNode(folder, self.bookmarks) ||
                     IsPrimaryPermanentNode(folder->parent(), self.bookmarks);
  return !isTopFolder;
}

#pragma mark Navigation Bar Callbacks

- (void)navigationBarWantsEditing:(id)sender {
  DCHECK(self.primaryMenuItem.type == bookmarks::MenuItemFolder);
  const BookmarkNode* folder = self.primaryMenuItem.folder;
  BookmarkFolderEditorViewController* folderEditor =
      [BookmarkFolderEditorViewController
          folderEditorWithBookmarkModel:self.bookmarks
                                 folder:folder
                           browserState:self.browserState];
  folderEditor.delegate = self;
  self.folderEditor = folderEditor;

  BookmarkNavigationController* navController =
      [[BookmarkNavigationController alloc]
          initWithRootViewController:self.folderEditor];
  [navController setModalPresentationStyle:UIModalPresentationFormSheet];
  [self presentViewController:navController animated:YES completion:NULL];
}

- (void)navigationBarCancel:(id)sender {
  [self delegateDismiss:GURL()];
}

- (void)navigationBarToggledMenu:(id)sender {
  if ([self.panelView userDrivenAnimationInProgress])
    return;

  if (self.panelView.showingMenu)
    [self hideMenuAnimated:YES updateNavigationBar:YES];
  else
    [self showMenuAnimated:YES];
}

- (void)navigationBarBack:(id)sender {
  DCHECK([self shouldShowBackButtonInsteadOfMenuButton]);

  // Go to the parent folder.
  DCHECK(self.primaryMenuItem.type == bookmarks::MenuItemFolder);
  const BookmarkNode* parentFolder = self.primaryMenuItem.folder->parent();
  const BookmarkNode* rootAncestor =
      RootLevelFolderForNode(parentFolder, self.bookmarks);
  BookmarkMenuItem* menuItem =
      [BookmarkMenuItem folderMenuItemForNode:parentFolder
                                 rootAncestor:rootAncestor];
  [self updatePrimaryMenuItem:menuItem animated:YES];
}

#pragma mark - Other internal methods.

- (CGRect)frameForPrimaryView {
  CGFloat margin;
  if (self.editing)
    margin = CGRectGetMaxY(self.editingBar.frame);
  else
    margin = CGRectGetMaxY(self.navigationBar.frame);
  CGFloat height = CGRectGetHeight(self.view.frame) - margin;
  CGFloat width = CGRectGetWidth(self.view.frame);
  return CGRectMake(0, margin, width, height);
}

- (void)updateUIForInterfaceOrientation:(UIInterfaceOrientation)orientation
                               duration:(NSTimeInterval)duration {
  [[self primaryView] changeOrientation:orientation];
  [self updateNavigationBarWithDuration:duration orientation:orientation];
}

- (void)showMenuAnimated:(BOOL)animated {
  [self.menuView setScrollsToTop:YES];
  [[self primaryView] setScrollsToTop:NO];
  self.scrollToTop = YES;
  [self.panelView showMenuAnimated:animated];
  [self updateNavigationBarAnimated:animated
                        orientation:GetInterfaceOrientation()];
}

- (void)hideMenuAnimated:(BOOL)animated updateNavigationBar:(BOOL)update {
  [self.menuView setScrollsToTop:NO];
  [[self primaryView] setScrollsToTop:YES];
  self.scrollToTop = NO;
  [self.panelView hideMenuAnimated:animated];
  if (update) {
    UIInterfaceOrientation orient = GetInterfaceOrientation();
    [self updateNavigationBarAnimated:animated orientation:orient];
  }
}

- (void)dismissModals:(BOOL)animated {
  [self.actionSheetCoordinator stop];
  self.actionSheetCoordinator = nil;
}

#pragma mark - BookmarkPanelViewDelegate

- (void)bookmarkPanelView:(BookmarkPanelView*)view
             willShowMenu:(BOOL)showMenu
    withAnimationDuration:(CGFloat)duration {
  if (showMenu) {
    [self.menuView setScrollsToTop:YES];
    [[self primaryView] setScrollsToTop:NO];
    self.scrollToTop = YES;
  } else {
    [self.menuView setScrollsToTop:NO];
    [[self primaryView] setScrollsToTop:YES];
    self.scrollToTop = NO;
  }

  if ([self shouldShowEditButtonWithMenuVisibility:showMenu])
    [self.navigationBar showEditButtonWithAnimationDuration:duration];
  else
    [self.navigationBar hideEditButtonWithAnimationDuration:duration];
}

- (void)bookmarkPanelView:(BookmarkPanelView*)view
    updatedMenuVisibility:(CGFloat)visibility {
  // As the menu becomes more visible, the edit button will always fade out.
  if ([self shouldShowEditButtonWithMenuVisibility:NO])
    [self.navigationBar updateEditButtonVisibility:1 - visibility];
  else
    [self.navigationBar updateEditButtonVisibility:0];
}

#pragma mark - BookmarkModelBridgeObserver

- (void)bookmarkModelLoaded {
  if (![self isViewLoaded])
    return;

  DCHECK(self.waitForModelView);
  __weak BookmarkHomeHandsetViewController* weakSelf = self;
  [self.waitForModelView stopWaitingWithCompletion:^{
    BookmarkHomeHandsetViewController* strongSelf = weakSelf;
    // Early return if the controller has been deallocated.
    if (!strongSelf)
      return;
    [UIView animateWithDuration:0.2
        animations:^{
          strongSelf.waitForModelView.alpha = 0.0;
        }
        completion:^(BOOL finished) {
          [strongSelf.waitForModelView removeFromSuperview];
          strongSelf.waitForModelView = nil;
        }];
    [strongSelf loadBookmarkViews];
  }];
}

- (void)bookmarkNodeChanged:(const BookmarkNode*)node {
  // The title of the folder may have changed.
  if (self.primaryMenuItem.type == bookmarks::MenuItemFolder &&
      self.primaryMenuItem.folder == node) {
    UIInterfaceOrientation orient = GetInterfaceOrientation();
    [self updateNavigationBarAnimated:NO orientation:orient];
  }
}

- (void)bookmarkNodeChildrenChanged:(const BookmarkNode*)bookmarkNode {
  // The node has not changed, but the ordering and existence of its children
  // have changed.
}

- (void)bookmarkNode:(const BookmarkNode*)bookmarkNode
     movedFromParent:(const BookmarkNode*)oldParent
            toParent:(const BookmarkNode*)newParent {
  // The node has moved to a new parent folder.
}

- (void)bookmarkNodeDeleted:(const BookmarkNode*)node
                 fromFolder:(const BookmarkNode*)folder {
  [self removeEditNode:node atIndexPath:nil];
}

- (void)bookmarkModelRemovedAllNodes {
  // All non-permanent nodes have been removed.
  [self setEditing:NO animated:YES];
}

- (UIStatusBarStyle)preferredStatusBarStyle {
  return self.editing ? UIStatusBarStyleLightContent : UIStatusBarStyleDefault;
}

// There are 2 UIViewController methods that could be overridden. This one and
// willAnimateRotationToInterfaceOrientation:duration:. The latter is called
// during an "animation block", and its unclear that reloading a collection view
// will always work duratin an "animation block", although it appears to work at
// first glance.
- (void)willRotateToInterfaceOrientation:(UIInterfaceOrientation)orientation
                                duration:(NSTimeInterval)duration {
  [self updateUIForInterfaceOrientation:orientation duration:duration];
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];
  UIInterfaceOrientation orient = GetInterfaceOrientation();
  [self updateUIForInterfaceOrientation:orient duration:0];
  if (self.cachedContentPosition) {
    [[self primaryView]
        applyContentPosition:[self.cachedContentPosition floatValue]];
    self.cachedContentPosition = nil;
  }
}

- (void)viewWillLayoutSubviews {
  [super viewWillLayoutSubviews];
  // Invalidate the layout of the collection view, as its frame might have
  // changed. Normally, this can be done automatically when the collection view
  // layout returns YES to -shouldInvalidateLayoutForBoundsChange:.
  // Unfortunately, it doesn't happen for all bounds changes. E.g. on iPhone 6
  // Plus landscape, the width of the collection is too large when created, then
  // resized down before being presented. Yet, the bounds change doesn't yield a
  // call to the flow layout, thus not invalidating the layout correctly.
  [[self primaryView].collectionView.collectionViewLayout invalidateLayout];

  self.navigationBar.frame = [self navigationBarFrame];
  self.editingBar.frame = [self navigationBarFrame];
  [self.panelView setFrame:[self frameForPrimaryView]];
}

#pragma mark - Internal non-UI Methods

- (void)delegateDismiss:(const GURL&)url {
  if ([self primaryView]) {
    bookmark_utils_ios::CachePosition(
        [[self primaryView] contentPositionInPortraitOrientation],
        [self primaryMenuItem]);
  }
  [self.delegate bookmarkHomeHandsetViewControllerWantsDismissal:self
                                                 navigationToUrl:url];
}

#pragma mark - BookmarkPromoControllerDelegate

- (void)promoStateChanged:(BOOL)promoEnabled {
  [self.folderView
      promoStateChangedAnimated:self.folderView == [self primaryView]];
}

- (NSIndexPath*)indexPathForCell:(UICollectionViewCell*)cell {
  DCHECK([self primaryView].collectionView);
  NSIndexPath* indexPath =
      [[self primaryView].collectionView indexPathForCell:cell];
  return indexPath;
}

+ (const BookmarkNode*)
defaultMoveFolderFromBookmarks:(const std::set<const BookmarkNode*>&)bookmarks
                         model:(bookmarks::BookmarkModel*)model {
  if (bookmarks.size() == 0)
    return model->mobile_node();
  const BookmarkNode* firstParent = (*(bookmarks.begin()))->parent();
  for (const BookmarkNode* node : bookmarks) {
    if (node->parent() != firstParent)
      return model->mobile_node();
  }

  return firstParent;
}

@end

@implementation BookmarkHomeHandsetViewController (ExposedForTesting)

- (void)ensureAllViewExists {
  // Do nothing.
}

- (const std::set<const BookmarkNode*>&)editNodes {
  return _editNodes;
}

- (void)setEditNodes:(const std::set<const BookmarkNode*>&)editNodes {
  _editNodes = editNodes;
}

@end
