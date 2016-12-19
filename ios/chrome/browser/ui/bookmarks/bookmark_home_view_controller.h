// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_HOME_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_HOME_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/ui/ntp/new_tab_page_panel_protocol.h"

#include <set>
#include <vector>

@class BookmarkCollectionView;
class GURL;
@protocol UrlLoader;

namespace bookmarks {
class BookmarkModel;
class BookmarkNode;
}  // namespace bookmarks

namespace ios {
class ChromeBrowserState;
}  // namespace ios

@class BookmarkHomeViewController;
@protocol BookmarkHomeViewControllerDelegate
// The view controller wants to be dismissed.
// If |url| != GURL(), then the user has selected |url| for navigation.
- (void)bookmarkHomeViewControllerWantsDismissal:
            (BookmarkHomeViewController*)controller
                                 navigationToUrl:(const GURL&)url;
@end

// Full screen view controller for browsing the bookmark hierarchy, batch
// editing bookmarks, and navigating to a single bookmark.
// Abstracty base class that provides common, non-UI functionality.
@interface BookmarkHomeViewController : UIViewController {
 @protected
  // The following 2 ivars both represent the set of nodes being edited.
  // The set is for fast lookup.
  // The vector maintains the order that edit nodes were added.
  // Use the relevant instance methods to modify these two ivars in tandem.
  // DO NOT modify these two ivars directly.
  std::set<const bookmarks::BookmarkNode*> _editNodes;
  std::vector<const bookmarks::BookmarkNode*> _editNodesOrdered;
}
// Designated initializer.
- (instancetype)initWithLoader:(id<UrlLoader>)loader
                  browserState:(ios::ChromeBrowserState*)browserState;

#pragma mark - Properties Relevant To Presenters

@property(nonatomic, assign) id<BookmarkHomeViewControllerDelegate> delegate;

#pragma mark - Properties Relevant To Subclasses

// Whether the view controller is in editing mode.
@property(nonatomic, assign, readonly) BOOL editing;
// The set of selected index paths for edition.
@property(nonatomic, retain, readonly) NSMutableArray* editIndexPaths;
@property(nonatomic, assign, readonly) bookmarks::BookmarkModel* bookmarks;
@property(nonatomic, assign, readonly) id<UrlLoader> loader;
@property(nonatomic, assign, readonly) ios::ChromeBrowserState* browserState;

#pragma mark - Relevant Methods
// Replaces |_editNodes| and |_editNodesOrdered| with new container objects.
- (void)resetEditNodes;
// Adds |node| and |indexPath| if it isn't already present.
- (void)insertEditNode:(const bookmarks::BookmarkNode*)node
           atIndexPath:(NSIndexPath*)indexPath;
// Removes |node| and |indexPath| if it's present.
- (void)removeEditNode:(const bookmarks::BookmarkNode*)node
           atIndexPath:(NSIndexPath*)indexPath;

#pragma mark - Methods Intended For Subclass Override

// Subclasses must call the super class implementation.
// This method updates the property, and resets the edit nodes.
- (void)setEditing:(BOOL)editing animated:(BOOL)animated;

// Dismisses any modal interaction elements. The base implementation does
// nothing.
- (void)dismissModals:(BOOL)animated;

@end

@interface BookmarkHomeViewController (PrivateAPIExposedForTesting)
- (const std::set<const bookmarks::BookmarkNode*>&)editNodes;
- (void)setEditNodes:(const std::set<const bookmarks::BookmarkNode*>&)editNodes;
@end

#endif  // IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_HOME_VIEW_CONTROLLER_H_
