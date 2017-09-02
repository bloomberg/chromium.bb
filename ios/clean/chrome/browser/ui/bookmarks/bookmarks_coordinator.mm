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
#include "ios/chrome/browser/ui/ui_util.h"
#import "ios/clean/chrome/browser/ui/url_loader_adaptor.h"
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
@end

@implementation BookmarksCoordinator
@synthesize contained = _contained;
@synthesize viewController = _viewController;
@synthesize bookmarkBrowser = _bookmarkBrowser;
@synthesize loader = _loader;

- (void)start {
  if (self.started)
    return;

  self.loader = [[URLLoaderAdaptor alloc] init];
  if (!self.contained) {
    BookmarkControllerFactory* bookmarkControllerFactory =
        [[BookmarkControllerFactory alloc] init];
    self.bookmarkBrowser = [bookmarkControllerFactory
        bookmarkControllerWithBrowserState:self.browser->browser_state()
                                    loader:self.loader];
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
          browserState:self.browser->browser_state()];
  }
  self.loader.viewControllerForAlert = self.viewController;
  [super start];
}

- (void)stop {
  self.bookmarkBrowser.homeDelegate = nil;
  self.bookmarkBrowser = nil;
  [super stop];
}

#pragma mark - BookmarkHomeViewControllerDelegate

- (void)bookmarkHomeViewControllerWantsDismissal:
            (BookmarkHomeViewController*)controller
                                 navigationToUrl:(const GURL&)url {
  [self dismissBookmarkBrowser];

  if (url != GURL()) {
    new_tab_page_uma::RecordAction(self.browser->browser_state(),
                                   new_tab_page_uma::ACTION_OPENED_BOOKMARK);
    base::RecordAction(
        base::UserMetricsAction("MobileBookmarkManagerEntryOpened"));

    if (url.SchemeIs(url::kJavaScriptScheme)) {  // bookmarklet
      NSString* jsToEval = [base::SysUTF8ToNSString(url.GetContent())
          stringByRemovingPercentEncoding];
      [self.loader loadJavaScriptFromLocationBar:jsToEval];
    } else {
      [self.loader loadURL:url
                   referrer:web::Referrer()
                 transition:ui::PAGE_TRANSITION_AUTO_BOOKMARK
          rendererInitiated:NO];
    }
  }
}

#pragma mark - Private

// Dismisses the bookmark browser when it is present on iPhones.
- (void)dismissBookmarkBrowser {
  if (!self.bookmarkBrowser)
    return;

  [self.bookmarkBrowser dismissModals];
  [self stop];
}

@end
