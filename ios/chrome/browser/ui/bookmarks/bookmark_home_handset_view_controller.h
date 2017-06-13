// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_HOME_HANDSET_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_HOME_HANDSET_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#include <set>
#include <vector>

@class BookmarkHomeHandsetViewController;
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

@protocol BookmarkHomeHandsetViewControllerDelegate
// The view controller wants to be dismissed.
// If |url| != GURL(), then the user has selected |url| for navigation.
- (void)bookmarkHomeHandsetViewControllerWantsDismissal:
            (BookmarkHomeHandsetViewController*)controller
                                        navigationToUrl:(const GURL&)url;
@end

// Navigate/edit the bookmark hierarchy on a handset.
@interface BookmarkHomeHandsetViewController : UIViewController {
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

@property(nonatomic, weak) id<BookmarkHomeHandsetViewControllerDelegate>
    delegate;

#pragma mark - Properties
// Whether the view controller is in editing mode.
@property(nonatomic, assign, readonly) BOOL editing;
// The set of selected index paths for edition.
@property(nonatomic, strong, readonly) NSMutableArray* editIndexPaths;
@property(nonatomic, assign, readonly) bookmarks::BookmarkModel* bookmarks;
@property(nonatomic, weak, readonly) id<UrlLoader> loader;
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

// This method updates the property, and resets the edit nodes.
- (void)setEditing:(BOOL)editing animated:(BOOL)animated;

// Dismisses any modal interaction elements. The base implementation does
// nothing.
- (void)dismissModals:(BOOL)animated;

@end

@interface BookmarkHomeHandsetViewController (ExposedForTesting)
// Creates the default view to show all bookmarks, if it doesn't already exist.
- (void)ensureAllViewExists;
- (const std::set<const bookmarks::BookmarkNode*>&)editNodes;
- (void)setEditNodes:(const std::set<const bookmarks::BookmarkNode*>&)editNodes;
@end

#endif  // IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_HOME_HANDSET_VIEW_CONTROLLER_H_
