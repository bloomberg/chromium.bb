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
#import "ios/chrome/browser/ui/bookmarks/bookmark_home_primary_view.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_home_view_controller_protected.h"
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
    BookmarkMenuViewDelegate,
    BookmarkModelBridgeObserver,
    BookmarkPanelViewDelegate> {
  // Bridge to register for bookmark changes.
  std::unique_ptr<bookmarks::BookmarkModelBridge> _bridge;
}

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

// The action sheet coordinator used when trying to edit a single bookmark.
@property(nonatomic, strong) ActionSheetCoordinator* actionSheetCoordinator;

#pragma mark View loading and switching
// This method should be called at most once in the life-cycle of the
// class. It should be called at the soonest possible time after the
// view has been loaded, and the bookmark model is loaded.
- (void)loadBookmarkViews;
// Updates the property 'primaryMenuItem'.
// Updates the UI to reflect the new state of 'primaryMenuItem'.
- (void)updatePrimaryMenuItem:(BookmarkMenuItem*)menuItem
                     animated:(BOOL)animated;
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

@end

@implementation BookmarkHomeHandsetViewController
@synthesize contentView = _contentView;

@synthesize cachedContentPosition = _cachedContentPosition;

@synthesize actionSheetCoordinator = _actionSheetCoordinator;
@synthesize delegate = _delegate;

- (instancetype)initWithLoader:(id<UrlLoader>)loader
                  browserState:(ios::ChromeBrowserState*)browserState {
  self = [super initWithLoader:loader browserState:browserState];
  if (self) {
    _bridge.reset(new bookmarks::BookmarkModelBridge(self, self.bookmarks));
    self.sideSwipingPossible = YES;
  }
  return self;
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

#pragma mark - Editing bar super methods overrides

- (CGRect)editingBarFrame {
  return self.navigationBar.frame;
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
