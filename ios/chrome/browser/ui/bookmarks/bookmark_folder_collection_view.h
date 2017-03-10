// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_FOLDER_COLLECTION_VIEW_H_
#define IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_FOLDER_COLLECTION_VIEW_H_

#import "ios/chrome/browser/ui/bookmarks/bookmark_collection_view.h"

@class BookmarkFolderCollectionView;

namespace bookmarks {
class BookmarkNode;
}  // namespace bookmarks

@protocol BookmarkFolderCollectionViewDelegate<BookmarkCollectionViewDelegate>
// Tells the delegate that a folder was selected for navigation.
- (void)bookmarkFolderCollectionView:(BookmarkFolderCollectionView*)view
         selectedFolderForNavigation:(const bookmarks::BookmarkNode*)folder;
@end

// Shows all sub-folders and sub-urls of a folder node in a collection view.
// Note: This class intentionally does not try to maintain state through a
// folder transition. Depending on the type of animation that the designers
// choose, we may require multiple instances of this view.
@interface BookmarkFolderCollectionView : BookmarkCollectionView

// Refreshes the entire view to reflect |folder|.
- (void)resetFolder:(const bookmarks::BookmarkNode*)folder;

// Called when something outside the view causes the promo state to change.
- (void)promoStateChangedAnimated:(BOOL)animate;

@property(nonatomic, assign) id<BookmarkFolderCollectionViewDelegate> delegate;
@property(nonatomic, assign, readonly) const bookmarks::BookmarkNode* folder;

@end

#endif  // IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_FOLDER_COLLECTION_VIEW_H_
