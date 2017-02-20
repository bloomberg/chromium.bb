// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/spotlight/spotlight_manager.h"

#include "base/logging.h"
#include "ios/chrome/app/spotlight/actions_spotlight_manager.h"
#include "ios/chrome/app/spotlight/bookmarks_spotlight_manager.h"
#include "ios/chrome/app/spotlight/topsites_spotlight_manager.h"
#include "ios/chrome/browser/experimental_flags.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Called from the BrowserBookmarkModelBridge from C++ -> ObjC.
@interface SpotlightManager ()<BookmarkUpdatedDelegate> {
  BookmarksSpotlightManager* _bookmarkManager;
  TopSitesSpotlightManager* _topSitesManager;
  ActionsSpotlightManager* _actionsManager;
}

- (instancetype)initWithBrowserState:(ios::ChromeBrowserState*)browserState
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;
@end

@implementation SpotlightManager

+ (SpotlightManager*)spotlightManagerWithBrowserState:
    (ios::ChromeBrowserState*)browserState {
  if (spotlight::IsSpotlightAvailable()) {
    return [[SpotlightManager alloc] initWithBrowserState:browserState];
  }
  return nil;
}

- (instancetype)initWithBrowserState:(ios::ChromeBrowserState*)browserState {
  DCHECK(spotlight::IsSpotlightAvailable());
  self = [super init];
  if (self) {
    _topSitesManager = [TopSitesSpotlightManager
        topSitesSpotlightManagerWithBrowserState:browserState];
    _bookmarkManager = [BookmarksSpotlightManager
        bookmarksSpotlightManagerWithBrowserState:browserState];
    [_bookmarkManager setDelegate:self];
    _actionsManager = [ActionsSpotlightManager actionsSpotlightManager];
  }
  return self;
}

- (instancetype)init {
  NOTREACHED();
  return nil;
}


- (void)resyncIndex {
  [_bookmarkManager reindexBookmarksIfNeeded];
  [_actionsManager indexActions];
}

- (void)bookmarkUpdated {
  [_topSitesManager reindexTopSites];
}

@end
