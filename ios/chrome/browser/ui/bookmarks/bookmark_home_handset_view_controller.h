// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_HOME_HANDSET_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_HOME_HANDSET_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/ui/bookmarks/bookmark_home_view_controller.h"

// Navigate/edit the bookmark hierarchy on a handset.
@interface BookmarkHomeHandsetViewController : BookmarkHomeViewController
// Designated initializer.
- (instancetype)initWithLoader:(id<UrlLoader>)loader
                  browserState:(ios::ChromeBrowserState*)browserState;
@end

@interface BookmarkHomeHandsetViewController (ExposedForTesting)
// Creates the default view to show all bookmarks, if it doesn't already exist.
- (void)ensureAllViewExists;
@end

#endif  // IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_HOME_HANDSET_VIEW_CONTROLLER_H_
