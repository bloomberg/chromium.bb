// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_TABLE_VIEW_H_
#define IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_TABLE_VIEW_H_

#import <UIKit/UIKit.h>
#include <set>

class GURL;

namespace bookmarks {
class BookmarkNode;
}  // namespace bookmarks

namespace ios {
class ChromeBrowserState;
}

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

@class BookmarkTableView;

// Delegate to handle actions on the table.
@protocol BookmarkTableViewDelegate<NSObject>
// Tells the delegate that a URL was selected for navigation.
- (void)bookmarkTableView:(BookmarkTableView*)view
    selectedUrlForNavigation:(const GURL&)url;

// Tells the delegate that a Folder was selected for navigation.
- (void)bookmarkTableView:(BookmarkTableView*)view
    selectedFolderForNavigation:(const bookmarks::BookmarkNode*)folder;

// Tells the delegate that |nodes| were selected for deletion.
- (void)bookmarkTableView:(BookmarkTableView*)view
    selectedNodesForDeletion:
        (const std::set<const bookmarks::BookmarkNode*>&)nodes;

// Returns true if a bookmarks promo cell should be shown.
- (BOOL)bookmarkTableViewShouldShowPromoCell:(BookmarkTableView*)view;

// Shows a sign-in view controller.
- (void)bookmarkTableViewShowSignIn:(BookmarkTableView*)view;

// Dismisses the promo.
- (void)bookmarkTableViewDismissPromo:(BookmarkTableView*)view;

@end

@interface BookmarkTableView : UIView

// Shows all sub-folders and sub-urls of a folder node (that is set as the root
// node) in a UITableView. Note: This class intentionally does not try to
// maintain state through a folder transition. A new instance of this class is
// pushed on the navigation stack for a different root node.
- (instancetype)initWithBrowserState:(ios::ChromeBrowserState*)browserState
                            delegate:(id<BookmarkTableViewDelegate>)delegate
                            rootNode:(const bookmarks::BookmarkNode*)rootNode
                               frame:(CGRect)frame NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;
- (instancetype)initWithFrame:(CGRect)frame
                        style:(UITableViewStyle)style NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;
+ (instancetype) new NS_UNAVAILABLE;

// Registers the feature preferences.
+ (void)registerBrowserStatePrefs:(user_prefs::PrefRegistrySyncable*)registry;

// Called when something outside the view causes the promo state to change.
- (void)promoStateChangedAnimated:(BOOL)animated;

@end

#endif  // IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_TABLE_VIEW_H_
