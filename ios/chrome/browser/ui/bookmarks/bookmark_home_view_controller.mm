// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bookmarks/bookmark_home_view_controller.h"

#include "base/mac/objc_property_releaser.h"
#include "base/mac/scoped_nsobject.h"
#include "components/bookmarks/browser/base_bookmark_model_observer.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "ios/chrome/browser/bookmarks/bookmark_model_factory.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_collection_view.h"
#import "ios/chrome/browser/ui/url_loader.h"

using bookmarks::BookmarkNode;

@interface BookmarkHomeViewController () {
  base::mac::ObjCPropertyReleaser _propertyReleaser_BookmarkHomeViewController;
}

// Redefined to be readwrite.
@property(nonatomic, retain, readwrite) NSMutableArray* editIndexPaths;

// Returns the parent, if all the bookmarks are siblings.
// Otherwise returns the mobile_node.
+ (const BookmarkNode*)
defaultMoveFolderFromBookmarks:(const std::set<const BookmarkNode*>&)bookmarks
                         model:(bookmarks::BookmarkModel*)model;
@end

@implementation BookmarkHomeViewController
@synthesize delegate = _delegate;
@synthesize editIndexPaths = _editIndexPaths;
@synthesize editing = _editing;
@synthesize bookmarks = _bookmarks;
@synthesize loader = _loader;
@synthesize browserState = _browserState;

- (instancetype)initWithLoader:(id<UrlLoader>)loader
                  browserState:(ios::ChromeBrowserState*)browserState {
  DCHECK(browserState);
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _propertyReleaser_BookmarkHomeViewController.Init(
        self, [BookmarkHomeViewController class]);

    _browserState = browserState->GetOriginalChromeBrowserState();
    _loader = loader;

    _bookmarks = ios::BookmarkModelFactory::GetForBrowserState(_browserState);
    _editIndexPaths = [[NSMutableArray alloc] init];

    [self resetEditNodes];
  }
  return self;
}

- (void)loadView {
  CGRect frame = [[UIScreen mainScreen] bounds];
  self.view =
      base::scoped_nsobject<UIView>([[UIView alloc] initWithFrame:frame]);
}

- (void)resetEditNodes {
  _editNodes = std::set<const BookmarkNode*>();
  _editNodesOrdered = std::vector<const BookmarkNode*>();
  [self.editIndexPaths removeAllObjects];
}

- (void)insertEditNode:(const BookmarkNode*)node
           atIndexPath:(NSIndexPath*)indexPath {
  if (_editNodes.find(node) != _editNodes.end())
    return;
  _editNodes.insert(node);
  _editNodesOrdered.push_back(node);
  if (indexPath) {
    [self.editIndexPaths addObject:indexPath];
  } else {
    // We insert null if we don't have the cell to keep the index valid.
    [self.editIndexPaths addObject:[NSNull null]];
  }
}

- (void)removeEditNode:(const BookmarkNode*)node
           atIndexPath:(NSIndexPath*)indexPath {
  if (_editNodes.find(node) == _editNodes.end())
    return;

  _editNodes.erase(node);
  std::vector<const BookmarkNode*>::iterator it =
      std::find(_editNodesOrdered.begin(), _editNodesOrdered.end(), node);
  DCHECK(it != _editNodesOrdered.end());
  _editNodesOrdered.erase(it);
  if (indexPath) {
    [self.editIndexPaths removeObject:indexPath];
  } else {
    // If we don't have the cell, we remove it by using its index.
    const NSUInteger index = std::distance(_editNodesOrdered.begin(), it);
    if (index < self.editIndexPaths.count) {
      [self.editIndexPaths removeObjectAtIndex:index];
    }
  }
}

- (void)setEditing:(BOOL)editing animated:(BOOL)animated {
  if (_editing == editing)
    return;

  _editing = editing;

  // Only reset the editing state when leaving edit mode. This allows subclasses
  // to add nodes for editing before entering edit mode.
  if (!editing)
    [self resetEditNodes];
}

- (void)dismissModals:(BOOL)animated {
  // Do nothing in base class.
}

+ (const BookmarkNode*)
defaultMoveFolderFromBookmarks:(const std::set<const BookmarkNode*>&)bookmarks
                         model:(bookmarks::BookmarkModel*)model {
  if (bookmarks.size() == 0)
    return model->mobile_node();
  const BookmarkNode* firstParent = (*(bookmarks.begin()))->parent();
  for (const BookmarkNode* node : bookmarks) {
    if (node->parent() != firstParent)
      return model->mobile_node();
  }

  return firstParent;
}

@end

@implementation BookmarkHomeViewController (PrivateAPIExposedForTesting)

- (const std::set<const BookmarkNode*>&)editNodes {
  return _editNodes;
}

- (void)setEditNodes:(const std::set<const BookmarkNode*>&)editNodes {
  _editNodes = editNodes;
}

@end
