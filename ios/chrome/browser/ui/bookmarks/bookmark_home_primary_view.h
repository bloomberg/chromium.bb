// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_HOME_PRIMARY_VIEW_H_
#define IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_HOME_PRIMARY_VIEW_H_

#import <UIKit/UIKit.h>

// Interface for views that can be set as primary views in the bookmark home
// controllers (BookmarkHomeHandsetViewController and
// BookmarkHomeTabletViewController).
@protocol BookmarkHomePrimaryView<NSObject>

// The collection view of the primary view.
- (UICollectionView*)collectionView;

// Regardless of the current orientation, returns the y of the content offset of
// the receiver if it were to have portrait orientation.
- (CGFloat)contentPositionInPortraitOrientation;

// Called when the content position needs to be updated.
- (void)applyContentPosition:(CGFloat)position;

// Called when the orientation changes.
// TODO(crbug.com/390548): Remove the use of this method.
- (void)changeOrientation:(UIInterfaceOrientation)orientation;

// If the receiver contains a scrollview, this is the occasion to set it up as
// scrolling to top when the status bar is tapped.
- (void)setScrollsToTop:(BOOL)scrollsToTop;

// Called when the receiver should change itself to editing mode.
- (void)setEditing:(BOOL)editing animated:(BOOL)animated;

@end

#endif  // IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_HOME_PRIMARY_VIEW_H_
