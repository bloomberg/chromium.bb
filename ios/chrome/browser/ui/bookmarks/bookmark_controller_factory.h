// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_CONTROLLER_FACTORY_H_
#define IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_CONTROLLER_FACTORY_H_

#import <Foundation/Foundation.h>

@class BookmarkHomeViewController;
@protocol NewTabPagePanelProtocol;
@protocol UrlLoader;

namespace ios {
class ChromeBrowserState;
}  // namespace ios

// This factory is responsible for providing an instance of a bookmark
// controller that can browse and edit the bookmark hierarchy.
@interface BookmarkControllerFactory : NSObject

- (BookmarkHomeViewController*)
bookmarkControllerWithBrowserState:(ios::ChromeBrowserState*)browserState
                            loader:(id<UrlLoader>)loader;

// Returns an instance of a NewTabPagePanelProtocol that can navigate and edit
// the bookmark hierarchy.
- (id<NewTabPagePanelProtocol>)
bookmarkPanelControllerForBrowserState:(ios::ChromeBrowserState*)browserState
                                loader:(id<UrlLoader>)loader
                            colorCache:(NSMutableDictionary*)cache;

@end

#endif  // IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_CONTROLLER_FACTORY_H_
