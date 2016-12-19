// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bookmarks/bookmark_folder_view_controller.h"

#include <memory>
#include <vector>

#import "base/ios/weak_nsobject.h"
#include "base/logging.h"
#include "base/mac/objc_property_releaser.h"
#include "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#include "components/bookmarks/browser/bookmark_model.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_folder_editor_view_controller.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_folder_table_view_cell.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_model_bridge_observer.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_navigation_controller.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_utils_ios.h"
#import "ios/chrome/browser/ui/icons/chrome_icon.h"
#import "ios/chrome/browser/ui/material_components/utils.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/third_party/material_components_ios/src/components/AppBar/src/MaterialAppBar.h"
#include "ui/base/l10n/l10n_util_mac.h"

using bookmarks::BookmarkNode;

namespace {

// The height of every folder cell.
const CGFloat kFolderCellHeight = 48.0;

// Height of section headers/footers.
const CGFloat kSectionHeaderHeight = 8.0;
const CGFloat kSectionFooterHeight = 8.0;

// Enum for the available sections.
// First section displays a cell to create a new folder.
// The second section displays as many folders as are available.
typedef enum {
  BookmarkFolderSectionDefault = 0,
  BookmarkFolderSectionFolders,
} BookmarkFolderSection;
const NSInteger BookmarkFolderSectionCount = 2;

}  // namespace

@interface BookmarkFolderViewController ()<
    BookmarkFolderEditorViewControllerDelegate,
    BookmarkModelBridgeObserver,
    UITableViewDataSource,
    UITableViewDelegate> {
  std::set<const BookmarkNode*> _editedNodes;
  std::vector<const BookmarkNode*> _folders;
  std::unique_ptr<bookmarks::BookmarkModelBridge> _modelBridge;
  base::scoped_nsobject<MDCAppBar> _appBar;
  base::mac::ObjCPropertyReleaser
      _propertyReleaser_BookmarkFolderViewController;
}

// Should the controller setup Cancel and Done buttons instead of a back button.
@property(nonatomic, assign) BOOL allowsCancel;

// Should the controller setup a new-folder button.
@property(nonatomic, assign) BOOL allowsNewFolders;

// Reference to the main bookmark model.
@property(nonatomic, assign) bookmarks::BookmarkModel* bookmarkModel;

// The currently selected folder.
@property(nonatomic, readonly) const BookmarkNode* selectedFolder;

// The view controller to present when creating a new folder.
@property(nonatomic, retain)
    BookmarkFolderEditorViewController* folderAddController;

// A linear list of folders.
@property(nonatomic, assign, readonly)
    const std::vector<const BookmarkNode*>& folders;

// The table view that displays the options and folders.
@property(nonatomic, retain) UITableView* tableView;

// Returns the cell for the default section and the given |row|.
- (BookmarkFolderTableViewCell*)defaultSectionCellForRow:(NSInteger)row;

// Returns a folder cell for the folder at |row| in |self.folders|.
- (BookmarkFolderTableViewCell*)folderSectionCellForRow:(NSInteger)row;

// Reloads the folder list.
- (void)reloadFolders;

// Pushes on the navigation controller a view controller to create a new folder.
- (void)pushFolderAddViewController;

// Called when the user taps on a folder row. The cell is checked, the UI is
// locked so that the user can't interact with it, then the delegate is
// notified. Usual implementations of this delegate callback are to pop or
// dismiss this controller on selection. The delay is here to let the user get a
// visual feedback of the selection before this view disappears.
- (void)delayedNotifyDelegateOfSelection;

@end

@implementation BookmarkFolderViewController

@synthesize allowsCancel = _allowsCancel;
@synthesize allowsNewFolders = _allowsNewFolders;
@synthesize bookmarkModel = _bookmarkModel;
@synthesize editedNodes = _editedNodes;
@synthesize folderAddController = _folderAddController;
@synthesize delegate = _delegate;
@synthesize folders = _folders;
@synthesize tableView = _tableView;
@synthesize selectedFolder = _selectedFolder;

- (instancetype)initWithBookmarkModel:(bookmarks::BookmarkModel*)bookmarkModel
                     allowsNewFolders:(BOOL)allowsNewFolders
                          editedNodes:
                              (const std::set<const BookmarkNode*>&)nodes
                         allowsCancel:(BOOL)allowsCancel
                       selectedFolder:(const BookmarkNode*)selectedFolder {
  DCHECK(bookmarkModel);
  DCHECK(bookmarkModel->loaded());
  DCHECK(selectedFolder == NULL || selectedFolder->is_folder());
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _propertyReleaser_BookmarkFolderViewController.Init(
        self, [BookmarkFolderViewController class]);
    _allowsCancel = allowsCancel;
    _allowsNewFolders = allowsNewFolders;
    _bookmarkModel = bookmarkModel;
    _editedNodes = nodes;
    _selectedFolder = selectedFolder;

    // Set up the bookmark model oberver.
    _modelBridge.reset(
        new bookmarks::BookmarkModelBridge(self, _bookmarkModel));

    _appBar.reset([[MDCAppBar alloc] init]);
    [self addChildViewController:[_appBar headerViewController]];
  }
  return self;
}

- (void)changeSelectedFolder:(const BookmarkNode*)selectedFolder {
  DCHECK(selectedFolder);
  DCHECK(selectedFolder->is_folder());
  _selectedFolder = selectedFolder;
  [self.tableView reloadData];
}

- (void)dealloc {
  _tableView.dataSource = nil;
  _tableView.delegate = nil;
  _folderAddController.delegate = nil;
  [super dealloc];
}

- (UIStatusBarStyle)preferredStatusBarStyle {
  return UIStatusBarStyleDefault;
}

#pragma mark - View lifecycle

- (void)viewDidLoad {
  [super viewDidLoad];
  if ([self respondsToSelector:@selector(setEdgesForExtendedLayout:)]) {
    [self setEdgesForExtendedLayout:UIRectEdgeNone];
  }
  self.view.backgroundColor = [UIColor whiteColor];
  self.view.accessibilityIdentifier = @"Folder Picker";

  self.title = l10n_util::GetNSString(IDS_IOS_BOOKMARK_CHOOSE_GROUP_BUTTON);

  base::scoped_nsobject<UIBarButtonItem> doneItem([[UIBarButtonItem alloc]
      initWithTitle:l10n_util::GetNSString(
                        IDS_IOS_BOOKMARK_EDIT_MODE_EXIT_MOBILE)
              style:UIBarButtonItemStylePlain
             target:self
             action:@selector(done:)]);
  doneItem.get().accessibilityIdentifier = @"Done";
  self.navigationItem.rightBarButtonItem = doneItem;

  if (self.allowsCancel) {
    UIBarButtonItem* cancelItem =
        [ChromeIcon templateBarButtonItemWithImage:[ChromeIcon closeIcon]
                                            target:self
                                            action:@selector(cancel:)];
    cancelItem.accessibilityLabel =
        l10n_util::GetNSString(IDS_IOS_BOOKMARK_NEW_CANCEL_BUTTON_LABEL);
    cancelItem.accessibilityIdentifier = @"Cancel";
    self.navigationItem.leftBarButtonItem = cancelItem;
  } else {
    UIBarButtonItem* backItem =
        [ChromeIcon templateBarButtonItemWithImage:[ChromeIcon backIcon]
                                            target:self
                                            action:@selector(back:)];
    backItem.accessibilityLabel =
        l10n_util::GetNSString(IDS_IOS_BOOKMARK_NEW_BACK_LABEL);
    backItem.accessibilityIdentifier = @"Back";
    self.navigationItem.leftBarButtonItem = backItem;
  }

  // The table view.
  base::scoped_nsobject<UITableView> tableView([[UITableView alloc]
      initWithFrame:self.view.bounds
              style:UITableViewStylePlain]);
  tableView.get().dataSource = self;
  tableView.get().delegate = self;
  tableView.get().autoresizingMask =
      UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
  tableView.get().separatorStyle = UITableViewCellSeparatorStyleNone;
  [self.view addSubview:tableView];
  [self.view sendSubviewToBack:tableView];
  self.tableView = tableView;

  // Add the app bar to the view hierarchy.  This must be done last, so that the
  // app bar's views are the frontmost.
  ConfigureAppBarWithCardStyle(_appBar);
  [_appBar headerViewController].headerView.trackingScrollView = self.tableView;
  [_appBar addSubviewsToParent];
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];
  [self reloadFolders];
}

- (UIViewController*)childViewControllerForStatusBarHidden {
  return [_appBar headerViewController];
}

- (UIViewController*)childViewControllerForStatusBarStyle {
  return [_appBar headerViewController];
}

#pragma mark - Accessibility

- (BOOL)accessibilityPerformEscape {
  [self.delegate folderPickerDidCancel:self];
  return YES;
}

#pragma mark - UIScrollViewDelegate

- (void)scrollViewDidScroll:(UIScrollView*)scrollView {
  MDCFlexibleHeaderView* headerView = [_appBar headerViewController].headerView;
  if (scrollView == headerView.trackingScrollView) {
    [headerView trackingScrollViewDidScroll];
  }
}

- (void)scrollViewDidEndDecelerating:(UIScrollView*)scrollView {
  MDCFlexibleHeaderView* headerView = [_appBar headerViewController].headerView;
  if (scrollView == headerView.trackingScrollView) {
    [headerView trackingScrollViewDidEndDecelerating];
  }
}

- (void)scrollViewDidEndDragging:(UIScrollView*)scrollView
                  willDecelerate:(BOOL)decelerate {
  MDCFlexibleHeaderView* headerView = [_appBar headerViewController].headerView;
  if (scrollView == headerView.trackingScrollView) {
    [headerView trackingScrollViewDidEndDraggingWillDecelerate:decelerate];
  }
}

- (void)scrollViewWillEndDragging:(UIScrollView*)scrollView
                     withVelocity:(CGPoint)velocity
              targetContentOffset:(inout CGPoint*)targetContentOffset {
  MDCFlexibleHeaderView* headerView = [_appBar headerViewController].headerView;
  if (scrollView == headerView.trackingScrollView) {
    [headerView
        trackingScrollViewWillEndDraggingWithVelocity:velocity
                                  targetContentOffset:targetContentOffset];
  }
}

#pragma mark - UITableViewDataSource

- (NSInteger)numberOfSectionsInTableView:(UITableView*)tableView {
  return BookmarkFolderSectionCount;
}

- (NSInteger)tableView:(UITableView*)tableView
    numberOfRowsInSection:(NSInteger)section {
  switch (static_cast<BookmarkFolderSection>(section)) {
    case BookmarkFolderSectionDefault:
      return [self shouldShowDefaultSection] ? 1 : 0;

    case BookmarkFolderSectionFolders:
      return self.folders.size();
  }
  NOTREACHED();
  return 0;
}

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  BookmarkFolderTableViewCell* cell = nil;
  switch (static_cast<BookmarkFolderSection>(indexPath.section)) {
    case BookmarkFolderSectionDefault:
      cell = [self defaultSectionCellForRow:indexPath.row];
      break;

    case BookmarkFolderSectionFolders:
      cell = [self folderSectionCellForRow:indexPath.row];
      break;
  }
  return cell;
}

#pragma mark - UITableViewDelegate

- (CGFloat)tableView:(UITableView*)tableView
    heightForRowAtIndexPath:(NSIndexPath*)indexPath {
  return kFolderCellHeight;
}

- (CGFloat)tableView:(UITableView*)tableView
    heightForHeaderInSection:(NSInteger)section {
  switch (static_cast<BookmarkFolderSection>(section)) {
    case BookmarkFolderSectionDefault:
      return [self shouldShowDefaultSection] ? kSectionHeaderHeight : 0;

    case BookmarkFolderSectionFolders:
      return kSectionHeaderHeight;
  }
  NOTREACHED();
  return 0;
}

- (UIView*)tableView:(UITableView*)tableView
    viewForHeaderInSection:(NSInteger)section {
  CGRect headerViewFrame =
      CGRectMake(0, 0, CGRectGetWidth(tableView.frame),
                 [self tableView:tableView heightForHeaderInSection:section]);
  UIView* headerView =
      [[[UIView alloc] initWithFrame:headerViewFrame] autorelease];
  if (section == BookmarkFolderSectionFolders &&
      [self shouldShowDefaultSection]) {
    CGRect separatorFrame =
        CGRectMake(0, 0, CGRectGetWidth(headerView.bounds),
                   1.0 / [[UIScreen mainScreen] scale]);  // 1-pixel divider.
    base::scoped_nsobject<UIView> separator(
        [[UIView alloc] initWithFrame:separatorFrame]);
    separator.get().autoresizingMask = UIViewAutoresizingFlexibleBottomMargin |
                                       UIViewAutoresizingFlexibleWidth;
    separator.get().backgroundColor = bookmark_utils_ios::separatorColor();
    [headerView addSubview:separator];
  }
  return headerView;
}

- (CGFloat)tableView:(UITableView*)tableView
    heightForFooterInSection:(NSInteger)section {
  switch (static_cast<BookmarkFolderSection>(section)) {
    case BookmarkFolderSectionDefault:
      return [self shouldShowDefaultSection] ? kSectionFooterHeight : 0;

    case BookmarkFolderSectionFolders:
      return kSectionFooterHeight;
  }
  NOTREACHED();
  return 0;
}

- (UIView*)tableView:(UITableView*)tableView
    viewForFooterInSection:(NSInteger)section {
  return [[[UIView alloc] init] autorelease];
}

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  [tableView deselectRowAtIndexPath:indexPath animated:YES];
  switch (static_cast<BookmarkFolderSection>(indexPath.section)) {
    case BookmarkFolderSectionDefault:
      [self pushFolderAddViewController];
      break;

    case BookmarkFolderSectionFolders: {
      const BookmarkNode* folder = self.folders[indexPath.row];
      [self changeSelectedFolder:folder];
      [self delayedNotifyDelegateOfSelection];
      break;
    }
  }
}

#pragma mark - BookmarkFolderEditorViewControllerDelegate

- (void)bookmarkFolderEditor:(BookmarkFolderEditorViewController*)folderEditor
      didFinishEditingFolder:(const BookmarkNode*)folder {
  DCHECK(folder);
  [self reloadFolders];
  [self changeSelectedFolder:folder];
  [self delayedNotifyDelegateOfSelection];
}

- (void)bookmarkFolderEditorDidDeleteEditedFolder:
    (BookmarkFolderEditorViewController*)folderEditor {
  NOTREACHED();
}

- (void)bookmarkFolderEditorDidCancel:
    (BookmarkFolderEditorViewController*)folderEditor {
  [self.navigationController popViewControllerAnimated:YES];
  self.folderAddController.delegate = nil;
  self.folderAddController = nil;
}

#pragma mark - BookmarkModelBridgeObserver

- (void)bookmarkModelLoaded {
  // The bookmark model is assumed to be loaded when this controller is created.
  NOTREACHED();
}

- (void)bookmarkNodeChanged:(const BookmarkNode*)bookmarkNode {
  if (!bookmarkNode->is_folder())
    return;
  [self reloadFolders];
}

- (void)bookmarkNodeChildrenChanged:(const BookmarkNode*)bookmarkNode {
  [self reloadFolders];
}

- (void)bookmarkNode:(const BookmarkNode*)bookmarkNode
     movedFromParent:(const BookmarkNode*)oldParent
            toParent:(const BookmarkNode*)newParent {
  if (bookmarkNode->is_folder()) {
    [self reloadFolders];
  }
}

- (void)bookmarkNodeDeleted:(const BookmarkNode*)bookmarkNode
                 fromFolder:(const BookmarkNode*)folder {
  if (!bookmarkNode->is_folder())
    return;

  if (bookmarkNode == self.selectedFolder) {
    // The selected folder has been deleted. Fallback on the Mobile Bookmarks
    // node.
    [self changeSelectedFolder:self.bookmarkModel->mobile_node()];
  }
  [self reloadFolders];
}

- (void)bookmarkModelRemovedAllNodes {
  // The selected folder is no longer valid. Fallback on the Mobile Bookmarks
  // node.
  [self changeSelectedFolder:self.bookmarkModel->mobile_node()];
  [self reloadFolders];
}

#pragma mark - Actions

- (void)done:(id)sender {
  [self.delegate folderPicker:self didFinishWithFolder:self.selectedFolder];
}

- (void)cancel:(id)sender {
  [self.delegate folderPickerDidCancel:self];
}

- (void)back:(id)sender {
  [self.delegate folderPickerDidCancel:self];
}

#pragma mark - Private

- (BOOL)shouldShowDefaultSection {
  return self.allowsNewFolders;
}

- (BookmarkFolderTableViewCell*)defaultSectionCellForRow:(NSInteger)row {
  DCHECK([self shouldShowDefaultSection]);
  DCHECK_EQ(0, row);
  BookmarkFolderTableViewCell* cell = [self.tableView
      dequeueReusableCellWithIdentifier:[BookmarkFolderTableViewCell
                                            folderCreationCellReuseIdentifier]];
  if (!cell) {
    cell = [BookmarkFolderTableViewCell folderCreationCell];
  }
  return cell;
}

- (BookmarkFolderTableViewCell*)folderSectionCellForRow:(NSInteger)row {
  DCHECK(row <
         [self.tableView numberOfRowsInSection:BookmarkFolderSectionFolders]);
  BookmarkFolderTableViewCell* cell = [self.tableView
      dequeueReusableCellWithIdentifier:[BookmarkFolderTableViewCell
                                            folderCellReuseIdentifier]];
  if (!cell) {
    cell = [BookmarkFolderTableViewCell folderCell];
  }
  const BookmarkNode* folder = self.folders[row];
  NSString* title = bookmark_utils_ios::TitleForBookmarkNode(folder);
  cell.textLabel.text = title;
  cell.accessibilityIdentifier = title;
  cell.accessibilityLabel = title;
  cell.checked = (self.selectedFolder == folder);

  // Indentation level.
  NSInteger level = 0;
  const BookmarkNode* node = folder;
  while (node && !(self.bookmarkModel->is_root_node(node))) {
    ++level;
    node = node->parent();
  }
  // The root node is not shown as a folder, so top level folders have a
  // level strictly positive.
  DCHECK(level > 0);
  cell.indentationLevel = level - 1;

  return cell;
}

- (void)reloadFolders {
  _folders = bookmark_utils_ios::VisibleNonDescendantNodes(self.editedNodes,
                                                           self.bookmarkModel);
  [self.tableView reloadData];
}

- (void)pushFolderAddViewController {
  DCHECK(self.allowsNewFolders);
  BookmarkFolderEditorViewController* folderCreator =
      [BookmarkFolderEditorViewController
          folderCreatorWithBookmarkModel:self.bookmarkModel
                            parentFolder:self.selectedFolder];
  folderCreator.delegate = self;
  [self.navigationController pushViewController:folderCreator animated:YES];
  self.folderAddController = folderCreator;
}

- (void)delayedNotifyDelegateOfSelection {
  self.view.userInteractionEnabled = NO;
  base::WeakNSObject<BookmarkFolderViewController> weakSelf(self);
  dispatch_after(
      dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.3 * NSEC_PER_SEC)),
      dispatch_get_main_queue(), ^{
        base::scoped_nsobject<BookmarkFolderViewController> strongSelf(
            [weakSelf retain]);
        // Early return if the controller has been deallocated.
        if (!strongSelf)
          return;
        strongSelf.get().view.userInteractionEnabled = YES;
        [strongSelf done:nil];
      });
}

@end
