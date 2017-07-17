// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bookmarks/bookmark_home_view_controller.h"

#include "components/bookmarks/browser/bookmark_model.h"
#include "ios/chrome/browser/bookmarks/bookmark_model_factory.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/ui/bookmarks/bars/bookmark_editing_bar.h"
#import "ios/chrome/browser/ui/bookmarks/bars/bookmark_navigation_bar.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_collection_view.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_edit_view_controller.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_folder_editor_view_controller.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_folder_view_controller.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_home_primary_view.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_home_view_controller_protected.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_home_waiting_view.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_menu_item.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_menu_view.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_navigation_controller.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_panel_view.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_promo_controller.h"
#import "ios/chrome/browser/ui/rtl_geometry.h"
#import "ios/chrome/browser/ui/ui_util.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"
#import "ios/chrome/browser/ui/url_loader.h"

using bookmarks::BookmarkNode;

namespace {
// The width of the bookmark menu, displaying the different sections.
const CGFloat kMenuWidth = 264;
}

@interface BookmarkHomeViewController ()<
    BookmarkEditViewControllerDelegate,
    BookmarkFolderEditorViewControllerDelegate,
    BookmarkFolderViewControllerDelegate,
    BookmarkPromoControllerDelegate>

@end

@implementation BookmarkHomeViewController

@synthesize bookmarkPromoController = _bookmarkPromoController;
@synthesize bookmarks = _bookmarks;
@synthesize browserState = _browserState;
@synthesize editing = _editing;
@synthesize editIndexPaths = _editIndexPaths;
@synthesize editingBar = _editingBar;
@synthesize editViewController = _editViewController;
@synthesize folderEditor = _folderEditor;
@synthesize folderSelector = _folderSelector;
@synthesize folderView = _folderView;
@synthesize loader = _loader;
@synthesize menuView = _menuView;
@synthesize navigationBar = _navigationBar;
@synthesize panelView = _panelView;
@synthesize primaryMenuItem = _primaryMenuItem;
@synthesize scrollToTop = _scrollToTop;
@synthesize sideSwipingPossible = _sideSwipingPossible;
@synthesize waitForModelView = _waitForModelView;

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#pragma mark - Initializer

- (instancetype)initWithLoader:(id<UrlLoader>)loader
                  browserState:(ios::ChromeBrowserState*)browserState {
  DCHECK(browserState);
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _editIndexPaths = [[NSMutableArray alloc] init];
    [self resetEditNodes];

    _browserState = browserState->GetOriginalChromeBrowserState();
    _loader = loader;

    _bookmarks = ios::BookmarkModelFactory::GetForBrowserState(browserState);
    // It is important to initialize the promo controller with the browser state
    // passed in, as it could be incognito.
    _bookmarkPromoController =
        [[BookmarkPromoController alloc] initWithBrowserState:browserState
                                                     delegate:self];
  }
  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.navigationBar = [[BookmarkNavigationBar alloc] initWithFrame:CGRectZero];
}

#pragma mark - Public

- (void)loadBookmarkViews {
  LayoutRect menuLayout =
      LayoutRectMake(0, self.view.bounds.size.width, 0, self.menuWidth,
                     self.view.bounds.size.height);

  // Create menu view.
  self.menuView = [[BookmarkMenuView alloc]
      initWithBrowserState:self.browserState
                     frame:LayoutRectGetRect(menuLayout)];
  self.menuView.autoresizingMask =
      UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;

  // Create panel view.
  self.panelView = [[BookmarkPanelView alloc] initWithFrame:CGRectZero
                                              menuViewWidth:self.menuWidth];
  self.panelView.autoresizingMask =
      UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;

  // Create folder view.
  BookmarkCollectionView* view =
      [[BookmarkCollectionView alloc] initWithBrowserState:self.browserState
                                                     frame:CGRectZero];
  self.folderView = view;
  [self.folderView setEditing:self.editing animated:NO];
  self.folderView.autoresizingMask =
      UIViewAutoresizingFlexibleHeight | UIViewAutoresizingFlexibleWidth;
}

- (void)updatePrimaryMenuItem:(BookmarkMenuItem*)menuItem {
  DCHECK(menuItem.type == bookmarks::MenuItemFolder);
  if ([self.primaryMenuItem isEqual:menuItem])
    return;

  // TODO(crbug.com/705339): Folder view is the only primary view now,
  // hence we don't need to remove primary view anymore.
  // Simplify this code that removes primary view and adds it back
  // in subclasses, once the addition code moves here.
  [[self primaryView] removeFromSuperview];
  self.primaryMenuItem = menuItem;

  [self.folderView resetFolder:self.primaryMenuItem.folder];
  [self.folderView promoStateChangedAnimated:NO];

  [[self primaryView] changeOrientation:GetInterfaceOrientation()];
  [[self primaryView] setScrollsToTop:!self.scrollToTop];

  [self.menuView updatePrimaryMenuItem:self.primaryMenuItem];
}

- (UIView<BookmarkHomePrimaryView>*)primaryView {
  if (self.primaryMenuItem.type == bookmarks::MenuItemFolder)
    return self.folderView;
  return nil;
}

- (void)loadWaitingView {
  DCHECK(!self.waitForModelView);
  DCHECK(self.view);

  // Present a waiting view.
  BookmarkHomeWaitingView* waitingView =
      [[BookmarkHomeWaitingView alloc] initWithFrame:self.view.bounds];
  self.waitForModelView = waitingView;
  [self.view addSubview:self.waitForModelView];
  [self.waitForModelView startWaiting];
}

- (CGFloat)menuWidth {
  return kMenuWidth;
}

#pragma mark - BookmarkPromoControllerDelegate

- (void)promoStateChanged:(BOOL)promoEnabled {
  [self.folderView
      promoStateChangedAnimated:self.folderView == [self primaryView]];
}

#pragma mark Action sheet callbacks

- (void)selectFirstNode:(const BookmarkNode*)node
               withCell:(UICollectionViewCell*)cell {
  DCHECK(!self.editing);
  [self insertEditNode:node atIndexPath:[self indexPathForCell:cell]];
  [self setEditing:YES animated:YES];
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
  navController.modalPresentationStyle = UIModalPresentationFormSheet;
  [self presentViewController:navController animated:YES completion:NULL];
}

#pragma mark - Navigation Bar Callbacks

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
  navController.modalPresentationStyle = UIModalPresentationFormSheet;
  [self presentViewController:navController animated:YES completion:NULL];
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

#pragma mark - Edit

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
    // Insert null to keep the index valid.
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

- (void)showEditingBarAnimated:(BOOL)animated {
  NOTREACHED() << "Must be overridden by subclass";
}

- (void)hideEditingBarAnimated:(BOOL)animated {
  NOTREACHED() << "Must be overridden by subclass";
}

- (CGRect)editingBarFrame {
  NOTREACHED() << "Must be implemented by subclass";
  return CGRectZero;
}

- (void)updateEditingStateAnimated:(BOOL)animated {
  if (!self.editing) {
    [self hideEditingBarAnimated:animated];
    [self updateEditBarShadow];
    [self enableSideSwiping:YES];
    return;
  }

  if (!self.editingBar) {
    self.editingBar =
        [[BookmarkEditingBar alloc] initWithFrame:[self editingBarFrame]];
    [self.editingBar setCancelTarget:self action:@selector(editingBarCancel)];
    [self.editingBar setDeleteTarget:self action:@selector(editingBarDelete)];
    [self.editingBar setMoveTarget:self action:@selector(editingBarMove)];
    [self.editingBar setEditTarget:self action:@selector(editingBarEdit)];

    [self.view addSubview:self.editingBar];
    self.editingBar.alpha = 0;
    self.editingBar.hidden = YES;
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
  [self enableSideSwiping:NO];
}

- (void)setEditing:(BOOL)editing animated:(BOOL)animated {
  if (_editing == editing)
    return;

  _editing = editing;

  if (editing) {
    self.bookmarkPromoController.promoState = NO;
  } else {
    // Only reset the editing state when leaving edit mode. This allows
    // subclasses to add nodes for editing before entering edit mode.
    [self resetEditNodes];
    [self.bookmarkPromoController updatePromoState];
  }

  [self updateEditingStateAnimated:animated];
  if ([[self primaryMenuItem] supportsEditing])
    [[self primaryView] setEditing:editing animated:animated];
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

#pragma mark - private

- (void)enableSideSwiping:(BOOL)enable {
  if (self.sideSwipingPossible) {
    [self.panelView enableSideSwiping:enable];
  }
}

// Deletes the nodes, and presents a toast with an undo button.
- (void)deleteSelectedNodes {
  [self deleteNodes:_editNodes];
}

- (NSIndexPath*)indexPathForCell:(UICollectionViewCell*)cell {
  DCHECK([self primaryView].collectionView);
  NSIndexPath* indexPath =
      [[self primaryView].collectionView indexPathForCell:cell];
  return indexPath;
}

@end
