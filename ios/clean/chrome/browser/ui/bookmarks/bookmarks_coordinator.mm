// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/bookmarks/bookmarks_coordinator.h"

#import "ios/chrome/browser/ui/bookmarks/bookmark_controller_factory.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_home_handset_view_controller.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_home_tablet_ntp_controller.h"
#import "ios/chrome/browser/ui/browser_list/browser.h"
#import "ios/chrome/browser/ui/coordinators/browser_coordinator+internal.h"
#include "ios/chrome/browser/ui/ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface BookmarksCoordinator ()
@property(nonatomic, strong) UIViewController* viewController;
@end

@implementation BookmarksCoordinator
@synthesize viewController = _viewController;

- (void)start {
  // HACK: Re-using old view controllers for now.
  if (!IsIPadIdiom()) {
    self.viewController = [[BookmarkHomeHandsetViewController alloc]
        initWithLoader:nil
          browserState:self.browser->browser_state()];
    self.viewController.modalPresentationStyle = UIModalPresentationFormSheet;
  } else {
    BookmarkControllerFactory* factory =
        [[BookmarkControllerFactory alloc] init];
    self.viewController = [factory
        bookmarkPanelControllerForBrowserState:self.browser->browser_state()
                                        loader:nil];
  }
  [super start];
}

@end
