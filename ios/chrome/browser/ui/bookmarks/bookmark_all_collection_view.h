// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_ALL_COLLECTION_VIEW_H_
#define IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_ALL_COLLECTION_VIEW_H_

#import "ios/chrome/browser/ui/bookmarks/bookmark_collection_view.h"

// Shows all non-folder bookmarks sorted by most recent creation date.
// The bookmarks are segregated by month into sections.
@interface BookmarkAllCollectionView : BookmarkCollectionView
@property(nonatomic, assign) id<BookmarkCollectionViewDelegate> delegate;
@end

#endif  // IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_ALL_COLLECTION_VIEW_H_
