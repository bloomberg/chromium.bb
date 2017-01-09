// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bookmarks/bookmark_all_collection_view.h"

#include <memory>
#include <vector>

#include "base/logging.h"
#include "base/mac/objc_property_releaser.h"
#include "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "ios/chrome/browser/bookmarks/bookmarks_utils.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_collection_cells.h"
#include "ios/chrome/browser/ui/bookmarks/bookmark_promo_cell.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_utils_ios.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/tree_node_iterator.h"

using bookmarks::BookmarkNode;

namespace {
typedef std::vector<const BookmarkNode*> NodeVector;
// Each section of the collection view corresponds to a NodesSection object,
// sorted by creation date.
using bookmark_utils_ios::NodesSection;

// There is sometimes a flurry of events from the bookmark model. When this
// happens the data the collection view is displaying needs to be recalculated
// and the collection view needs to be reloaded. The recalculation of the data
// is expensive, so instead of doing the update over and over again the update
// is done once and all other updates received during the same runloop event are
// deferred to the next turn of the runloop (if everything is deferred this
// unfortunately triggers a bug in the UICollectionView sometimes).
// This enum tracks the state of the refresh. See -collectionViewNeedsUpdate
// for the state machine.
typedef enum { kNoUpdate = 0, kOneUpdateDone, kUpdateScheduled } UpdateState;
}  // namespace

@interface BookmarkAllCollectionView ()<BookmarkPromoCellDelegate> {
  // A vector of vectors. Url nodes are segregated by month of creation.
  std::vector<std::unique_ptr<NodesSection>> _nodesSectionVector;
  // To avoid refreshing the internal model too often.
  UpdateState _updateScheduled;
  base::mac::ObjCPropertyReleaser _propertyReleaser_BookmarkAllCollectionView;
}

// Keep a reference to the promo cell to deregister as delegate.
@property(nonatomic, retain) BookmarkPromoCell* promoCell;

// Triggers an update of the collection, but delayed in order to coallesce a lot
// of events into one update.
- (void)collectionViewNeedsUpdate;

@end

@implementation BookmarkAllCollectionView

@synthesize delegate = _delegate;
@synthesize promoCell = _promoCell;

- (instancetype)initWithBrowserState:(ios::ChromeBrowserState*)browserState
                               frame:(CGRect)frame {
  self = [super initWithBrowserState:browserState frame:frame];
  if (self) {
    _propertyReleaser_BookmarkAllCollectionView.Init(
        self, [BookmarkAllCollectionView class]);
    self.accessibilityIdentifier = @"bookmark_all_collection_view";
    [self updateCollectionView];
  }
  return self;
}

- (void)dealloc {
  _promoCell.delegate = nil;
  [super dealloc];
}

- (void)updateCollectionView {
  if (!self.bookmarkModel->loaded())
    return;

  // Regenerate the list of all bookmarks.
  NodeVector allItems;
  ui::TreeNodeIterator<const BookmarkNode> iterator(
      self.bookmarkModel->root_node());

  while (iterator.has_next()) {
    const BookmarkNode* bookmark = iterator.Next();

    if (bookmark->is_url())
      allItems.push_back(bookmark);
  }

  // Perform segregation.
  bookmark_utils_ios::segregateNodes(allItems, _nodesSectionVector);

  [self cancelAllFaviconLoads];
  [self.collectionView reloadData];
}

- (void)collectionViewNeedsUpdate {
  switch (_updateScheduled) {
    case kNoUpdate:
      // If the collection view was not updated recently, update it now.
      [self updateCollectionView];
      _updateScheduled = kOneUpdateDone;
      // And reset the state when going back to the main loop.
      dispatch_async(dispatch_get_main_queue(), ^{
        _updateScheduled = kNoUpdate;
      });
      break;
    case kOneUpdateDone:
      // An update was already done on this turn of the main loop, schedule the
      // next update for later.
      _updateScheduled = kUpdateScheduled;
      dispatch_async(dispatch_get_main_queue(), ^{
        if (_updateScheduled == kNoUpdate)
          [self updateCollectionView];
      });
      break;
    case kUpdateScheduled:
      // Nothing to do.
      break;
  }
}

#pragma mark - BookmarkModelBridgeObserver Callbacks

- (void)bookmarkModelLoaded {
  [self updateCollectionView];
}

- (void)bookmarkNodeChanged:(const BookmarkNode*)bookmarkNode {
  if (bookmarkNode->is_folder())
    return;

  // TODO(crbug.com/603661): Ideally, we would only reload the relevant index
  // path. However, calling reloadItemsAtIndexPaths:(0,0) immediately after
  // reloadData results in a exception: NSInternalInconsistencyException
  // 'request for index path for global index 2147483645 ...'
  // One solution would be to keep track of whether we've just called
  // reloadData, but that requires experimentation to determine how long we have
  // to wait before we can safely call reloadItemsAtIndexPaths.
  [self updateCollectionView];
}

- (void)bookmarkNodeFaviconChanged:
    (const bookmarks::BookmarkNode*)bookmarkNode {
  // Only urls have favicons.
  DCHECK(bookmarkNode->is_url());

  // Update image of corresponding cell.
  NSIndexPath* indexPath = [self indexPathForNode:bookmarkNode];
  if (!indexPath)
    return;

  // Check that this cell is visible.
  NSArray* visiblePaths = [self.collectionView indexPathsForVisibleItems];
  if (![visiblePaths containsObject:indexPath])
    return;

  [self loadFaviconAtIndexPath:indexPath];
}

- (void)bookmarkNodeChildrenChanged:(const BookmarkNode*)bookmarkNode {
  [self collectionViewNeedsUpdate];
}

- (void)bookmarkNode:(const BookmarkNode*)bookmarkNode
     movedFromParent:(const BookmarkNode*)oldParent
            toParent:(const BookmarkNode*)newParent {
  [self updateCollectionView];
}

- (void)bookmarkNodeDeleted:(const BookmarkNode*)node
                 fromFolder:(const BookmarkNode*)folder {
  // Only remove the node from the list of all nodes. Since we also receive a
  // 'bookmarkNodeChildrenChanged' callback, the collection view will be updated
  // there.
  for (const auto& nodesSection : _nodesSectionVector) {
    NodeVector nodeVector = nodesSection->vector;
    // If the node was in _nodesSectionVector, it is now invalid. In that case,
    // remove it from _nodesSectionVector.
    auto it = std::find(nodeVector.begin(), nodeVector.end(), node);
    if (it != nodeVector.end()) {
      nodeVector.erase(it);
      nodesSection->vector = nodeVector;
      break;
    }
  }
}

- (void)bookmarkModelRemovedAllNodes {
  [self updateCollectionView];
}

#pragma mark - Parent class overrides that affect functionality

- (void)collectionView:(UICollectionView*)collectionView
       willDisplayCell:(UICollectionViewCell*)cell
    forItemAtIndexPath:(NSIndexPath*)indexPath {
  auto node = [self nodeAtIndexPath:indexPath];
  if (node && node->type() == bookmarks::BookmarkNode::URL) {
    [self loadFaviconAtIndexPath:indexPath];
  }
}

- (void)didAddCellForEditingAtIndexPath:(NSIndexPath*)indexPath {
  DCHECK(![self isPromoSection:indexPath.section]);
  const BookmarkNode* node = [self nodeAtIndexPath:indexPath];
  UICollectionViewCell* cell =
      [self.collectionView cellForItemAtIndexPath:indexPath];
  [self.delegate bookmarkCollectionView:self cell:cell addNodeForEditing:node];
}

- (void)didRemoveCellForEditingAtIndexPath:(NSIndexPath*)indexPath {
  DCHECK(![self isPromoSection:indexPath.section]);
  const BookmarkNode* node = [self nodeAtIndexPath:indexPath];
  UICollectionViewCell* cell =
      [self.collectionView cellForItemAtIndexPath:indexPath];
  [self.delegate bookmarkCollectionView:self
                                   cell:cell
                   removeNodeForEditing:node];
}

- (BOOL)shouldSelectCellAtIndexPath:(NSIndexPath*)indexPath {
  return ![self isPromoSection:indexPath.section];
}

- (void)didTapCellAtIndexPath:(NSIndexPath*)indexPath {
  DCHECK(![self isPromoSection:indexPath.section]);
  const BookmarkNode* node = [self nodeAtIndexPath:indexPath];
  DCHECK(node);
  RecordBookmarkLaunch(BOOKMARK_LAUNCH_LOCATION_ALL_ITEMS);
  [self.delegate bookmarkCollectionView:self
               selectedUrlForNavigation:node->url()];
}

- (void)didTapMenuButtonAtIndexPath:(NSIndexPath*)indexPath
                             onView:(UIView*)view
                            forCell:(BookmarkItemCell*)cell {
  DCHECK(![self isPromoSection:indexPath.section]);
  [self.delegate bookmarkCollectionView:self
                   wantsMenuForBookmark:[self nodeAtIndexPath:indexPath]
                                 onView:view
                                forCell:cell];
}

- (bookmark_cell::ButtonType)buttonTypeForCellAtIndexPath:
    (NSIndexPath*)indexPath {
  DCHECK(![self isPromoSection:indexPath.section]);
  if (self.editing)
    return bookmark_cell::ButtonNone;
  return bookmark_cell::ButtonMenu;
}

- (BOOL)allowLongPressForCellAtIndexPath:(NSIndexPath*)indexPath {
  return [self isPromoSection:indexPath.section] ? NO : !self.editing;
}

- (void)didLongPressCell:(UICollectionViewCell*)cell
             atIndexPath:(NSIndexPath*)indexPath {
  DCHECK(![self isPromoSection:indexPath.section]);
  [self.delegate bookmarkCollectionView:self
                       didLongPressCell:cell
                            forBookmark:[self nodeAtIndexPath:indexPath]];
}

- (BOOL)cellIsSelectedForEditingAtIndexPath:(NSIndexPath*)indexPath {
  DCHECK(![self isPromoSection:indexPath.section]);

  const BookmarkNode* node = [self nodeAtIndexPath:indexPath];
  const std::set<const BookmarkNode*>& editingNodes =
      [self.delegate nodesBeingEdited];
  return editingNodes.find(node) != editingNodes.end();
}

- (const BookmarkNode*)nodeAtIndexPath:(NSIndexPath*)indexPath {
  NSInteger section = indexPath.section;
  if ([self isPromoSection:section])
    return nullptr;

  if ([self shouldShowPromoCell])
    --section;
  return _nodesSectionVector[section]->vector[indexPath.row];
}

- (NSIndexPath*)indexPathForNode:(const BookmarkNode*)bookmarkNode {
  NSInteger section = 0;

  // When showing promo cell, bookmarks start with section 1.
  if ([self shouldShowPromoCell])
    section = 1;

  for (const auto& nodesSection : _nodesSectionVector) {
    NodeVector nodeVector = nodesSection->vector;
    NSInteger item = 0;
    for (const BookmarkNode* node : nodeVector) {
      if (bookmarkNode == node) {
        return [NSIndexPath indexPathForItem:item inSection:section];
      }
      ++item;
    }
    ++section;
  }

  return nil;
}

#pragma mark - Parent class overrides that change UI

// Parent class override.
- (UIEdgeInsets)insetForSectionAtIndex:(NSInteger)section {
  // Only return insets for non-empty sections.
  NSInteger count = [self numberOfItemsInSection:section];
  if (count == 0)
    return UIEdgeInsetsZero;

  // The last section needs special treatment.
  UIEdgeInsets insets = [super insetForSectionAtIndex:section];
  NSInteger sectionCount = [self.collectionView.dataSource
      numberOfSectionsInCollectionView:self.collectionView];

  if (section == sectionCount - 1) {
    insets.top = 0;
    return insets;
  }

  insets.top = 0;
  insets.bottom = 0;
  return insets;
}

// Parent class override.
- (UICollectionViewCell*)cellAtIndexPath:(NSIndexPath*)indexPath {
  if ([self isPromoSection:indexPath.section]) {
    self.promoCell = [self.collectionView
        dequeueReusableCellWithReuseIdentifier:[BookmarkPromoCell
                                                   reuseIdentifier]
                                  forIndexPath:indexPath];
    self.promoCell.delegate = self;
    return self.promoCell;
  }

  return [self cellForBookmark:[self nodeAtIndexPath:indexPath]
                     indexPath:indexPath];
}

// Parent class override.
- (CGSize)headerSizeForSection:(NSInteger)section {
  if ([self isPromoSection:section])
    return CGSizeZero;

  return CGSizeMake(self.bounds.size.width, [BookmarkHeaderView handsetHeight]);
}

- (UICollectionReusableView*)headerAtIndexPath:(NSIndexPath*)indexPath {
  NSInteger section = indexPath.section;
  if ([self isPromoSection:section])
    return nil;

  if ([self shouldShowPromoCell])
    --section;

  BookmarkHeaderView* view = [self.collectionView
      dequeueReusableSupplementaryViewOfKind:
          UICollectionElementKindSectionHeader
                         withReuseIdentifier:[BookmarkHeaderView
                                                 reuseIdentifier]
                                forIndexPath:indexPath];

  NSString* title =
      base::SysUTF8ToNSString(_nodesSectionVector[section]->timeRepresentation);
  [view setTitle:title];
  return view;
}

- (NSInteger)numberOfItemsInSection:(NSInteger)section {
  if ([self isPromoSection:section])
    return 1;

  if ([self shouldShowPromoCell])
    --section;
  return _nodesSectionVector[section]->vector.size();
}

- (NSInteger)numberOfSections {
  const BOOL showPromo = [self shouldShowPromoCell];
  const NSInteger nodeSectionsCount = _nodesSectionVector.size();
  return showPromo ? nodeSectionsCount + 1 : nodeSectionsCount;
}

#pragma mark - BookmarkPromoCellDelegate

- (void)bookmarkPromoCellDidTapSignIn:(BookmarkPromoCell*)bookmarkPromoCell {
  [self.delegate bookmarkCollectionViewShowSignIn:self];
}

- (void)bookmarkPromoCellDidTapDismiss:(BookmarkPromoCell*)bookmarkPromoCell {
  [self.delegate bookmarkCollectionViewDismissPromo:self];
}

#pragma mark - Promo Cell

- (BOOL)isPromoActive {
  return [self.delegate bookmarkCollectionViewShouldShowPromoCell:self];
}

- (BOOL)shouldShowPromoCell {
  // The promo cell is not shown in edit mode.
  return !self.editing && [self isPromoActive];
}

@end
