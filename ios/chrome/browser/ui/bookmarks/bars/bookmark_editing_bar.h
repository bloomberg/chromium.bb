// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BOOKMARKS_BARS_BOOKMARK_EDITING_BAR_H_
#define IOS_CHROME_BROWSER_UI_BOOKMARKS_BARS_BOOKMARK_EDITING_BAR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/bookmarks/bars/bookmark_top_bar.h"

// The top bar that shows up when a user is editing bookmarks.
@interface BookmarkEditingBar : BookmarkTopBar
// Set the target/action of the respective buttons.
// The cancel button ends editing mode.
- (void)setCancelTarget:(id)target action:(SEL)action;
// The edit button edits a single bookmark/folder.
- (void)setEditTarget:(id)target action:(SEL)action;
// The move button moves any number of bookmarks/folders.
- (void)setMoveTarget:(id)target action:(SEL)action;
// The delete button moves any number of bookmarks/folders.
- (void)setDeleteTarget:(id)target action:(SEL)action;

// The UI changes based on the number of bookmarks selected for editing.
- (void)updateUIWithBookmarkCount:(int)bookmarkCount
                      folderCount:(int)folderCount;
// Instantaneously shows or hides the shadow.
- (void)showShadow:(BOOL)show;
@end

#endif  // IOS_CHROME_BROWSER_UI_BOOKMARKS_BARS_BOOKMARK_EDITING_BAR_H_
