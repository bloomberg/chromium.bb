// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bookmarks/bookmark_home_view_controller.h"

#include "base/ios/ios_util.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/sys_string_conversions.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/bookmarks/bookmark_model_factory.h"
#include "ios/chrome/browser/bookmarks/bookmarks_utils.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/metrics/new_tab_page_uma.h"
#import "ios/chrome/browser/ui/bookmarks/bars/bookmark_context_bar.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_edit_view_controller.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_folder_editor_view_controller.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_folder_view_controller.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_home_waiting_view.h"
#include "ios/chrome/browser/ui/bookmarks/bookmark_model_bridge_observer.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_navigation_controller.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_path_cache.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_promo_controller.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_table_view.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_utils_ios.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/icons/chrome_icon.h"
#import "ios/chrome/browser/ui/keyboard/UIKeyCommand+Chrome.h"
#import "ios/chrome/browser/ui/material_components/utils.h"
#import "ios/chrome/browser/ui/rtl_geometry.h"
#import "ios/chrome/browser/ui/signin_interaction/public/signin_presenter.h"
#import "ios/chrome/browser/ui/ui_util.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"
#import "ios/chrome/browser/ui/url_loader.h"
#import "ios/chrome/browser/ui/util/constraints_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/third_party/material_components_ios/src/components/AppBar/src/MaterialAppBar.h"
#import "ios/third_party/material_components_ios/src/components/Typography/src/MaterialTypography.h"
#include "ios/web/public/referrer.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_mac.h"

using bookmarks::BookmarkNode;

namespace {
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

// Returns a vector of all URLs in |nodes|.
std::vector<GURL> GetUrlsToOpen(const std::vector<const BookmarkNode*>& nodes) {
  std::vector<GURL> urls;
  for (const BookmarkNode* node : nodes) {
    if (node->is_url()) {
      urls.push_back(node->url());
    }
  }
  return urls;
}
}  // namespace

@interface BookmarkHomeViewController ()<
    BookmarkEditViewControllerDelegate,
    BookmarkFolderEditorViewControllerDelegate,
    BookmarkFolderViewControllerDelegate,
    BookmarkModelBridgeObserver,
    BookmarkPromoControllerDelegate,
    BookmarkTableViewDelegate,
    ContextBarDelegate,
    SigninPresenter,
    UIGestureRecognizerDelegate> {
  // Bridge to register for bookmark changes.
  std::unique_ptr<bookmarks::BookmarkModelBridge> _bridge;

  // The root node, whose child nodes are shown in the bookmark table view.
  const bookmarks::BookmarkNode* _rootNode;
  // YES if NSLayoutConstraits were added.
  BOOL _addedConstraints;
}

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

// Dispatcher for sending commands.
@property(nonatomic, readonly, weak) id<ApplicationCommands> dispatcher;
@end

@implementation BookmarkHomeViewController

@synthesize appBar = _appBar;
@synthesize bookmarkPromoController = _bookmarkPromoController;
@synthesize bookmarks = _bookmarks;
@synthesize browserState = _browserState;
@synthesize editViewController = _editViewController;
@synthesize folderEditor = _folderEditor;
@synthesize folderSelector = _folderSelector;
@synthesize loader = _loader;
@synthesize waitForModelView = _waitForModelView;
@synthesize homeDelegate = _homeDelegate;
@synthesize bookmarksTableView = _bookmarksTableView;
@synthesize contextBar = _contextBar;
@synthesize contextBarState = _contextBarState;
@synthesize dispatcher = _dispatcher;
@synthesize cachedContentPosition = _cachedContentPosition;
@synthesize isReconstructingFromCache = _isReconstructingFromCache;

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#pragma mark - Initializer

- (instancetype)initWithLoader:(id<UrlLoader>)loader
                  browserState:(ios::ChromeBrowserState*)browserState
                    dispatcher:(id<ApplicationCommands>)dispatcher {
  DCHECK(browserState);
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _browserState = browserState->GetOriginalChromeBrowserState();
    _loader = loader;
    _dispatcher = dispatcher;

    _bookmarks = ios::BookmarkModelFactory::GetForBrowserState(browserState);

    _bridge.reset(new bookmarks::BookmarkModelBridge(self, _bookmarks));

    // It is important to initialize the promo controller with the browser state
    // passed in, as it could be incognito.
    _bookmarkPromoController = [[BookmarkPromoController alloc]
        initWithBrowserState:browserState
                    delegate:self
                   presenter:self /* id<SigninPresenter> */];
  }
  return self;
}

- (void)setRootNode:(const bookmarks::BookmarkNode*)rootNode {
  _rootNode = rootNode;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  if (self.bookmarks->loaded()) {
    [self loadBookmarkViews];
  } else {
    [self loadWaitingView];
  }
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];
  // Set the delegate here to make sure it is working when navigating in the
  // ViewController hierarchy (as each view controller is setting itself as
  // delegate).
  self.navigationController.interactivePopGestureRecognizer.delegate = self;
}

- (void)viewDidLayoutSubviews {
  [super viewDidLayoutSubviews];
  // Set the content position after views are laid out, to ensure the right
  // window of rows is shown. Once used, reset self.cachedContentPosition.
  if (self.cachedContentPosition) {
    [self.bookmarksTableView
        setContentPosition:self.cachedContentPosition.floatValue];
    self.cachedContentPosition = nil;
  }
  // The height of contextBar might change due to word wrapping of buttons
  // after titleLabel or orientation changed.
  [self.contextBar updateHeight];
}

- (BOOL)prefersStatusBarHidden {
  return NO;
}

- (NSArray*)keyCommands {
  __weak BookmarkHomeViewController* weakSelf = self;
  return @[ [UIKeyCommand cr_keyCommandWithInput:UIKeyInputEscape
                                   modifierFlags:Cr_UIKeyModifierNone
                                           title:nil
                                          action:^{
                                            [weakSelf navigationBarCancel:nil];
                                          }] ];
}

- (UIStatusBarStyle)preferredStatusBarStyle {
  return UIStatusBarStyleDefault;
}

#pragma mark - Protected

- (void)loadBookmarkViews {
  self.automaticallyAdjustsScrollViewInsets = NO;
  self.bookmarksTableView = [[BookmarkTableView alloc]
      initWithBrowserState:self.browserState
                  delegate:self
                  rootNode:_rootNode
                     frame:self.view.bounds
                 presenter:self /* id<SigninPresenter> */];
  [self.bookmarksTableView
      setAutoresizingMask:UIViewAutoresizingFlexibleWidth |
                          UIViewAutoresizingFlexibleHeight];
  [self.bookmarksTableView setTranslatesAutoresizingMaskIntoConstraints:NO];
  [self.view addSubview:self.bookmarksTableView];

  // After the table view has been added.
  [self setupNavigationBar];

  if (_rootNode != self.bookmarks->root_node()) {
    [self setupContextBar];
  }
  if (self.isReconstructingFromCache) {
    [self setupUIStackCacheIfApplicable];
  }
  DCHECK(self.bookmarks->loaded());
  DCHECK([self isViewLoaded]);
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

- (void)cachePosition {
  // Cache position for BookmarkTableView.
  [BookmarkPathCache
      cacheBookmarkUIPositionWithPrefService:self.browserState->GetPrefs()
                                    folderId:_rootNode->id()
                              scrollPosition:static_cast<double>(
                                                 self.bookmarksTableView
                                                     .contentPosition)];
}

#pragma mark - BookmarkPromoControllerDelegate

- (void)promoStateChanged:(BOOL)promoEnabled {
  [self.bookmarksTableView promoStateChangedAnimated:YES];
}

- (void)configureSigninPromoWithConfigurator:
            (SigninPromoViewConfigurator*)configurator
                             identityChanged:(BOOL)identityChanged {
  [self.bookmarksTableView
      configureSigninPromoWithConfigurator:configurator
                           identityChanged:identityChanged];
}

#pragma mark Action sheet callbacks

// Opens the folder move editor for the given node.
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

// Deletes the current node.
- (void)deleteNodes:(const std::set<const BookmarkNode*>&)nodes {
  DCHECK_GE(nodes.size(), 1u);
  bookmark_utils_ios::DeleteBookmarksWithUndoToast(nodes, self.bookmarks,
                                                   self.browserState);
  [self setTableViewEditing:NO];
}

// Opens the editor on the given node.
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

- (void)openAllNodes:(const std::vector<const bookmarks::BookmarkNode*>&)nodes
         inIncognito:(BOOL)inIncognito
              newTab:(BOOL)newTab {
  [self cachePosition];
  std::vector<GURL> urls = GetUrlsToOpen(nodes);
  [self.homeDelegate bookmarkHomeViewControllerWantsDismissal:self
                                             navigationToUrls:urls
                                                  inIncognito:inIncognito
                                                       newTab:newTab];
}

#pragma mark - Navigation Bar Callbacks

- (void)navigationBarCancel:(id)sender {
  [self.bookmarksTableView navigateAway];
  [self dismissWithURL:GURL()];
}

#pragma mark - BookmarkTableViewDelegate

- (SigninPromoViewMediator*)signinPromoViewMediator {
  return self.bookmarkPromoController.signinPromoViewMediator;
}

- (void)bookmarkTableView:(BookmarkTableView*)view
    selectedUrlForNavigation:(const GURL&)url {
  [self dismissWithURL:url];
}

- (void)bookmarkTableView:(BookmarkTableView*)view
    selectedFolderForNavigation:(const bookmarks::BookmarkNode*)folder {
  BookmarkHomeViewController* controller =
      [self createControllerWithRootFolder:folder];
  [self.navigationController pushViewController:controller animated:YES];
}

- (void)bookmarkTableView:(BookmarkTableView*)view
    selectedNodesForDeletion:
        (const std::set<const bookmarks::BookmarkNode*>&)nodes {
  [self deleteNodes:nodes];
}

- (BOOL)bookmarkTableViewShouldShowPromoCell:(BookmarkTableView*)tableView {
  return self.bookmarkPromoController.shouldShowSigninPromo;
}

- (void)bookmarkTableView:(BookmarkTableView*)view
        selectedEditNodes:
            (const std::set<const bookmarks::BookmarkNode*>&)nodes {
  // Early return if bookmarks table is not in edit mode.
  if (!self.bookmarksTableView.editing) {
    return;
  }

  if (nodes.size() == 0) {
    // if nothing to select, exit edit mode.
    if (![self.bookmarksTableView hasBookmarksOrFolders]) {
      [self setTableViewEditing:NO];
      return;
    }
    [self setContextBarState:BookmarksContextBarBeginSelection];
    return;
  }
  if (nodes.size() == 1) {
    const bookmarks::BookmarkNode* node = *nodes.begin();
    if (node->is_url()) {
      [self setContextBarState:BookmarksContextBarSingleURLSelection];
    } else if (node->is_folder()) {
      [self setContextBarState:BookmarksContextBarSingleFolderSelection];
    }
    return;
  }

  BOOL foundURL = NO;
  BOOL foundFolder = NO;
  for (const BookmarkNode* node : nodes) {
    if (!foundURL && node->is_url()) {
      foundURL = YES;
    } else if (!foundFolder && node->is_folder()) {
      foundFolder = YES;
    }
    // Break early, if we found both types of nodes.
    if (foundURL && foundFolder) {
      break;
    }
  }

  // Only URLs are selected.
  if (foundURL && !foundFolder) {
    [self setContextBarState:BookmarksContextBarMultipleURLSelection];
    return;
  }
  // Only Folders are selected.
  if (!foundURL && foundFolder) {
    [self setContextBarState:BookmarksContextBarMultipleFolderSelection];
    return;
  }
  // Mixed selection.
  if (foundURL && foundFolder) {
    [self setContextBarState:BookmarksContextBarMixedSelection];
    return;
  }

  NOTREACHED();
  return;
}

- (void)bookmarkTableView:(BookmarkTableView*)view
    showContextMenuForNode:(const bookmarks::BookmarkNode*)node {
  if (node->is_url()) {
    [self presentViewController:[self contextMenuForSingleBookmarkURL:node]
                       animated:YES
                     completion:nil];
    return;
  }

  if (node->is_folder()) {
    [self presentViewController:[self contextMenuForSingleBookmarkFolder:node]
                       animated:YES
                     completion:nil];
    return;
  }
  NOTREACHED();
}

- (void)bookmarkTableView:(BookmarkTableView*)view
              didMoveNode:(const bookmarks::BookmarkNode*)node
               toPosition:(int)position {
  bookmark_utils_ios::UpdateBookmarkPositionWithUndoToast(
      node, _rootNode, position, self.bookmarks, self.browserState);
}

- (void)bookmarkTableViewRefreshContextBar:(BookmarkTableView*)view {
  // At default state, the enable state of context bar buttons could change
  // during refresh.
  if (self.contextBarState == BookmarksContextBarDefault) {
    [self setBookmarksContextBarButtonsDefaultState];
  }
}

- (BOOL)isAtTopOfNavigation:(BookmarkTableView*)view {
  return (self.navigationController.topViewController == self);
}

#pragma mark - BookmarkFolderViewControllerDelegate

- (void)folderPicker:(BookmarkFolderViewController*)folderPicker
    didFinishWithFolder:(const BookmarkNode*)folder {
  DCHECK(folder);
  DCHECK(!folder->is_url());
  DCHECK_GE(folderPicker.editedNodes.size(), 1u);

  bookmark_utils_ios::MoveBookmarksWithUndoToast(
      folderPicker.editedNodes, self.bookmarks, folder, self.browserState);

  [self setTableViewEditing:NO];
  [self dismissViewControllerAnimated:YES completion:NULL];
  self.folderSelector.delegate = nil;
  self.folderSelector = nil;
}

- (void)folderPickerDidCancel:(BookmarkFolderViewController*)folderPicker {
  [self setTableViewEditing:NO];
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

- (void)bookmarkFolderEditorWillCommitTitleChange:
    (BookmarkFolderEditorViewController*)controller {
  [self setTableViewEditing:NO];
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
}

- (void)bookmarkEditorWillCommitTitleOrUrlChange:
    (BookmarkEditViewController*)controller {
  [self setTableViewEditing:NO];
}

#pragma mark - BookmarkModelBridgeObserver

- (void)bookmarkModelLoaded {
  if (![self isViewLoaded])
    return;

  int64_t unusedFolderId;
  double unusedScrollPosition;
  // Bookmark Model is loaded after presenting Bookmarks,  we need to check
  // again here if restoring of cache position is needed.  It is to prevent
  // crbug.com/765503.
  if ([BookmarkPathCache
          getBookmarkUIPositionCacheWithPrefService:self.browserState
                                                        ->GetPrefs()
                                              model:self.bookmarks
                                           folderId:&unusedFolderId
                                     scrollPosition:&unusedScrollPosition]) {
    self.isReconstructingFromCache = YES;
  }

  DCHECK(self.waitForModelView);
  __weak BookmarkHomeViewController* weakSelf = self;
  [self.waitForModelView stopWaitingWithCompletion:^{
    BookmarkHomeViewController* strongSelf = weakSelf;
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
  // No-op here.  Bookmarks might be refreshed at bookmarkTableView.
}

- (void)bookmarkNodeChildrenChanged:(const BookmarkNode*)bookmarkNode {
  // No-op here.  Bookmarks might be refreshed at bookmarkTableView.
}

- (void)bookmarkNode:(const BookmarkNode*)bookmarkNode
     movedFromParent:(const BookmarkNode*)oldParent
            toParent:(const BookmarkNode*)newParent {
  // No-op here.  Bookmarks might be refreshed at bookmarkTableView.
}

- (void)bookmarkNodeDeleted:(const BookmarkNode*)node
                 fromFolder:(const BookmarkNode*)folder {
  if (_rootNode == node) {
    [self setTableViewEditing:NO];
  }
}

- (void)bookmarkModelRemovedAllNodes {
  // No-op
}

#pragma mark - Accessibility

- (BOOL)accessibilityPerformEscape {
  [self dismissWithURL:GURL()];
  return YES;
}

#pragma mark - private

- (void)setupUIStackCacheIfApplicable {
  self.isReconstructingFromCache = NO;

  int64_t folderId;
  double scrollPosition;
  // If folderId is invalid or rootNode reached the cached folderId, stop
  // stacking and return.
  if (![BookmarkPathCache
          getBookmarkUIPositionCacheWithPrefService:self.browserState
                                                        ->GetPrefs()
                                              model:self.bookmarks
                                           folderId:&folderId
                                     scrollPosition:&scrollPosition] ||
      folderId == _rootNode->id()) {
    return;
  }

  // Otherwise drill down until we recreate the UI stack for the cached bookmark
  // path.
  NSMutableArray* mutablePath = [bookmark_utils_ios::CreateBookmarkPath(
      self.bookmarks, folderId) mutableCopy];
  if (!mutablePath) {
    return;
  }
  NSArray* thisBookmarkPath =
      bookmark_utils_ios::CreateBookmarkPath(self.bookmarks, _rootNode->id());
  if (!thisBookmarkPath) {
    return;
  }

  [mutablePath removeObjectsInArray:thisBookmarkPath];
  const BookmarkNode* node = bookmark_utils_ios::FindFolderById(
      self.bookmarks, [[mutablePath firstObject] longLongValue]);
  DCHECK(node);
  // if node is an empty permanent node, return.
  if (node->empty() && IsPrimaryPermanentNode(node, self.bookmarks)) {
    return;
  }

  BookmarkHomeViewController* controller =
      [self createControllerWithRootFolder:node];
  // Only scroll to the last viewing position for the leaf node.
  if (mutablePath.count == 1 && scrollPosition) {
    [controller
        setCachedContentPosition:[NSNumber numberWithDouble:scrollPosition]];
  }
  controller.isReconstructingFromCache = YES;
  [self.navigationController pushViewController:controller animated:NO];
}

// Set up context bar for the new UI.
- (void)setupContextBar {
  self.contextBar = [[BookmarkContextBar alloc] initWithFrame:CGRectZero];
  self.contextBar.delegate = self;
  [self.contextBar setTranslatesAutoresizingMaskIntoConstraints:NO];

  [self setContextBarState:BookmarksContextBarDefault];
  [self.view addSubview:self.contextBar];
}

// Set up navigation bar for the new UI.
- (void)setupNavigationBar {
  self.navigationController.navigationBarHidden = YES;
  self.appBar = [[MDCAppBar alloc] init];
  [self addChildViewController:self.appBar.headerViewController];
  ConfigureAppBarWithCardStyle(self.appBar);
  // Set the header view's tracking scroll view.
  self.appBar.headerViewController.headerView.trackingScrollView =
      self.bookmarksTableView.tableView;
  self.bookmarksTableView.headerView =
      self.appBar.headerViewController.headerView;

  [self.appBar addSubviewsToParent];
  // Prevent the touch events on appBar from being forwarded to the tableView.
  // See https://crbug.com/773580
  [self.appBar.headerViewController.headerView
      stopForwardingTouchEventsForView:self.appBar.navigationBar];

  if (self.navigationController.viewControllers.count > 1) {
    // Add custom back button.
    UIBarButtonItem* backButton =
        [ChromeIcon templateBarButtonItemWithImage:[ChromeIcon backIcon]
                                            target:self
                                            action:@selector(back)];
    self.navigationItem.leftBarButtonItem = backButton;
  }

  // Add custom title.
  self.title = bookmark_utils_ios::TitleForBookmarkNode(_rootNode);

  // Add custom done button.
  self.navigationItem.rightBarButtonItem = [self customizedDoneButton];
}

// Back button callback for the new ui.
- (void)back {
  [self.bookmarksTableView navigateAway];
  [self.navigationController popViewControllerAnimated:YES];
}

- (UIBarButtonItem*)customizedDoneButton {
  UIBarButtonItem* doneButton = [[UIBarButtonItem alloc]
      initWithTitle:l10n_util::GetNSString(IDS_IOS_NAVIGATION_BAR_DONE_BUTTON)
              style:UIBarButtonItemStyleDone
             target:self
             action:@selector(navigationBarCancel:)];
  doneButton.accessibilityLabel =
      l10n_util::GetNSString(IDS_IOS_NAVIGATION_BAR_DONE_BUTTON);
  return doneButton;
}

// Saves the current position and asks the delegate to open the url, if delegate
// is set, otherwise opens the URL using loader.
- (void)dismissWithURL:(const GURL&)url {
  [self cachePosition];
  if (self.homeDelegate) {
    std::vector<GURL> urls;
    if (url.is_valid())
      urls.push_back(url);
    [self.homeDelegate bookmarkHomeViewControllerWantsDismissal:self
                                               navigationToUrls:urls];
  } else {
    // Before passing the URL to the block, make sure the block has a copy of
    // the URL and not just a reference.
    const GURL localUrl(url);
    dispatch_async(dispatch_get_main_queue(), ^{
      [self loadURL:localUrl];
    });
  }
}

- (void)loadURL:(const GURL&)url {
  if (url.is_empty() || url.SchemeIs(url::kJavaScriptScheme))
    return;

  new_tab_page_uma::RecordAction(self.browserState,
                                 new_tab_page_uma::ACTION_OPENED_BOOKMARK);
  base::RecordAction(
      base::UserMetricsAction("MobileBookmarkManagerEntryOpened"));
  [self.loader loadURL:url
               referrer:web::Referrer()
             transition:ui::PAGE_TRANSITION_AUTO_BOOKMARK
      rendererInitiated:NO];
}

- (void)updateViewConstraints {
  if (!_addedConstraints) {
    if (self.contextBar && self.bookmarksTableView) {
      NSDictionary* views = @{
        @"tableView" : self.bookmarksTableView,
        @"contextBar" : self.contextBar,
      };
      NSArray* constraints = @[
        @"V:|[tableView][contextBar]|",
        @"H:|[tableView]|",
        @"H:|[contextBar]|",
      ];
      ApplyVisualConstraints(constraints, views);
    } else if (self.bookmarksTableView) {
      NSDictionary* views = @{
        @"tableView" : self.bookmarksTableView,
      };
      NSArray* constraints = @[
        @"V:|[tableView]|",
        @"H:|[tableView]|",
      ];
      ApplyVisualConstraints(constraints, views);
    }
    _addedConstraints = YES;
  }
  [super updateViewConstraints];
}

- (BookmarkHomeViewController*)createControllerWithRootFolder:
    (const bookmarks::BookmarkNode*)folder {
  BookmarkHomeViewController* controller =
      [[BookmarkHomeViewController alloc] initWithLoader:_loader
                                            browserState:self.browserState
                                              dispatcher:self.dispatcher];
  [controller setRootNode:folder];
  controller.homeDelegate = self.homeDelegate;
  return controller;
}

// Sets the editing mode for tableView, update context bar state accordingly.
- (void)setTableViewEditing:(BOOL)editing {
  [self.bookmarksTableView setEditing:editing];
  [self setContextBarState:editing ? BookmarksContextBarBeginSelection
                                   : BookmarksContextBarDefault];
}

#pragma mark - ContextBarDelegate implementation

// Called when the leading button is clicked.
- (void)leadingButtonClicked {
  // Ignore the button tap if view controller presenting.
  if ([self presentedViewController]) {
    return;
  }
  const std::set<const bookmarks::BookmarkNode*> nodes =
      [self.bookmarksTableView editNodes];
  switch (self.contextBarState) {
    case BookmarksContextBarDefault:
      // New Folder clicked.
      [self.bookmarksTableView addNewFolder];
      break;
    case BookmarksContextBarBeginSelection:
      // This must never happen, as the leading button is disabled at this
      // point.
      NOTREACHED();
      break;
    case BookmarksContextBarSingleURLSelection:
    case BookmarksContextBarMultipleURLSelection:
    case BookmarksContextBarSingleFolderSelection:
    case BookmarksContextBarMultipleFolderSelection:
    case BookmarksContextBarMixedSelection:
      // Delete clicked.
      [self deleteNodes:nodes];
      break;
    case BookmarksContextBarNone:
    default:
      NOTREACHED();
  }
}

// Called when the center button is clicked.
- (void)centerButtonClicked {
  // Ignore the button tap if view controller presenting.
  if ([self presentedViewController]) {
    return;
  }
  const std::set<const bookmarks::BookmarkNode*> nodes =
      [self.bookmarksTableView editNodes];
  // Center button is shown and is clickable only when at least
  // one node is selected.
  DCHECK(nodes.size() > 0);
  switch (self.contextBarState) {
    case BookmarksContextBarDefault:
      // Center button is disabled in default state.
      NOTREACHED();
      break;
    case BookmarksContextBarBeginSelection:
      // Center button is disabled in start state.
      NOTREACHED();
      break;
    case BookmarksContextBarSingleURLSelection:
      // More clicked, show action sheet with context menu.
      [self presentViewController:
                [self contextMenuForSingleBookmarkURL:*(nodes.begin())]
                         animated:YES
                       completion:nil];
      break;
    case BookmarksContextBarMultipleURLSelection:
      // More clicked, show action sheet with context menu.
      [self
          presentViewController:[self contextMenuForMultipleBookmarkURLs:nodes]
                       animated:YES
                     completion:nil];
      break;
    case BookmarksContextBarSingleFolderSelection:
      // More clicked, show action sheet with context menu.
      [self presentViewController:
                [self contextMenuForSingleBookmarkFolder:*(nodes.begin())]
                         animated:YES
                       completion:nil];
      break;
    case BookmarksContextBarMultipleFolderSelection:
    case BookmarksContextBarMixedSelection:
      // More clicked, show action sheet with context menu.
      [self presentViewController:
                [self contextMenuForMixedAndMultiFolderSelection:nodes]
                         animated:YES
                       completion:nil];
      break;
    case BookmarksContextBarNone:
    default:
      NOTREACHED();
  }
}

// Called when the trailing button, "Select" or "Cancel" is clicked.
- (void)trailingButtonClicked {
  // Ignore the button tap if view controller presenting.
  if ([self presentedViewController]) {
    return;
  }
  // Toggle edit mode.
  [self setTableViewEditing:!self.bookmarksTableView.editing];
}

#pragma mark - ContextBarStates

// Customizes the context bar buttons based the |state| passed in.
- (void)setContextBarState:(BookmarksContextBarState)state {
  _contextBarState = state;
  switch (state) {
    case BookmarksContextBarDefault:
      [self setBookmarksContextBarButtonsDefaultState];
      break;
    case BookmarksContextBarBeginSelection:
      [self setBookmarksContextBarSelectionStartState];
      break;
    case BookmarksContextBarSingleURLSelection:
    case BookmarksContextBarMultipleURLSelection:
    case BookmarksContextBarMultipleFolderSelection:
    case BookmarksContextBarMixedSelection:
    case BookmarksContextBarSingleFolderSelection:
      // Reset to start state, and then override with customizations that apply.
      [self setBookmarksContextBarSelectionStartState];
      [self.contextBar setButtonEnabled:YES forButton:ContextBarCenterButton];
      [self.contextBar setButtonEnabled:YES forButton:ContextBarLeadingButton];
      break;
    case BookmarksContextBarNone:
    default:
      break;
  }
}

- (void)setBookmarksContextBarButtonsDefaultState {
  // Set New Folder button
  [self.contextBar setButtonTitle:l10n_util::GetNSString(
                                      IDS_IOS_BOOKMARK_CONTEXT_BAR_NEW_FOLDER)
                        forButton:ContextBarLeadingButton];
  [self.contextBar setButtonVisibility:YES forButton:ContextBarLeadingButton];
  [self.contextBar setButtonEnabled:[self.bookmarksTableView allowsNewFolder]
                          forButton:ContextBarLeadingButton];
  [self.contextBar setButtonStyle:ContextBarButtonStyleDefault
                        forButton:ContextBarLeadingButton];

  // Set Center button to invisible.
  [self.contextBar setButtonVisibility:NO forButton:ContextBarCenterButton];

  // Set Select button.
  [self.contextBar
      setButtonTitle:l10n_util::GetNSString(IDS_IOS_BOOKMARK_CONTEXT_BAR_SELECT)
           forButton:ContextBarTrailingButton];
  [self.contextBar setButtonVisibility:YES forButton:ContextBarTrailingButton];
  [self.contextBar
      setButtonEnabled:[self.bookmarksTableView hasBookmarksOrFolders]
             forButton:ContextBarTrailingButton];
}

- (void)setBookmarksContextBarSelectionStartState {
  // Disabled Delete button.
  [self.contextBar
      setButtonTitle:l10n_util::GetNSString(IDS_IOS_BOOKMARK_CONTEXT_BAR_DELETE)
           forButton:ContextBarLeadingButton];
  [self.contextBar setButtonVisibility:YES forButton:ContextBarLeadingButton];
  [self.contextBar setButtonEnabled:NO forButton:ContextBarLeadingButton];
  [self.contextBar setButtonStyle:ContextBarButtonStyleDelete
                        forButton:ContextBarLeadingButton];

  // Disabled More button.
  [self.contextBar
      setButtonTitle:l10n_util::GetNSString(IDS_IOS_BOOKMARK_CONTEXT_BAR_MORE)
           forButton:ContextBarCenterButton];
  [self.contextBar setButtonVisibility:YES forButton:ContextBarCenterButton];
  [self.contextBar setButtonEnabled:NO forButton:ContextBarCenterButton];

  // Enabled Cancel button.
  [self.contextBar setButtonTitle:l10n_util::GetNSString(IDS_CANCEL)
                        forButton:ContextBarTrailingButton];
  [self.contextBar setButtonVisibility:YES forButton:ContextBarTrailingButton];
  [self.contextBar setButtonEnabled:YES forButton:ContextBarTrailingButton];
}

#pragma mark - Context Menu

- (UIAlertController*)contextMenuForMultipleBookmarkURLs:
    (const std::set<const bookmarks::BookmarkNode*>)nodes {
  __weak BookmarkHomeViewController* weakSelf = self;
  UIAlertController* alert = [UIAlertController
      alertControllerWithTitle:nil
                       message:nil
                preferredStyle:UIAlertControllerStyleActionSheet];
  alert.view.accessibilityIdentifier = @"bookmark_context_menu";

  UIAlertAction* cancelAction =
      [UIAlertAction actionWithTitle:l10n_util::GetNSString(IDS_CANCEL)
                               style:UIAlertActionStyleCancel
                             handler:nil];

  UIAlertAction* openAllAction = [UIAlertAction
      actionWithTitle:l10n_util::GetNSString(IDS_IOS_BOOKMARK_CONTEXT_MENU_OPEN)
                style:UIAlertActionStyleDefault
              handler:^(UIAlertAction* _Nonnull action) {
                std::vector<const BookmarkNode*> nodes =
                    [weakSelf.bookmarksTableView getEditNodesInVector];
                [weakSelf openAllNodes:nodes inIncognito:NO newTab:NO];
              }];

  UIAlertAction* openInIncognitoAction = [UIAlertAction
      actionWithTitle:l10n_util::GetNSString(
                          IDS_IOS_BOOKMARK_CONTEXT_MENU_OPEN_INCOGNITO)
                style:UIAlertActionStyleDefault
              handler:^(UIAlertAction* _Nonnull action) {
                std::vector<const BookmarkNode*> nodes =
                    [weakSelf.bookmarksTableView getEditNodesInVector];
                [weakSelf openAllNodes:nodes inIncognito:YES newTab:NO];
              }];

  UIAlertAction* moveAction = [UIAlertAction
      actionWithTitle:l10n_util::GetNSString(IDS_IOS_BOOKMARK_CONTEXT_MENU_MOVE)
                style:UIAlertActionStyleDefault
              handler:^(UIAlertAction* _Nonnull action) {
                [weakSelf moveNodes:nodes];
              }];
  [alert addAction:openAllAction];
  [alert addAction:openInIncognitoAction];
  [alert addAction:moveAction];
  [alert addAction:cancelAction];
  return alert;
}

- (UIAlertController*)contextMenuForSingleBookmarkURL:
    (const BookmarkNode*)node {
  __weak BookmarkHomeViewController* weakSelf = self;
  UIAlertController* alert = [UIAlertController
      alertControllerWithTitle:nil
                       message:nil
                preferredStyle:UIAlertControllerStyleActionSheet];
  alert.view.accessibilityIdentifier = @"bookmark_context_menu";

  UIAlertAction* cancelAction =
      [UIAlertAction actionWithTitle:l10n_util::GetNSString(IDS_CANCEL)
                               style:UIAlertActionStyleCancel
                             handler:nil];

  UIAlertAction* editAction = [UIAlertAction
      actionWithTitle:l10n_util::GetNSString(IDS_IOS_BOOKMARK_CONTEXT_MENU_EDIT)
                style:UIAlertActionStyleDefault
              handler:^(UIAlertAction* _Nonnull action) {
                [weakSelf editNode:node];
              }];

  void (^copyHandler)(UIAlertAction*) = ^(UIAlertAction*) {
    UIPasteboard* pasteboard = [UIPasteboard generalPasteboard];
    std::string urlString = node->url().possibly_invalid_spec();
    pasteboard.string = base::SysUTF8ToNSString(urlString);
    [self setTableViewEditing:NO];
  };
  UIAlertAction* copyAction = [UIAlertAction
      actionWithTitle:l10n_util::GetNSString(IDS_IOS_CONTENT_CONTEXT_COPY)
                style:UIAlertActionStyleDefault
              handler:copyHandler];

  UIAlertAction* openInNewTabAction = [UIAlertAction
      actionWithTitle:l10n_util::GetNSString(
                          IDS_IOS_CONTENT_CONTEXT_OPENLINKNEWTAB)
                style:UIAlertActionStyleDefault
              handler:^(UIAlertAction* _Nonnull action) {
                std::vector<const BookmarkNode*> nodes = {node};
                [weakSelf openAllNodes:nodes inIncognito:NO newTab:YES];
              }];

  UIAlertAction* openInIncognitoAction = [UIAlertAction
      actionWithTitle:l10n_util::GetNSString(
                          IDS_IOS_CONTENT_CONTEXT_OPENLINKNEWINCOGNITOTAB)
                style:UIAlertActionStyleDefault
              handler:^(UIAlertAction* _Nonnull action) {
                std::vector<const BookmarkNode*> nodes = {node};
                [weakSelf openAllNodes:nodes inIncognito:YES newTab:YES];
              }];

  [alert addAction:editAction];
  [alert addAction:openInNewTabAction];
  [alert addAction:openInIncognitoAction];
  [alert addAction:copyAction];
  [alert addAction:cancelAction];
  return alert;
}

- (UIAlertController*)contextMenuForSingleBookmarkFolder:
    (const BookmarkNode*)node {
  __weak BookmarkHomeViewController* weakSelf = self;
  UIAlertController* alert = [UIAlertController
      alertControllerWithTitle:nil
                       message:nil
                preferredStyle:UIAlertControllerStyleActionSheet];
  alert.view.accessibilityIdentifier = @"bookmark_context_menu";

  UIAlertAction* cancelAction =
      [UIAlertAction actionWithTitle:l10n_util::GetNSString(IDS_CANCEL)
                               style:UIAlertActionStyleCancel
                             handler:nil];

  UIAlertAction* editAction = [UIAlertAction
      actionWithTitle:l10n_util::GetNSString(
                          IDS_IOS_BOOKMARK_CONTEXT_MENU_EDIT_FOLDER)
                style:UIAlertActionStyleDefault
              handler:^(UIAlertAction* _Nonnull action) {
                [weakSelf editNode:node];
              }];

  void (^moveHandler)(UIAlertAction*) = ^(UIAlertAction*) {
    std::set<const BookmarkNode*> nodes;
    nodes.insert(node);
    [weakSelf moveNodes:nodes];
  };
  UIAlertAction* moveAction = [UIAlertAction
      actionWithTitle:l10n_util::GetNSString(IDS_IOS_BOOKMARK_CONTEXT_MENU_MOVE)
                style:UIAlertActionStyleDefault
              handler:moveHandler];
  [alert addAction:editAction];
  [alert addAction:moveAction];
  [alert addAction:cancelAction];
  return alert;
}

- (UIAlertController*)contextMenuForMixedAndMultiFolderSelection:
    (const std::set<const bookmarks::BookmarkNode*>)nodes {
  __weak BookmarkHomeViewController* weakSelf = self;
  UIAlertController* alert = [UIAlertController
      alertControllerWithTitle:nil
                       message:nil
                preferredStyle:UIAlertControllerStyleActionSheet];
  alert.view.accessibilityIdentifier = @"bookmark_context_menu";

  UIAlertAction* cancelAction =
      [UIAlertAction actionWithTitle:l10n_util::GetNSString(IDS_CANCEL)
                               style:UIAlertActionStyleCancel
                             handler:nil];

  UIAlertAction* moveAction = [UIAlertAction
      actionWithTitle:l10n_util::GetNSString(IDS_IOS_BOOKMARK_CONTEXT_MENU_MOVE)
                style:UIAlertActionStyleDefault
              handler:^(UIAlertAction* _Nonnull action) {
                [weakSelf moveNodes:nodes];
              }];
  [alert addAction:moveAction];
  [alert addAction:cancelAction];
  return alert;
}

#pragma mark - UIGestureRecognizerDelegate

- (BOOL)gestureRecognizerShouldBegin:(UIGestureRecognizer*)gestureRecognizer {
  DCHECK(gestureRecognizer ==
         self.navigationController.interactivePopGestureRecognizer);
  return self.navigationController.viewControllers.count > 1;
}

#pragma mark - SigninPresenter

- (void)showSignin:(ShowSigninCommand*)command {
  [self.dispatcher showSignin:command baseViewController:self];
}

@end
