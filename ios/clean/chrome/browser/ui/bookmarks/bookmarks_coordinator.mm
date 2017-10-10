// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/bookmarks/bookmarks_coordinator.h"

#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/sys_string_conversions.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "ios/chrome/browser/bookmarks/bookmark_model_factory.h"
#include "ios/chrome/browser/bookmarks/bookmark_new_generation_features.h"
#import "ios/chrome/browser/metrics/new_tab_page_uma.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_controller_factory.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_home_handset_view_controller.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_home_tablet_ntp_controller.h"
#import "ios/chrome/browser/ui/browser_list/browser.h"
#import "ios/chrome/browser/ui/coordinators/browser_coordinator+internal.h"
#import "ios/clean/chrome/browser/ui/adaptor/application_commands_adaptor.h"
#import "ios/clean/chrome/browser/ui/adaptor/url_loader_adaptor.h"
#include "ios/web/public/referrer.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface BookmarksCoordinator ()<BookmarkHomeViewControllerDelegate>
@property(nonatomic, strong) UIViewController* viewController;
// A reference to the bookmark browser presented when the device is not an iPad.
@property(nonatomic, strong) BookmarkHomeViewController* bookmarkBrowser;
// Adaptor for old architecture URL loaders.
@property(nonatomic, strong) URLLoaderAdaptor* loader;
// Adaptor for old architecture application commands.
@property(nonatomic, strong)
    ApplicationCommandsAdaptor* applicationCommandAdaptor;
@end

@implementation BookmarksCoordinator
@synthesize mode = _mode;
@synthesize viewController = _viewController;
@synthesize bookmarkBrowser = _bookmarkBrowser;
@synthesize loader = _loader;
@synthesize applicationCommandAdaptor = _applicationCommandAdaptor;

- (void)start {
  if (self.started)
    return;

  DCHECK(self.mode != UNDEFINED);

  self.loader = [[URLLoaderAdaptor alloc] init];
  self.applicationCommandAdaptor = [[ApplicationCommandsAdaptor alloc] init];
  if (self.mode == PRESENTED) {
    BookmarkControllerFactory* bookmarkControllerFactory =
        [[BookmarkControllerFactory alloc] init];
    self.bookmarkBrowser = [bookmarkControllerFactory
        bookmarkControllerWithBrowserState:self.browser->browser_state()
                                    loader:self.loader
                                dispatcher:self.applicationCommandAdaptor];
    self.bookmarkBrowser.homeDelegate = self;

    if (base::FeatureList::IsEnabled(kBookmarkNewGeneration)) {
      UINavigationController* navController = [[UINavigationController alloc]
          initWithRootViewController:self.bookmarkBrowser];

      bookmarks::BookmarkModel* bookmarkModel =
          ios::BookmarkModelFactory::GetForBrowserState(
              self.browser->browser_state());
      [self.bookmarkBrowser setRootNode:bookmarkModel->root_node()];
      self.viewController = navController;
    } else {
      self.viewController = self.bookmarkBrowser;
    }
  } else {
    self.viewController = [[BookmarkHomeTabletNTPController alloc]
        initWithLoader:self.loader
          browserState:self.browser->browser_state()
            dispatcher:self.applicationCommandAdaptor];
  }
  self.loader.viewControllerForAlert = self.viewController;
  self.applicationCommandAdaptor.viewControllerForAlert = self.viewController;
  [super start];
}

- (void)stop {
  self.bookmarkBrowser.homeDelegate = nil;
  self.bookmarkBrowser = nil;
  [super stop];
}

#pragma mark - BookmarkHomeViewControllerDelegate

- (void)
bookmarkHomeViewControllerWantsDismissal:(BookmarkHomeViewController*)controller
                        navigationToUrls:(const std::vector<GURL>&)urls {
  // TODO(crbug.com/695749):  Rewrite this function when adding bookmark becomes
  // available in chrome_clean_skeleton.  See if we can share the code from
  // bookmark_home_view_controller.  Also see if we can add _currentBrowserState
  // to ths class which can store user's tab mode before Bookmarks is open.
  [self bookmarkHomeViewControllerWantsDismissal:controller
                                navigationToUrls:urls
                                     inIncognito:NO
                                          newTab:NO];
}

- (void)bookmarkHomeViewControllerWantsDismissal:
            (BookmarkHomeViewController*)controller
                                navigationToUrls:(const std::vector<GURL>&)urls
                                     inIncognito:(BOOL)inIncognito
                                          newTab:(BOOL)newTab {
  // TODO(crbug.com/695749): Rewrite this function when adding bookmark becomes
  // available in chrome_clean_skeleton.  See if we can share the code from
  // bookmark_home_view_controller.

  [self dismissBookmarkBrowser];

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
      new_tab_page_uma::RecordAction(self.browser->browser_state(),
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

// Dismisses the bookmark browser when it is present on iPhones.
- (void)dismissBookmarkBrowser {
  if (!self.bookmarkBrowser)
    return;

  [self.bookmarkBrowser dismissModals];
  [self stop];
}

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
