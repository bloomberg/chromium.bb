// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_INTERACTION_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_INTERACTION_CONTROLLER_H_

#import <UIKit/UIKit.h>

@class Tab;
@protocol UrlLoader;

namespace bookmarks {
class BookmarkNode;
}  // namespace bookmarks

namespace ios {
class ChromeBrowserState;
}  // namespace ios

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

// The BookmarkInteractionController abstracts the management of the various
// UIViewControllers used to create, remove and edit a bookmark. Its main entry
// point is called when the user taps on the star icon.
@interface BookmarkInteractionController : NSObject

// Registers the feature preferences.
+ (void)registerBrowserStatePrefs:(user_prefs::PrefRegistrySyncable*)registry;

// Accesses the default folder for new bookmarks. The default folder is Mobile
// Bookmarks.
+ (const bookmarks::BookmarkNode*)folderForNewBookmarksInBrowserState:
    (ios::ChromeBrowserState*)browserState;
+ (void)setFolderForNewBookmarks:(const bookmarks::BookmarkNode*)folder
                  inBrowserState:(ios::ChromeBrowserState*)browserState;

- (instancetype)initWithBrowserState:(ios::ChromeBrowserState*)browserState
                              loader:(id<UrlLoader>)loader
                    parentController:(UIViewController*)parentController
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// Presents the bookmark UI for a single bookmark. The |parentView| and |origin|
// are hints that may or may not be used in how the UI for a single bookmark
// will appear.
// Subclasses must override this method.
- (void)presentBookmarkForTab:(Tab*)tab
          currentlyBookmarked:(BOOL)bookmarked
                       inView:(UIView*)parentView
                   originRect:(CGRect)origin;

// Presents the bookmarks browser modally. Subclasses must override this method.
- (void)presentBookmarks;

// Removes any bookmark modal controller from view if visible. Subclasses must
// override this method.
- (void)dismissBookmarkModalControllerAnimated:(BOOL)animated;

// Removes any snackbar related to bookmarks that could have been presented. The
// default implementation is a no-op.
- (void)dismissSnackbar;

@end

#endif  // IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_INTERACTION_CONTROLLER_H_
