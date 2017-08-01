// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bookmarks/bookmark_menu_view.h"

#include <memory>

#include "base/mac/foundation_util.h"

#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_model_observer.h"
#include "ios/chrome/browser/bookmarks/bookmark_model_factory.h"
#include "ios/chrome/browser/bookmarks/bookmarks_utils.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/experimental_flags.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_menu_cell.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_menu_item.h"
#include "ios/chrome/browser/ui/bookmarks/bookmark_model_bridge_observer.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_utils_ios.h"
#include "ios/chrome/browser/ui/ui_util.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/third_party/material_components_ios/src/components/Ink/src/MaterialInk.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/tree_node_iterator.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using bookmarks::BookmarkNode;

@interface BookmarkMenuView ()<BookmarkModelBridgeObserver,
                               MDCInkTouchControllerDelegate,
                               UITableViewDataSource,
                               UITableViewDelegate> {
  // A bridge to receive bookmark model observer callbacks.
  std::unique_ptr<bookmarks::BookmarkModelBridge> _modelBridge;
}
@property(nonatomic, assign) bookmarks::BookmarkModel* bookmarkModel;
// This array directly represents the rows that show up in the table.
@property(nonatomic, strong) NSMutableArray* menuItems;
// The primary menu item is blue instead of gray.
@property(nonatomic, strong) BookmarkMenuItem* primaryMenuItem;
@property(nonatomic, assign) ios::ChromeBrowserState* browserState;
@property(nonatomic, strong) UITableView* tableView;
@property(nonatomic, strong) MDCInkTouchController* inkTouchController;

// Updates the data model, and the UI.
- (void)reloadData;

// Creates the views for this class.
- (void)createViews;

@end

@implementation BookmarkMenuView
@synthesize bookmarkModel = _bookmarkModel;
@synthesize delegate = _delegate;
@synthesize menuItems = _menuItems;
@synthesize primaryMenuItem = _primaryMenuItem;
@synthesize browserState = _browserState;
@synthesize tableView = _tableView;
@synthesize inkTouchController = _inkTouchController;

- (id)initWithFrame:(CGRect)frame {
  NOTREACHED();
  return nil;
}

- (instancetype)initWithBrowserState:(ios::ChromeBrowserState*)browserState
                               frame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    _browserState = browserState;

    // Set up connection to the BookmarkModel.
    _bookmarkModel =
        ios::BookmarkModelFactory::GetForBrowserState(_browserState);
    // Set up observers.
    _modelBridge.reset(
        new bookmarks::BookmarkModelBridge(self, _bookmarkModel));

    self.menuItems = [NSMutableArray array];

    [self createViews];
  }
  return self;
}

- (void)dealloc {
  self.tableView.delegate = nil;
  self.tableView.dataSource = nil;
}

- (void)createViews {
  // Make the table view.
  self.tableView = [[UITableView alloc] initWithFrame:self.bounds];
  [self addSubview:self.tableView];
  self.tableView.separatorStyle = UITableViewCellSeparatorStyleNone;
  self.tableView.delegate = self;
  self.tableView.dataSource = self;
  self.tableView.scrollsToTop = NO;
  [self reloadData];

  // Set up ink touch controller.
  MDCInkTouchController* inkTouchController =
      [[MDCInkTouchController alloc] initWithView:self.tableView];
  self.inkTouchController = inkTouchController;
  self.inkTouchController.delegate = self;
  self.inkTouchController.delaysInkSpread = YES;
}

- (void)layoutSubviews {
  [super layoutSubviews];
  self.tableView.frame = self.bounds;
}

- (void)reloadData {
  if (!self.bookmarkModel->loaded())
    return;

  BookmarkMenuItem* primaryItem = [self.primaryMenuItem parentItem];

  [self.menuItems removeAllObjects];

  const BookmarkNode* mobileBookmarks = self.bookmarkModel->mobile_node();
  const BookmarkNode* bookmarkBar = self.bookmarkModel->bookmark_bar_node();
  const BookmarkNode* otherBookmarks = self.bookmarkModel->other_node();

  // The first section is always visible.
  NSMutableArray* topSection = [[NSMutableArray alloc] init];
  [self.menuItems addObject:topSection];

  // Mobile bookmark is shown even if empty.
  [topSection
      addObject:[BookmarkMenuItem folderMenuItemForNode:mobileBookmarks
                                           rootAncestor:mobileBookmarks]];
  // Bookmarks Bar and Other Bookmarks are special folders and are shown at the
  // top if they contain anything.
  if (!bookmarkBar->empty()) {
    [topSection addObject:[BookmarkMenuItem folderMenuItemForNode:bookmarkBar
                                                     rootAncestor:bookmarkBar]];
  }
  if (!otherBookmarks->empty()) {
    [topSection
        addObject:[BookmarkMenuItem folderMenuItemForNode:otherBookmarks
                                             rootAncestor:otherBookmarks]];
  }

  // The second section contains all the top level folders (except for the
  // permanent nodes).
  NSMutableArray* folderSection = [[NSMutableArray alloc] init];
  std::vector<const BookmarkNode*> rootLevelFolders =
      RootLevelFolders(self.bookmarkModel);
  bookmark_utils_ios::SortFolders(&rootLevelFolders);
  for (auto* node : rootLevelFolders) {
    [folderSection addObject:[BookmarkMenuItem folderMenuItemForNode:node
                                                        rootAncestor:node]];
  }
  if ([folderSection count]) {
    // Add the title and the divider at the top of the section.
    [folderSection
        insertObject:[BookmarkMenuItem sectionMenuItemWithTitle:
                                           l10n_util::GetNSString(
                                               IDS_IOS_BOOKMARK_FOLDERS_LABEL)]
             atIndex:0];
    [folderSection insertObject:[BookmarkMenuItem dividerMenuItem] atIndex:0];
    [self.menuItems addObject:folderSection];
  }

  // If the currently selected menuitem is no longer present in the menu, then
  // select the first item in the top section instead.
  if (![topSection containsObject:primaryItem] &&
      ![folderSection containsObject:primaryItem]) {
    self.primaryMenuItem = [topSection firstObject];
    [self.delegate bookmarkMenuView:self selectedMenuItem:self.primaryMenuItem];
  }

  [self.tableView reloadData];
}

- (BookmarkMenuItem*)defaultMenuItem {
  // The first item in the first section.
  DCHECK([[self.menuItems firstObject] firstObject]);
  return [[self.menuItems firstObject] firstObject];
}

- (BookmarkMenuItem*)menuItemAtIndexPath:(NSIndexPath*)indexPath {
  return self.menuItems[indexPath.section][indexPath.row];
}

#pragma mark UIView method

- (void)didMoveToSuperview {
  [super didMoveToSuperview];
  // The background color depends on where in the view hierarchy the menu is.
  // For example, the menu may be moved to a slide over panel if the
  // horizontal size class changes from regular to compact.
  self.tableView.backgroundColor = bookmark_utils_ios::menuBackgroundColor();
}

#pragma mark BookmarkModelBridgeObserver

- (void)bookmarkModelLoaded {
  [self reloadData];
}

- (void)bookmarkNodeChanged:(const BookmarkNode*)bookmarkNode {
  [self reloadData];
}

- (void)bookmarkNodeChildrenChanged:(const BookmarkNode*)bookmarkNode {
  [self reloadData];
}

- (void)bookmarkNode:(const BookmarkNode*)bookmarkNode
     movedFromParent:(const BookmarkNode*)oldParent
            toParent:(const BookmarkNode*)newParent {
  if (self.primaryMenuItem.type == bookmarks::MenuItemFolder &&
      bookmarkNode->is_folder()) {
    // Checking which folder moved and if the current folder was implicated is
    // complicated and not worth the effort. Just rebuild a new primaryMenu item
    // unconditionally, this is simpler.
    const BookmarkNode* currentFolder = self.primaryMenuItem.folder;
    BookmarkMenuItem* menuItem = [BookmarkMenuItem
        folderMenuItemForNode:currentFolder
                 rootAncestor:RootLevelFolderForNode(currentFolder,
                                                     self.bookmarkModel)];
    if (menuItem != self.primaryMenuItem) {
      self.primaryMenuItem = menuItem;
      [self.delegate bookmarkMenuView:self
                     selectedMenuItem:self.primaryMenuItem];
    }
  }
  [self reloadData];
}

- (void)bookmarkNodeDeleted:(const BookmarkNode*)node
                 fromFolder:(const BookmarkNode*)parentFolder {
  // If the current folder or one of its ancestor has been deleted, the
  // selection needs to move up to a non deleted ancestor. This check is made
  // more complex as by the time this method is called |node| is no longer in
  // the hierarchy : its parent is already set to null.

  if (self.primaryMenuItem.type != bookmarks::MenuItemFolder) {
    // If the object currently selected is not a folder, just reload.
    [self reloadData];
    return;
  }

  if (parentFolder == self.primaryMenuItem.folder || !node->is_folder()) {
    // A child of the selected folder has been deleted or a url not visible in
    // the UI right now has been deleted. Nothing to do as the menu itself needs
    // no change.
    return;
  }

  const BookmarkNode* root =
      RootLevelFolderForNode(parentFolder, self.bookmarkModel);

  if (root != self.primaryMenuItem.rootAncestor) {
    // The deleted folder is not in the same hierarchy as the current selected
    // folder, there is nothing to reload unless the deleted folder is a root
    // node.
    if (!root)
      [self reloadData];
    return;
  }

  if (node == self.primaryMenuItem.folder) {
    // The simple case where the deleted folder is the one currently in the UI.
    // At this point the deleted folder is known to not be a root node:
    DCHECK_NE(self.primaryMenuItem.folder, self.primaryMenuItem.rootAncestor);
    // Simply move to the parent.
    self.primaryMenuItem =
        [BookmarkMenuItem folderMenuItemForNode:parentFolder rootAncestor:root];
    [self.delegate bookmarkMenuView:self selectedMenuItem:self.primaryMenuItem];
    [self reloadData];
  }

  // The only case left is when the deleted folder used to be an ancestor of the
  // selected folder. This is easy to infer, if the selected folder is no longer
  // present in the common root hierarchy, this means it was deleted as well.
  ui::TreeNodeIterator<const BookmarkNode> iterator(root);
  while (iterator.has_next()) {
    if (self.primaryMenuItem.folder == iterator.Next())
      return;  // Nothing to do.
  }
  // The current folder was not found, relocate to the first non deleted
  // ancestor.
  self.primaryMenuItem =
      [BookmarkMenuItem folderMenuItemForNode:parentFolder rootAncestor:root];
  [self.delegate bookmarkMenuView:self selectedMenuItem:self.primaryMenuItem];
  [self reloadData];
}

- (void)bookmarkModelRemovedAllNodes {
  [self reloadData];
}

#pragma mark UITableViewDataSource

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  BookmarkMenuCell* cell = [tableView
      dequeueReusableCellWithIdentifier:[BookmarkMenuCell reuseIdentifier]];
  if (!cell) {
    cell = [[BookmarkMenuCell alloc]
          initWithStyle:UITableViewCellStyleDefault
        reuseIdentifier:[BookmarkMenuCell reuseIdentifier]];
  }
  cell.selectionStyle = UITableViewCellSelectionStyleNone;
  BookmarkMenuItem* menuItem = [self menuItemAtIndexPath:indexPath];
  BOOL primary =
      [[self.primaryMenuItem parentItem] isEqual:[menuItem parentItem]];
  [cell updateWithBookmarkMenuItem:menuItem primary:primary];
  if (primary && bookmark_utils_ios::bookmarkMenuIsInSlideInPanel()) {
    [tableView selectRowAtIndexPath:indexPath
                           animated:NO
                     scrollPosition:UITableViewScrollPositionNone];
  }
  return cell;
}

- (NSInteger)tableView:(UITableView*)tableView
    numberOfRowsInSection:(NSInteger)section {
  return [self.menuItems[section] count];
}

- (NSInteger)numberOfSectionsInTableView:(UITableView*)tableView {
  return [self.menuItems count];
}

#pragma mark UITableViewDelegate

- (CGFloat)tableView:(UITableView*)tableView
    heightForRowAtIndexPath:(NSIndexPath*)indexPath {
  BookmarkMenuItem* menuItem = [self menuItemAtIndexPath:indexPath];
  return [menuItem height];
}

- (NSIndexPath*)tableView:(UITableView*)tableView
    willSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  BookmarkMenuItem* menuItem = [self menuItemAtIndexPath:indexPath];
  return [menuItem canBeSelected] ? indexPath : nil;
}

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  BookmarkMenuItem* menuItem = [self menuItemAtIndexPath:indexPath];
  [self.delegate bookmarkMenuView:self selectedMenuItem:menuItem];
}

- (CGFloat)tableView:(UITableView*)tableView
    heightForHeaderInSection:(NSInteger)section {
  return 8.0;
}

- (CGFloat)tableView:(UITableView*)tableView
    heightForFooterInSection:(NSInteger)section {
  BOOL isLastSection = [tableView numberOfSections] == (section + 1);
  return isLastSection ? 8.0 : 0.0;
}

- (UIView*)tableView:(UITableView*)tableView
    viewForHeaderInSection:(NSInteger)section {
  return [[UIView alloc] initWithFrame:CGRectZero];
}

- (UIView*)tableView:(UITableView*)tableView
    viewForFooterInSection:(NSInteger)section {
  return [[UIView alloc] initWithFrame:CGRectZero];
}

#pragma mark MDCInkTouchControllerDelegate

- (BOOL)inkTouchController:(MDCInkTouchController*)inkTouchController
    shouldProcessInkTouchesAtTouchLocation:(CGPoint)location {
  NSIndexPath* indexPath = [self.tableView indexPathForRowAtPoint:location];
  BookmarkMenuItem* menuItem = [self menuItemAtIndexPath:indexPath];
  return menuItem.type == bookmarks::MenuItemFolder;
}

- (MDCInkView*)inkTouchController:(MDCInkTouchController*)inkTouchController
           inkViewAtTouchLocation:(CGPoint)location {
  NSIndexPath* indexPath = [self.tableView indexPathForRowAtPoint:location];
  BookmarkMenuCell* cell = base::mac::ObjCCastStrict<BookmarkMenuCell>(
      [self.tableView cellForRowAtIndexPath:indexPath]);
  return cell.inkView;
}

#pragma mark Public Methods

- (void)updatePrimaryMenuItem:(BookmarkMenuItem*)menuItem {
  if ([self.primaryMenuItem isEqual:menuItem])
    return;

  self.primaryMenuItem = menuItem;
  [self.tableView reloadData];
}

- (void)setScrollsToTop:(BOOL)scrollsToTop {
  self.tableView.scrollsToTop = scrollsToTop;
}

@end
