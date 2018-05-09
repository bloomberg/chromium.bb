// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_HOME_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_HOME_CONSUMER_H_

#import "ios/chrome/browser/ui/table_view/chrome_table_view_consumer.h"

// BookmarkHomeConsumer provides methods that allow mediators to update the UI.
@protocol BookmarkHomeConsumer<ChromeTableViewConsumer>

// Refreshes the UI.
- (void)refreshContents;

// Starts an asynchronous favicon load for the row at the given |indexPath|. Can
// optionally fetch a favicon from a Google server if nothing suitable is found
// locally; otherwise uses the fallback icon style.
- (void)loadFaviconAtIndexPath:(NSIndexPath*)indexPath
        continueToGoogleServer:(BOOL)continueToGoogleServer;

@end

#endif  // IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_HOME_CONSUMER_H_
