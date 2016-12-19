// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_MENU_VIEW_H_
#define IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_MENU_VIEW_H_

#import <UIKit/UIKit.h>

@class BookmarkMenuItem;
@class BookmarkMenuView;

namespace ios {
class ChromeBrowserState;
}  // namespace ios

@protocol BookmarkMenuViewDelegate
- (void)bookmarkMenuView:(BookmarkMenuView*)view
        selectedMenuItem:(BookmarkMenuItem*)menuItem;
@end

// This view consists of a table view that shows all the relevant menu items.
@interface BookmarkMenuView : UIView

@property(nonatomic, assign) id<BookmarkMenuViewDelegate> delegate;
@property(nonatomic, readonly) BookmarkMenuItem* defaultMenuItem;

// Designated initializer.
- (instancetype)initWithBrowserState:(ios::ChromeBrowserState*)browserState
                               frame:(CGRect)frame;
// The primary menu item is blue instead of gray.
- (void)updatePrimaryMenuItem:(BookmarkMenuItem*)menuItem;
// Applies |scrollsToTop| to the menu view.
- (void)setScrollsToTop:(BOOL)scrollsToTop;

@end

#endif  // IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_MENU_VIEW_H_
