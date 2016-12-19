// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#import "ios/chrome/browser/ui/bookmarks/bookmark_folder_editor_view_controller.h"

#include <memory>
#include <set>

#include "base/auto_reset.h"
#include "base/i18n/rtl.h"
#import "base/ios/weak_nsobject.h"
#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/mac/objc_property_releaser.h"
#include "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_elevated_toolbar.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_folder_view_controller.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_model_bridge_observer.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_utils_ios.h"
#import "ios/chrome/browser/ui/bookmarks/cells/bookmark_parent_folder_item.h"
#import "ios/chrome/browser/ui/bookmarks/cells/bookmark_text_field_item.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_item.h"
#import "ios/chrome/browser/ui/collection_view/collection_view_model.h"
#import "ios/chrome/browser/ui/icons/chrome_icon.h"
#import "ios/chrome/browser/ui/material_components/utils.h"
#include "ios/chrome/browser/ui/rtl_geometry.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/third_party/material_components_ios/src/components/NavigationBar/src/MaterialNavigationBar.h"
#import "ios/third_party/material_components_ios/src/components/Palettes/src/MaterialPalettes.h"
#import "ios/third_party/material_components_ios/src/components/ShadowElevations/src/MaterialShadowElevations.h"
#import "ios/third_party/material_components_ios/src/components/ShadowLayer/src/MaterialShadowLayer.h"
#import "ios/third_party/material_components_ios/src/components/Typography/src/MaterialTypography.h"
#include "ui/base/l10n/l10n_util.h"

using bookmarks::BookmarkNode;

namespace {

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierInfo = kSectionIdentifierEnumZero,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeFolderTitle = kItemTypeEnumZero,
  ItemTypeParentFolder,
};

}  // namespace

@interface BookmarkFolderEditorViewController ()<
    BookmarkFolderViewControllerDelegate,
    BookmarkModelBridgeObserver,
    BookmarkTextFieldItemDelegate> {
  std::unique_ptr<bookmarks::BookmarkModelBridge> _modelBridge;
  base::mac::ObjCPropertyReleaser
      _propertyReleaser_BookmarkFolderEditorViewController;
  // Flag to ignore bookmark model Move notifications when the move is performed
  // by this class.
  BOOL _ignoresOwnMove;
}
@property(nonatomic, assign) BOOL editingExistingFolder;
@property(nonatomic, assign) bookmarks::BookmarkModel* bookmarkModel;
@property(nonatomic, assign) ios::ChromeBrowserState* browserState;
@property(nonatomic, assign) const BookmarkNode* folder;
@property(nonatomic, retain) BookmarkFolderViewController* folderViewController;
@property(nonatomic, assign) const BookmarkNode* parentFolder;
@property(nonatomic, assign) UIBarButtonItem* doneItem;
@property(nonatomic, retain) BookmarkTextFieldItem* titleItem;
@property(nonatomic, retain) BookmarkParentFolderItem* parentFolderItem;
// Bottom toolbar with DELETE button that only appears when the edited folder
// allows deletion.
@property(nonatomic, assign) BookmarksElevatedToolbar* toolbar;

// |bookmarkModel| must not be NULL and must be loaded.
- (instancetype)initWithBookmarkModel:(bookmarks::BookmarkModel*)bookmarkModel
    NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithStyle:(CollectionViewControllerStyle)style
    NS_UNAVAILABLE;

// Enables or disables the save button depending on the state of the form.
- (void)updateSaveButtonState;

// Configures collection view model.
- (void)setupCollectionViewModel;

// Adds toolbar with DELETE button.
- (void)addToolbar;

// Removes toolbar.
- (void)removeToolbar;

@end

@implementation BookmarkFolderEditorViewController

@synthesize bookmarkModel = _bookmarkModel;
@synthesize delegate = _delegate;
@synthesize editingExistingFolder = _editingExistingFolder;
@synthesize folder = _folder;
@synthesize folderViewController = _folderViewController;
@synthesize parentFolder = _parentFolder;
@synthesize browserState = _browserState;
@synthesize doneItem = _doneItem;
@synthesize titleItem = _titleItem;
@synthesize parentFolderItem = _parentFolderItem;
@synthesize toolbar = _toolbar;

#pragma mark - Class methods

+ (instancetype)
folderCreatorWithBookmarkModel:(bookmarks::BookmarkModel*)bookmarkModel
                  parentFolder:(const BookmarkNode*)parentFolder {
  base::scoped_nsobject<BookmarkFolderEditorViewController> folderCreator(
      [[self alloc] initWithBookmarkModel:bookmarkModel]);
  folderCreator.get().parentFolder = parentFolder;
  folderCreator.get().folder = NULL;
  folderCreator.get().editingExistingFolder = NO;
  return folderCreator.autorelease();
}

+ (instancetype)
folderEditorWithBookmarkModel:(bookmarks::BookmarkModel*)bookmarkModel
                       folder:(const BookmarkNode*)folder
                 browserState:(ios::ChromeBrowserState*)browserState {
  DCHECK(folder);
  DCHECK(!bookmarkModel->is_permanent_node(folder));
  DCHECK(browserState);
  base::scoped_nsobject<BookmarkFolderEditorViewController> folderEditor(
      [[self alloc] initWithBookmarkModel:bookmarkModel]);
  folderEditor.get().parentFolder = folder->parent();
  folderEditor.get().folder = folder;
  folderEditor.get().browserState = browserState;
  folderEditor.get().editingExistingFolder = YES;
  return folderEditor.autorelease();
}

#pragma mark - Initialization

- (instancetype)initWithBookmarkModel:(bookmarks::BookmarkModel*)bookmarkModel {
  DCHECK(bookmarkModel);
  DCHECK(bookmarkModel->loaded());
  self = [super initWithStyle:CollectionViewControllerStyleAppBar];
  if (self) {
    _propertyReleaser_BookmarkFolderEditorViewController.Init(
        self, [BookmarkFolderEditorViewController class]);
    _bookmarkModel = bookmarkModel;

    // Set up the bookmark model oberver.
    _modelBridge.reset(
        new bookmarks::BookmarkModelBridge(self, _bookmarkModel));
  }
  return self;
}

- (instancetype)initWithStyle:(CollectionViewControllerStyle)style {
  NOTREACHED();
  return nil;
}

- (void)dealloc {
  _titleItem.delegate = nil;
  _folderViewController.delegate = nil;
  [super dealloc];
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.collectionView.backgroundColor = [UIColor whiteColor];

  // Add Done button.
  base::scoped_nsobject<UIBarButtonItem> doneItem([[UIBarButtonItem alloc]
      initWithTitle:l10n_util::GetNSString(
                        IDS_IOS_BOOKMARK_EDIT_MODE_EXIT_MOBILE)
              style:UIBarButtonItemStylePlain
             target:self
             action:@selector(saveFolder)]);
  doneItem.get().accessibilityIdentifier = @"Save";
  self.navigationItem.rightBarButtonItem = doneItem;
  self.doneItem = doneItem;

  if (self.editingExistingFolder) {
    // Add Cancel Button.
    UIBarButtonItem* cancelItem =
        [ChromeIcon templateBarButtonItemWithImage:[ChromeIcon closeIcon]
                                            target:self
                                            action:@selector(cancel)];
    cancelItem.accessibilityLabel =
        l10n_util::GetNSString(IDS_IOS_BOOKMARK_NEW_CANCEL_BUTTON_LABEL);
    cancelItem.accessibilityIdentifier = @"Cancel";
    self.navigationItem.leftBarButtonItem = cancelItem;

    [self addToolbar];
  } else {
    // Add Back button.
    UIBarButtonItem* backItem =
        [ChromeIcon templateBarButtonItemWithImage:[ChromeIcon backIcon]
                                            target:self
                                            action:@selector(back)];
    backItem.accessibilityLabel =
        l10n_util::GetNSString(IDS_IOS_BOOKMARK_NEW_BACK_LABEL);
    backItem.accessibilityIdentifier = @"Back";
    self.navigationItem.leftBarButtonItem = backItem;
  }

  [self updateEditingState];
  [self setupCollectionViewModel];
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];
  [self updateSaveButtonState];
}

#pragma mark - Accessibility

- (BOOL)accessibilityPerformEscape {
  [self.delegate bookmarkFolderEditorDidCancel:self];
  return YES;
}

#pragma mark - Actions

- (void)back {
  [self.delegate bookmarkFolderEditorDidCancel:self];
}

- (void)cancel {
  [self.delegate bookmarkFolderEditorDidCancel:self];
}

- (void)deleteFolder {
  DCHECK(self.editingExistingFolder);
  DCHECK(self.folder);
  std::set<const BookmarkNode*> editedNodes;
  editedNodes.insert(self.folder);
  bookmark_utils_ios::DeleteBookmarksWithUndoToast(
      editedNodes, self.bookmarkModel, self.browserState);
  [self.delegate bookmarkFolderEditorDidDeleteEditedFolder:self];
}

- (void)saveFolder {
  DCHECK(self.parentFolder);

  NSString* folderString = self.titleItem.text;
  DCHECK(folderString.length > 0);
  base::string16 folderTitle = base::SysNSStringToUTF16(folderString);

  if (self.editingExistingFolder) {
    DCHECK(self.folder);
    self.bookmarkModel->SetTitle(self.folder, folderTitle);
    if (self.folder->parent() != self.parentFolder) {
      base::AutoReset<BOOL> autoReset(&_ignoresOwnMove, YES);
      std::set<const BookmarkNode*> editedNodes;
      editedNodes.insert(self.folder);
      bookmark_utils_ios::MoveBookmarksWithUndoToast(
          editedNodes, self.bookmarkModel, self.parentFolder,
          self.browserState);
    }
  } else {
    DCHECK(!self.folder);
    self.folder = self.bookmarkModel->AddFolder(
        self.parentFolder, self.parentFolder->child_count(), folderTitle);
  }
  [self.delegate bookmarkFolderEditor:self didFinishEditingFolder:self.folder];
}

- (void)changeParentFolder {
  std::set<const BookmarkNode*> editedNodes;
  if (self.folder)
    editedNodes.insert(self.folder);
  base::scoped_nsobject<BookmarkFolderViewController> folderViewController(
      [[BookmarkFolderViewController alloc]
          initWithBookmarkModel:self.bookmarkModel
               allowsNewFolders:NO
                    editedNodes:editedNodes
                   allowsCancel:NO
                 selectedFolder:self.parentFolder]);
  folderViewController.get().delegate = self;
  self.folderViewController = folderViewController;

  [self.navigationController pushViewController:folderViewController
                                       animated:YES];
}

#pragma mark - BookmarkFolderViewControllerDelegate

- (void)folderPicker:(BookmarkFolderViewController*)folderPicker
    didFinishWithFolder:(const BookmarkNode*)folder {
  self.parentFolder = folder;
  [self updateParentFolderState];
  [self.navigationController popViewControllerAnimated:YES];
  self.folderViewController.delegate = nil;
  self.folderViewController = nil;
}

- (void)folderPickerDidCancel:(BookmarkFolderViewController*)folderPicker {
  [self.navigationController popViewControllerAnimated:YES];
  self.folderViewController.delegate = nil;
  self.folderViewController = nil;
}

#pragma mark - BookmarkModelBridgeObserver

- (void)bookmarkModelLoaded {
  // The bookmark model is assumed to be loaded when this controller is created.
  NOTREACHED();
}

- (void)bookmarkNodeChanged:(const BookmarkNode*)bookmarkNode {
  if (bookmarkNode == self.parentFolder) {
    [self updateParentFolderState];
  }
}

- (void)bookmarkNodeChildrenChanged:(const BookmarkNode*)bookmarkNode {
  // No-op.
}

- (void)bookmarkNode:(const BookmarkNode*)bookmarkNode
     movedFromParent:(const BookmarkNode*)oldParent
            toParent:(const BookmarkNode*)newParent {
  if (_ignoresOwnMove)
    return;
  if (bookmarkNode == self.folder) {
    DCHECK(oldParent == self.parentFolder);
    self.parentFolder = newParent;
    [self updateParentFolderState];
  }
}

- (void)bookmarkNodeDeleted:(const BookmarkNode*)bookmarkNode
                 fromFolder:(const BookmarkNode*)folder {
  if (bookmarkNode == self.parentFolder) {
    self.parentFolder = NULL;
    [self updateParentFolderState];
    return;
  }
  if (bookmarkNode == self.folder) {
    self.folder = NULL;
    self.editingExistingFolder = NO;
    [self updateEditingState];
  }
}

- (void)bookmarkModelRemovedAllNodes {
  if (self.bookmarkModel->is_permanent_node(self.parentFolder))
    return;  // The current parent folder is still valid.

  self.parentFolder = NULL;
  [self updateParentFolderState];
}

#pragma mark - BookmarkTextFieldItemDelegate

- (void)textDidChangeForItem:(BookmarkTextFieldItem*)item {
  [self updateSaveButtonState];
}

- (BOOL)textFieldShouldReturn:(UITextField*)textField {
  [textField resignFirstResponder];
  return YES;
}

#pragma mark - UICollectionViewDelegate

- (void)collectionView:(UICollectionView*)collectionView
    didSelectItemAtIndexPath:(NSIndexPath*)indexPath {
  [super collectionView:collectionView didSelectItemAtIndexPath:indexPath];
  if ([self.collectionViewModel itemTypeForIndexPath:indexPath] ==
      ItemTypeParentFolder) {
    [self changeParentFolder];
  }
}

#pragma mark - UICollectionViewFlowLayout

- (CGSize)collectionView:(UICollectionView*)collectionView
                    layout:(UICollectionViewLayout*)collectionViewLayout
    sizeForItemAtIndexPath:(NSIndexPath*)indexPath {
  switch ([self.collectionViewModel itemTypeForIndexPath:indexPath]) {
    case ItemTypeFolderTitle: {
      const CGFloat kTitleCellHeight = 96;
      return CGSizeMake(CGRectGetWidth(collectionView.bounds),
                        kTitleCellHeight);
    }
    case ItemTypeParentFolder: {
      const CGFloat kParentFolderCellHeight = 50;
      return CGSizeMake(CGRectGetWidth(collectionView.bounds),
                        kParentFolderCellHeight);
    }
    default:
      NOTREACHED();
      return CGSizeZero;
  }
}

#pragma mark - Private

- (void)setParentFolder:(const BookmarkNode*)parentFolder {
  if (!parentFolder) {
    parentFolder = self.bookmarkModel->mobile_node();
  }
  _parentFolder = parentFolder;
}

- (void)updateEditingState {
  if (![self isViewLoaded])
    return;

  self.view.accessibilityIdentifier =
      (self.folder) ? @"Folder Editor" : @"Folder Creator";

  [self setTitle:(self.folder)
                     ? l10n_util::GetNSString(
                           IDS_IOS_BOOKMARK_NEW_GROUP_EDITOR_EDIT_TITLE)
                     : l10n_util::GetNSString(
                           IDS_IOS_BOOKMARK_NEW_GROUP_EDITOR_CREATE_TITLE)];
}

- (void)updateParentFolderState {
  NSIndexPath* folderSelectionIndexPath =
      [self.collectionViewModel indexPathForItemType:ItemTypeParentFolder
                                   sectionIdentifier:SectionIdentifierInfo];
  self.parentFolderItem.title =
      bookmark_utils_ios::TitleForBookmarkNode(self.parentFolder);
  [self.collectionView reloadItemsAtIndexPaths:@[ folderSelectionIndexPath ]];

  if (self.editingExistingFolder && !self.toolbar)
    [self addToolbar];

  if (!self.editingExistingFolder && self.toolbar)
    [self removeToolbar];
}

- (void)setupCollectionViewModel {
  [self loadModel];

  [self.collectionViewModel addSectionWithIdentifier:SectionIdentifierInfo];

  base::scoped_nsobject<BookmarkTextFieldItem> titleItem(
      [[BookmarkTextFieldItem alloc] initWithType:ItemTypeFolderTitle]);
  titleItem.get().text =
      (self.folder)
          ? bookmark_utils_ios::TitleForBookmarkNode(self.folder)
          : l10n_util::GetNSString(IDS_IOS_BOOKMARK_NEW_GROUP_DEFAULT_NAME);
  titleItem.get().placeholder =
      l10n_util::GetNSString(IDS_IOS_BOOKMARK_NEW_EDITOR_NAME_LABEL);
  titleItem.get().accessibilityIdentifier = @"Title";
  [self.collectionViewModel addItem:titleItem
            toSectionWithIdentifier:SectionIdentifierInfo];
  titleItem.get().delegate = self;
  self.titleItem = titleItem;

  base::scoped_nsobject<BookmarkParentFolderItem> parentFolderItem(
      [[BookmarkParentFolderItem alloc] initWithType:ItemTypeParentFolder]);
  parentFolderItem.get().title =
      bookmark_utils_ios::TitleForBookmarkNode(self.parentFolder);
  [self.collectionViewModel addItem:parentFolderItem
            toSectionWithIdentifier:SectionIdentifierInfo];
  self.parentFolderItem = parentFolderItem;
}

- (void)addToolbar {
  // Add bottom toolbar with Delete button.
  base::scoped_nsobject<BookmarksElevatedToolbar> buttonBar(
      [[BookmarksElevatedToolbar alloc] init]);
  base::scoped_nsobject<UIBarButtonItem> deleteItem([[UIBarButtonItem alloc]
      initWithTitle:l10n_util::GetNSString(IDS_IOS_BOOKMARK_GROUP_DELETE)
              style:UIBarButtonItemStylePlain
             target:self
             action:@selector(deleteFolder)]);
  deleteItem.get().accessibilityIdentifier = @"Delete Folder";
  [deleteItem setTitleTextAttributes:@{
    NSForegroundColorAttributeName : [UIColor blackColor]
  }
                            forState:UIControlStateNormal];
  [buttonBar.get().layer
      addSublayer:[[[MDCShadowLayer alloc] init] autorelease]];
  buttonBar.get().shadowElevation = MDCShadowElevationSearchBarResting;
  buttonBar.get().items = @[ deleteItem ];
  [self.view addSubview:buttonBar];

  // Constraint |buttonBar| to be in bottom.
  buttonBar.get().translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addConstraints:
                 [NSLayoutConstraint
                     constraintsWithVisualFormat:@"H:|[buttonBar]|"
                                         options:0
                                         metrics:nil
                                           views:NSDictionaryOfVariableBindings(
                                                     buttonBar)]];
  [self.view addConstraint:[NSLayoutConstraint
                               constraintWithItem:buttonBar
                                        attribute:NSLayoutAttributeBottom
                                        relatedBy:NSLayoutRelationEqual
                                           toItem:self.view
                                        attribute:NSLayoutAttributeBottom
                                       multiplier:1.0
                                         constant:0.0]];
  [self.view
      addConstraint:[NSLayoutConstraint
                        constraintWithItem:buttonBar
                                 attribute:NSLayoutAttributeHeight
                                 relatedBy:NSLayoutRelationEqual
                                    toItem:nil
                                 attribute:NSLayoutAttributeNotAnAttribute
                                multiplier:1.0
                                  constant:48.0]];
  self.toolbar = buttonBar;
}

- (void)removeToolbar {
  [self.toolbar removeFromSuperview];
  self.toolbar = nil;
}

- (void)updateSaveButtonState {
  self.doneItem.enabled = (self.titleItem.text.length > 0);
}

@end
