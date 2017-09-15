// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bookmarks/bookmark_interaction_controller.h"

#include <stdint.h>

#include "base/logging.h"
#include "base/mac/bind_objc_block.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "ios/chrome/browser/bookmarks/bookmark_model_factory.h"
#include "ios/chrome/browser/bookmarks/bookmark_new_generation_features.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/metrics/new_tab_page_uma.h"
#import "ios/chrome/browser/tabs/tab.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_controller_factory.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_edit_view_controller.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_home_view_controller.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_mediator.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_navigation_controller.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_utils_ios.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#include "ios/chrome/browser/ui/uikit_ui_util.h"
#include "ios/chrome/browser/ui/url_loader.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/third_party/material_components_ios/src/components/Snackbar/src/MaterialSnackbar.h"
#include "ios/web/public/referrer.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;

@interface BookmarkInteractionController ()<
    BookmarkEditViewControllerDelegate,
    BookmarkHomeViewControllerDelegate> {
  // The browser state of the current user.
  ios::ChromeBrowserState* _currentBrowserState;  // weak

  // The browser state to use, might be different from _currentBrowserState if
  // it is incognito.
  ios::ChromeBrowserState* _browserState;  // weak

  // The designated url loader.
  __weak id<UrlLoader> _loader;

  // The parent controller on top of which the UI needs to be presented.
  __weak UIViewController* _parentController;
}

// The bookmark model in use.
@property(nonatomic, assign) BookmarkModel* bookmarkModel;

// A reference to the potentially presented bookmark browser.
@property(nonatomic, strong) BookmarkHomeViewController* bookmarkBrowser;

// A reference to the potentially presented single bookmark editor.
@property(nonatomic, strong) BookmarkEditViewController* bookmarkEditor;

@property(nonatomic, strong) BookmarkMediator* mediator;

@property(nonatomic, readonly, weak) id<ApplicationCommands> dispatcher;

// Builds a controller and brings it on screen.
- (void)presentBookmarkForTab:(Tab*)tab;

// Dismisses the bookmark browser.
- (void)dismissBookmarkBrowserAnimated:(BOOL)animated;

// Dismisses the bookmark editor.
- (void)dismissBookmarkEditorAnimated:(BOOL)animated;

@end

@implementation BookmarkInteractionController
@synthesize bookmarkBrowser = _bookmarkBrowser;
@synthesize bookmarkEditor = _bookmarkEditor;
@synthesize bookmarkModel = _bookmarkModel;
@synthesize mediator = _mediator;
@synthesize dispatcher = _dispatcher;

- (instancetype)initWithBrowserState:(ios::ChromeBrowserState*)browserState
                              loader:(id<UrlLoader>)loader
                    parentController:(UIViewController*)parentController
                          dispatcher:(id<ApplicationCommands>)dispatcher {
  self = [super init];
  if (self) {
    // Bookmarks are always opened with the main browser state, even in
    // incognito mode.
    _currentBrowserState = browserState;
    _browserState = browserState->GetOriginalChromeBrowserState();
    _loader = loader;
    _parentController = parentController;
    _dispatcher = dispatcher;
    _bookmarkModel =
        ios::BookmarkModelFactory::GetForBrowserState(_browserState);
    _mediator = [[BookmarkMediator alloc] initWithBrowserState:_browserState];
    DCHECK(_bookmarkModel);
    DCHECK(_parentController);
  }
  return self;
}

- (void)dealloc {
  _bookmarkBrowser.homeDelegate = nil;
  _bookmarkEditor.delegate = nil;
}

- (void)presentBookmarkForTab:(Tab*)tab {
  DCHECK(!self.bookmarkBrowser && !self.bookmarkEditor);
  DCHECK(tab);

  const BookmarkNode* bookmark =
      self.bookmarkModel->GetMostRecentlyAddedUserNodeForURL(
          tab.lastCommittedURL);
  if (!bookmark)
    return;

  [self dismissSnackbar];

  BookmarkEditViewController* bookmarkEditor =
      [[BookmarkEditViewController alloc] initWithBookmark:bookmark
                                              browserState:_browserState];
  self.bookmarkEditor = bookmarkEditor;
  self.bookmarkEditor.delegate = self;
  UINavigationController* navController = [[BookmarkNavigationController alloc]
      initWithRootViewController:self.bookmarkEditor];
  navController.modalPresentationStyle = UIModalPresentationFormSheet;
  [_parentController presentViewController:navController
                                  animated:YES
                                completion:nil];
}

- (void)presentBookmarkForTab:(Tab*)tab
          currentlyBookmarked:(BOOL)bookmarked
                       inView:(UIView*)parentView
                   originRect:(CGRect)origin {
  if (!self.bookmarkModel->loaded())
    return;
  if (!tab)
    return;

  if (bookmarked) {
    [self presentBookmarkForTab:tab];
  } else {
    __weak BookmarkInteractionController* weakSelf = self;
    __weak Tab* weakTab = tab;
    void (^editAction)() = ^() {
      BookmarkInteractionController* strongSelf = weakSelf;
      if (!strongSelf || !weakTab)
        return;
      [strongSelf presentBookmarkForTab:weakTab];
    };
    [self.mediator addBookmarkWithTitle:tab.title
                                    URL:tab.lastCommittedURL
                             editAction:editAction];
  }
}

- (void)presentBookmarks {
  DCHECK(!self.bookmarkBrowser && !self.bookmarkEditor);
  BookmarkControllerFactory* bookmarkControllerFactory =
      [[BookmarkControllerFactory alloc] init];
  self.bookmarkBrowser = [bookmarkControllerFactory
      bookmarkControllerWithBrowserState:_currentBrowserState
                                  loader:_loader
                              dispatcher:self.dispatcher];
  self.bookmarkBrowser.homeDelegate = self;

  if (base::FeatureList::IsEnabled(kBookmarkNewGeneration)) {
    [self.bookmarkBrowser setRootNode:self.bookmarkModel->root_node()];
    // If cache is present then reconstruct the last visited bookmark from
    // cache.
    if (bookmark_utils_ios::GetBookmarkUIPositionCache(self.bookmarkModel)) {
      self.bookmarkBrowser.isReconstructingFromCache = YES;
    }
    UINavigationController* navController = [[UINavigationController alloc]
        initWithRootViewController:self.bookmarkBrowser];
    [navController setModalPresentationStyle:UIModalPresentationFormSheet];
    [_parentController presentViewController:navController
                                    animated:YES
                                  completion:nil];
  } else {
    self.bookmarkBrowser.modalPresentationStyle = UIModalPresentationFormSheet;
    [_parentController presentViewController:self.bookmarkBrowser
                                    animated:YES
                                  completion:nil];
  }
}

- (void)dismissBookmarkBrowserAnimated:(BOOL)animated {
  if (!self.bookmarkBrowser)
    return;

  [self.bookmarkBrowser dismissModals];
  [_parentController dismissViewControllerAnimated:animated
                                        completion:^{
                                          self.bookmarkBrowser.homeDelegate =
                                              nil;
                                          self.bookmarkBrowser = nil;
                                        }];
}

- (void)dismissBookmarkEditorAnimated:(BOOL)animated {
  if (!self.bookmarkEditor)
    return;

  [_parentController dismissViewControllerAnimated:animated
                                        completion:^{
                                          self.bookmarkEditor.delegate = nil;
                                          self.bookmarkEditor = nil;
                                        }];
}

- (void)dismissBookmarkModalControllerAnimated:(BOOL)animated {
  [self dismissBookmarkBrowserAnimated:animated];
  [self dismissBookmarkEditorAnimated:animated];
}

- (void)dismissSnackbar {
  // Dismiss any bookmark related snackbar this controller could have presented.
  [MDCSnackbarManager dismissAndCallCompletionBlocksWithCategory:
                          bookmark_utils_ios::kBookmarksSnackbarCategory];
}

#pragma mark - BookmarkEditViewControllerDelegate

- (BOOL)bookmarkEditor:(BookmarkEditViewController*)controller
    shoudDeleteAllOccurencesOfBookmark:(const BookmarkNode*)bookmark {
  return YES;
}

- (void)bookmarkEditorWantsDismissal:(BookmarkEditViewController*)controller {
  [self dismissBookmarkEditorAnimated:YES];
}

#pragma mark - BookmarkHomeViewControllerDelegate

- (void)
bookmarkHomeViewControllerWantsDismissal:(BookmarkHomeViewController*)controller
                        navigationToUrls:(const std::vector<GURL>&)urls {
  [self dismissBookmarkBrowserAnimated:YES];

  if (urls.empty())
    return;

  BOOL openInCurrentTab = YES;
  for (const GURL& url : urls) {
    DCHECK(url.is_valid());
    if (openInCurrentTab) {
      // Only open the first URL in the current tab.
      openInCurrentTab = NO;

      // TODO(crbug.com/695749): See if we need different metrics for 'Open
      // All'.
      new_tab_page_uma::RecordAction(_browserState,
                                     new_tab_page_uma::ACTION_OPENED_BOOKMARK);
      base::RecordAction(
          base::UserMetricsAction("MobileBookmarkManagerEntryOpened"));

      [self openURLInCurrentTab:url];
    } else {
      // Open other URLs (if any) in background tabs.
      [self openURLInBackgroundTab:url];
    }
  }  // end for
}

#pragma mark - Private

- (void)openURLInCurrentTab:(const GURL&)url {
  if (url.SchemeIs(url::kJavaScriptScheme)) {  // bookmarklet
    NSString* jsToEval = [base::SysUTF8ToNSString(url.GetContent())
        stringByRemovingPercentEncoding];
    [_loader loadJavaScriptFromLocationBar:jsToEval];
  } else {
    [_loader loadURL:url
                 referrer:web::Referrer()
               transition:ui::PAGE_TRANSITION_AUTO_BOOKMARK
        rendererInitiated:NO];
  }
}

- (void)openURLInBackgroundTab:(const GURL&)url {
  [_loader webPageOrderedOpen:url
                     referrer:web::Referrer()
                 inBackground:YES
                     appendTo:kLastTab];
}

@end
