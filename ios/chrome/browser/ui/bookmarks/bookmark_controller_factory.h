// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_CONTROLLER_FACTORY_H_
#define IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_CONTROLLER_FACTORY_H_

#import <Foundation/Foundation.h>

@protocol ApplicationCommands;
@class BookmarkHomeViewController;
@protocol NewTabPagePanelProtocol;
@protocol UrlLoader;

namespace ios {
class ChromeBrowserState;
}  // namespace ios

// TODO(crbug.com/753599) : Remove this class after
// BookmarkHomeHandsetViewController merged into BookmarkHomeViewController.
// This factory is responsible for providing an instance of a bookmark
// controller that can browse and edit the bookmark hierarchy.
@interface BookmarkControllerFactory : NSObject

// Returns an instance of BookmarkHomeHandsetViewController.
- (BookmarkHomeViewController*)
bookmarkControllerWithBrowserState:(ios::ChromeBrowserState*)browserState
                            loader:(id<UrlLoader>)loader
                        dispatcher:(id<ApplicationCommands>)dispatcher;

@end

#endif  // IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_CONTROLLER_FACTORY_H_
