// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bookmarks/bookmark_table_view.h"

#include "components/bookmarks/browser/bookmark_model.h"
#include "ios/chrome/browser/bookmarks/bookmark_model_factory.h"
#include "ios/chrome/browser/bookmarks/bookmarks_utils.h"
#include "ios/chrome/browser/ui/bookmarks/bookmark_collection_view_background.h"
#include "ios/chrome/browser/ui/bookmarks/bookmark_model_bridge_observer.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_utils_ios.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using bookmarks::BookmarkNode;

@interface BookmarkTableView ()<UITableViewDataSource,
                                UITableViewDelegate,
                                BookmarkModelBridgeObserver> {
  // A vector of bookmark nodes to display in the table view.
  std::vector<const BookmarkNode*> _bookmarkItems;
  const BookmarkNode* _currentRootNode;
  // Bridge to register for bookmark changes.
  std::unique_ptr<bookmarks::BookmarkModelBridge> _modelBridge;
}

// The UITableView to show bookmarks.
@property(nonatomic, strong) UITableView* tableView;
// The model holding bookmark data.
@property(nonatomic, assign) bookmarks::BookmarkModel* bookmarkModel;
// The browser state.
@property(nonatomic, assign) ios::ChromeBrowserState* browserState;
// The delegate for actions on the table.
@property(nonatomic, assign) id<BookmarkTableViewDelegate> delegate;
// Background view of the collection view shown when there is no items.
@property(nonatomic, strong)
    BookmarkCollectionViewBackground* emptyTableBackgroundView;

@end

@implementation BookmarkTableView

@synthesize bookmarkModel = _bookmarkModel;
@synthesize browserState = _browserState;
@synthesize tableView = _tableView;
@synthesize delegate = _delegate;
@synthesize emptyTableBackgroundView = _emptyTableBackgroundView;

// TODO(crbug.com/695749) Add promo section, bottom context bar,
// promo view and register kIosBookmarkSigninPromoDisplayedCount.

- (instancetype)initWithBrowserState:(ios::ChromeBrowserState*)browserState
                            delegate:(id<BookmarkTableViewDelegate>)delegate
                            rootNode:(const BookmarkNode*)rootNode
                               frame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    DCHECK(rootNode);
    _browserState = browserState;
    _delegate = delegate;
    _currentRootNode = rootNode;

    // Set up connection to the BookmarkModel.
    _bookmarkModel =
        ios::BookmarkModelFactory::GetForBrowserState(browserState);

    // Set up observers.
    _modelBridge.reset(
        new bookmarks::BookmarkModelBridge(self, _bookmarkModel));

    [self computeBookmarkTableViewData];

    self.tableView =
        [[UITableView alloc] initWithFrame:frame style:UITableViewStylePlain];
    self.tableView.dataSource = self;
    self.tableView.delegate = self;
    // Remove extra rows.
    self.tableView.tableFooterView = [[UIView alloc] initWithFrame:CGRectZero];
    self.tableView.autoresizingMask =
        UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    [self addSubview:self.tableView];
    [self bringSubviewToFront:self.tableView];

    // Set up the background view shown when the table is empty.
    self.emptyTableBackgroundView =
        [[BookmarkCollectionViewBackground alloc] initWithFrame:frame];
    self.emptyTableBackgroundView.autoresizingMask =
        UIViewAutoresizingFlexibleHeight | UIViewAutoresizingFlexibleWidth;
    self.emptyTableBackgroundView.text =
        l10n_util::GetNSString(IDS_IOS_BOOKMARK_NO_BOOKMARKS_LABEL);
    [self updateEmptyBackground];
    [self addSubview:self.emptyTableBackgroundView];
  }
  return self;
}

#pragma mark - UITableViewDataSource

- (NSInteger)numberOfSectionsInTableView:(UITableView*)tableView {
  // TODO(crbug.com/695749) Add promo section check here.
  return 1;
}

- (NSInteger)tableView:(UITableView*)tableView
    numberOfRowsInSection:(NSInteger)section {
  // TODO(crbug.com/695749) Add promo section check here.
  return _bookmarkItems.size();
}

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  const BookmarkNode* node = [self nodeAtIndexPath:indexPath];
  static NSString* bookmarkCellIdentifier = @"bookmarkCellIdentifier";

  // TODO(crbug.com/695749) Use custom cells.
  UITableViewCell* cell =
      [tableView dequeueReusableCellWithIdentifier:bookmarkCellIdentifier];

  if (cell == nil) {
    cell = [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleDefault
                                  reuseIdentifier:bookmarkCellIdentifier];
  }

  cell.textLabel.text = bookmark_utils_ios::TitleForBookmarkNode(node);
  cell.textLabel.accessibilityIdentifier = cell.textLabel.text;
  if (node->is_folder()) {
    [cell setAccessoryType:UITableViewCellAccessoryDisclosureIndicator];
  } else {
    [cell setAccessoryType:UITableViewCellAccessoryNone];
  }
  return cell;
}

- (BOOL)tableView:(UITableView*)tableView
    canEditRowAtIndexPath:(NSIndexPath*)indexPath {
  // We enable the swipe-to-delete gesture and reordering control for nodes of
  // type URL or Folder, and not the permanent ones.
  const BookmarkNode* node = [self nodeAtIndexPath:indexPath];
  return node->type() == BookmarkNode::URL ||
         node->type() == BookmarkNode::FOLDER;
}

- (void)tableView:(UITableView*)tableView
    commitEditingStyle:(UITableViewCellEditingStyle)editingStyle
     forRowAtIndexPath:(NSIndexPath*)indexPath {
  if (editingStyle == UITableViewCellEditingStyleDelete) {
    const BookmarkNode* node = [self nodeAtIndexPath:indexPath];
    std::set<const BookmarkNode*> nodes;
    nodes.insert(node);
    [self.delegate bookmarkTableView:self selectedNodesForDeletion:nodes];
  }
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  // TODO(crbug.com/695749) Add promo section check here.
  const BookmarkNode* node = [self nodeAtIndexPath:indexPath];
  DCHECK(node);
  if (node->is_folder()) {
    [self.delegate bookmarkTableView:self selectedFolderForNavigation:node];
  } else {
    // Open URL. Pass this to the delegate.
    [self.delegate bookmarkTableView:self selectedUrlForNavigation:node->url()];
  }
  // Deselect row.
  [tableView deselectRowAtIndexPath:indexPath animated:YES];
}

#pragma mark - BookmarkModelBridgeObserver Callbacks
// BookmarkModelBridgeObserver Callbacks
// Instances of this class automatically observe the bookmark model.
// The bookmark model has loaded.
- (void)bookmarkModelLoaded {
  [self refreshContents];
}

// The node has changed, but not its children.
- (void)bookmarkNodeChanged:(const BookmarkNode*)bookmarkNode {
  // The root folder changed. Do nothing.
  if (bookmarkNode == _currentRootNode)
    return;

  // A specific cell changed. Reload, if currently shown.
  if (std::find(_bookmarkItems.begin(), _bookmarkItems.end(), bookmarkNode) !=
      _bookmarkItems.end()) {
    [self refreshContents];
  }
}

// The node has not changed, but its children have.
- (void)bookmarkNodeChildrenChanged:(const BookmarkNode*)bookmarkNode {
  // The current root folder's children changed. Reload everything.
  if (bookmarkNode == _currentRootNode) {
    [self refreshContents];
    return;
  }
}

// The node has moved to a new parent folder.
- (void)bookmarkNode:(const BookmarkNode*)bookmarkNode
     movedFromParent:(const BookmarkNode*)oldParent
            toParent:(const BookmarkNode*)newParent {
  if (oldParent == _currentRootNode || newParent == _currentRootNode) {
    // A folder was added or removed from the current root folder.
    [self refreshContents];
  }
}

// |node| was deleted from |folder|.
- (void)bookmarkNodeDeleted:(const BookmarkNode*)node
                 fromFolder:(const BookmarkNode*)folder {
  if (_currentRootNode == node) {
    _currentRootNode = NULL;
    [self refreshContents];
  }
}

// All non-permanent nodes have been removed.
- (void)bookmarkModelRemovedAllNodes {
  // TODO(crbug.com/695749) Check if this case is applicable in the new UI.
}

#pragma mark - Private

- (void)refreshContents {
  [self computeBookmarkTableViewData];
  [self updateEmptyBackground];
  [self.tableView reloadData];
}

// Returns the bookmark node associated with |indexPath|.
- (const BookmarkNode*)nodeAtIndexPath:(NSIndexPath*)indexPath {
  // TODO(crbug.com/695749) Add check if section is bookmarks.
  return _bookmarkItems[indexPath.row];

  NOTREACHED();
  return nullptr;
}

// Computes the bookmarks table view based on the current root node.
- (void)computeBookmarkTableViewData {
  if (!self.bookmarkModel->loaded() || _currentRootNode == NULL)
    return;

  // Regenerate the list of all bookmarks.
  _bookmarkItems.clear();
  int childCount = _currentRootNode->child_count();
  for (int i = 0; i < childCount; ++i) {
    const BookmarkNode* node = _currentRootNode->GetChild(i);
    _bookmarkItems.push_back(node);
  }
}

- (void)updateEmptyBackground {
  self.emptyTableBackgroundView.alpha =
      _currentRootNode != NULL && _currentRootNode->child_count() > 0 ? 0 : 1;
}

@end
