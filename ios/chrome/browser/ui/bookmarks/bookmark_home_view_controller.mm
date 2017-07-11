// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bookmarks/bookmark_home_view_controller.h"

#include "components/bookmarks/browser/bookmark_model.h"
#include "ios/chrome/browser/bookmarks/bookmark_model_factory.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/ui/bookmarks/bars/bookmark_navigation_bar.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_collection_view.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_home_primary_view.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_home_waiting_view.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_menu_item.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_menu_view.h"
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

@interface BookmarkHomeViewController ()<BookmarkPromoControllerDelegate>
// Read / write declaration of read only properties.
@property(nonatomic, strong) BookmarkPromoController* bookmarkPromoController;
@property(nonatomic, assign) bookmarks::BookmarkModel* bookmarks;
@property(nonatomic, assign) ios::ChromeBrowserState* browserState;
@property(nonatomic, strong) BookmarkCollectionView* folderView;
@property(nonatomic, weak) id<UrlLoader> loader;
@property(nonatomic, strong) BookmarkMenuView* menuView;
@property(nonatomic, strong) BookmarkNavigationBar* navigationBar;
@property(nonatomic, strong) BookmarkPanelView* panelView;
@property(nonatomic, strong) BookmarkMenuItem* primaryMenuItem;
@end

@implementation BookmarkHomeViewController

@synthesize bookmarkPromoController = _bookmarkPromoController;
@synthesize bookmarks = _bookmarks;
@synthesize browserState = _browserState;
@synthesize folderView = _folderView;
@synthesize loader = _loader;
@synthesize menuView = _menuView;
@synthesize navigationBar = _navigationBar;
@synthesize panelView = _panelView;
@synthesize primaryMenuItem = _primaryMenuItem;
@synthesize waitForModelView = _waitForModelView;
@synthesize scrollToTop = _scrollToTop;

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#pragma mark - Initializer

- (instancetype)initWithLoader:(id<UrlLoader>)loader
                  browserState:(ios::ChromeBrowserState*)browserState {
  DCHECK(browserState);
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
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
      initWithBrowserState:_browserState
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

@end
