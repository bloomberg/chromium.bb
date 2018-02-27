// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/ntp/recent_tabs/recent_tabs_table_view_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation RecentTabsTableViewController : ChromeTableViewController
@synthesize browserState = _browserState;
@synthesize dispatcher = _dispatcher;
@synthesize loader = _loader;

#pragma mark - Public Interface

- (instancetype)initWithBrowserState:(ios::ChromeBrowserState*)browserState
                              loader:(id<UrlLoader>)loader
                          dispatcher:(id<ApplicationCommands>)dispatcher {
  self = [super initWithStyle:UITableViewStylePlain];
  if (self) {
    _dispatcher = dispatcher;
    _loader = loader;
    _browserState = browserState;
  }
  return self;
}

// TODO(crbug.com/805135)
- (void)refreshUserState:(SessionsSyncUserState)state {
}

// TODO(crbug.com/805135)
- (void)refreshRecentlyClosedTabs {
}

// TODO(crbug.com/805135)
- (void)setTabRestoreService:(sessions::TabRestoreService*)tabRestoreService {
}
// TODO(crbug.com/805135)
- (void)dismissModals {
}

@end
