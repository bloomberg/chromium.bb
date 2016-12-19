// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/spotlight/spotlight_manager.h"

#include "base/logging.h"
#include "base/mac/scoped_nsobject.h"
#include "ios/chrome/app/spotlight/actions_spotlight_manager.h"
#include "ios/chrome/app/spotlight/bookmarks_spotlight_manager.h"
#include "ios/chrome/app/spotlight/topsites_spotlight_manager.h"
#include "ios/chrome/browser/experimental_flags.h"

// Called from the BrowserBookmarkModelBridge from C++ -> ObjC.
@interface SpotlightManager ()<BookmarkUpdatedDelegate> {
  base::scoped_nsobject<BookmarksSpotlightManager> _bookmarkManager;
  base::scoped_nsobject<TopSitesSpotlightManager> _topSitesManager;
  base::scoped_nsobject<ActionsSpotlightManager> _actionsManager;
}

- (instancetype)initWithBrowserState:(ios::ChromeBrowserState*)browserState
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;
@end

@implementation SpotlightManager

+ (SpotlightManager*)spotlightManagerWithBrowserState:
    (ios::ChromeBrowserState*)browserState {
  DCHECK(spotlight::IsSpotlightAvailable());
  return [[[SpotlightManager alloc] initWithBrowserState:browserState]
      autorelease];
}

- (instancetype)initWithBrowserState:(ios::ChromeBrowserState*)browserState {
  self = [super init];
  if (self) {
    _topSitesManager.reset([[TopSitesSpotlightManager
        topSitesSpotlightManagerWithBrowserState:browserState] retain]);
    _bookmarkManager.reset([[BookmarksSpotlightManager
        bookmarksSpotlightManagerWithBrowserState:browserState] retain]);
    [_bookmarkManager setDelegate:self];
    _actionsManager.reset(
        [[ActionsSpotlightManager actionsSpotlightManager] retain]);
  }
  return self;
}

- (instancetype)init {
  NOTREACHED();
  return nil;
}

- (void)dealloc {
  [super dealloc];
}

- (void)resyncIndex {
  [_bookmarkManager reindexBookmarksIfNeeded];
  [_actionsManager indexActions];
}

- (void)bookmarkUpdated {
  [_topSitesManager reindexTopSites];
}

@end
