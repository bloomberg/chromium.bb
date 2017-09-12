// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bookmarks/bookmark_controller_factory.h"

#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_home_handset_view_controller.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_home_tablet_ntp_controller.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_home_view_controller.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation BookmarkControllerFactory

- (BookmarkHomeViewController*)
bookmarkControllerWithBrowserState:(ios::ChromeBrowserState*)browserState
                            loader:(id<UrlLoader>)loader
                        dispatcher:(id<ApplicationCommands>)dispatcher {
  return (BookmarkHomeViewController*)[[BookmarkHomeHandsetViewController alloc]
      initWithLoader:loader
        browserState:browserState
          dispatcher:dispatcher];
}

- (BookmarkHomeTabletNTPController*)
bookmarkPanelControllerForBrowserState:(ios::ChromeBrowserState*)browserState
                                loader:(id<UrlLoader>)loader
                            dispatcher:(id<ApplicationCommands>)dispatcher {
  return [[BookmarkHomeTabletNTPController alloc] initWithLoader:loader
                                                    browserState:browserState
                                                      dispatcher:dispatcher];
}

@end
