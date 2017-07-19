// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_HOME_HANDSET_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_HOME_HANDSET_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/ui/bookmarks/bookmark_home_view_controller.h"

#import <UIKit/UIKit.h>

#include <set>
#include <vector>

@class BookmarkHomeHandsetViewController;
class GURL;

@protocol UrlLoader;

@protocol BookmarkHomeHandsetViewControllerDelegate
// The view controller wants to be dismissed.
// If |url| != GURL(), then the user has selected |url| for navigation.
- (void)bookmarkHomeHandsetViewControllerWantsDismissal:
            (BookmarkHomeHandsetViewController*)controller
                                        navigationToUrl:(const GURL&)url;
@end

// Navigate/edit the bookmark hierarchy on a handset.
@interface BookmarkHomeHandsetViewController : BookmarkHomeViewController

#pragma mark - Properties Relevant To Presenters

@property(nonatomic, weak) id<BookmarkHomeHandsetViewControllerDelegate>
    delegate;

// Dismisses any modal interaction elements. The base implementation does
// nothing.
- (void)dismissModals:(BOOL)animated;

@end

#endif  // IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_HOME_HANDSET_VIEW_CONTROLLER_H_
