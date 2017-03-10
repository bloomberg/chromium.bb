// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bookmarks/bookmark_edit_view_controller.h"

#include <memory>
#include <set>

#include "base/auto_reset.h"
#include "base/ios/block_types.h"
#include "base/ios/weak_nsobject.h"
#include "base/logging.h"
#include "base/mac/bind_objc_block.h"
#import "base/mac/foundation_util.h"
#include "base/mac/objc_property_releaser.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/url_formatter/url_fixer.h"
#include "ios/chrome/browser/bookmarks/bookmark_model_factory.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_elevated_toolbar.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_extended_button.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_folder_view_controller.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_interaction_controller.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_model_bridge_observer.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_utils_ios.h"
#import "ios/chrome/browser/ui/bookmarks/cells/bookmark_parent_folder_item.h"
#import "ios/chrome/browser/ui/bookmarks/cells/bookmark_text_field_item.h"
#import "ios/chrome/browser/ui/collection_view/collection_view_model.h"
#import "ios/chrome/browser/ui/icons/chrome_icon.h"
#import "ios/chrome/browser/ui/image_util.h"
#import "ios/chrome/browser/ui/keyboard/UIKeyCommand+Chrome.h"
#include "ios/chrome/browser/ui/rtl_geometry.h"
#include "ios/chrome/browser/ui/ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#import "ios/public/provider/chrome/browser/ui/text_field_styling.h"
#import "ios/third_party/material_components_ios/src/components/Palettes/src/MaterialPalettes.h"
#import "ios/third_party/material_components_ios/src/components/ShadowElevations/src/MaterialShadowElevations.h"
#import "ios/third_party/material_components_ios/src/components/ShadowLayer/src/MaterialShadowLayer.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"

using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;

namespace {
// Converts NSString entered by the user to a GURL.
GURL ConvertUserDataToGURL(NSString* urlString) {
  if (urlString) {
    return url_formatter::FixupURL(base::SysNSStringToUTF8(urlString),
                                   std::string());
  } else {
    return GURL();
  }
}

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierInfo = kSectionIdentifierEnumZero,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeName = kItemTypeEnumZero,
  ItemTypeFolder,
  ItemTypeURL,
};
}  // namespace

@interface BookmarkEditViewController ()<BookmarkFolderViewControllerDelegate,
                                         BookmarkModelBridgeObserver,
                                         BookmarkTextFieldItemDelegate,
                                         TextFieldValidation> {
  // Flag to ignore bookmark model changes notifications.
  BOOL _ignoresBookmarkModelChanges;

  std::unique_ptr<bookmarks::BookmarkModelBridge> _modelBridge;

  base::mac::ObjCPropertyReleaser _propertyReleaser_BookmarkEditViewController;
}

// The bookmark this controller displays or edits.
// Redefined to be readwrite.
@property(nonatomic, assign) const BookmarkNode* bookmark;

// Reference to the bookmark model.
@property(nonatomic, assign) BookmarkModel* bookmarkModel;

// The parent of the bookmark. This may be different from |bookmark->parent()|
// if the changes have not been saved yet. |folder| then represents the
// candidate for the new parent of |bookmark|.  This property is always a
// non-NULL, valid folder.
@property(nonatomic, assign) const BookmarkNode* folder;

// The folder picker view controller.
// Redefined to be readwrite.
@property(nonatomic, retain) BookmarkFolderViewController* folderViewController;

@property(nonatomic, assign) ios::ChromeBrowserState* browserState;

// Cancel button item in navigation bar.
@property(nonatomic, retain) UIBarButtonItem* cancelItem;

// Done button item in navigation bar.
@property(nonatomic, retain) UIBarButtonItem* doneItem;

// CollectionViewItem-s from the collection.
@property(nonatomic, retain) BookmarkTextFieldItem* nameItem;
@property(nonatomic, retain) BookmarkParentFolderItem* folderItem;
@property(nonatomic, retain) BookmarkTextFieldItem* URLItem;

// Reports the changes to the delegate, that has the responsibility to save the
// bookmark.
- (void)commitBookmarkChanges;

// Changes |self.folder| and updates the UI accordingly.
// The change is not committed until the user taps the Save button.
- (void)changeFolder:(const BookmarkNode*)folder;

// The Save button is disabled if the form values are deemed non-valid. This
// method updates the state of the Save button accordingly.
- (void)updateSaveButtonState;

// Reloads the folder label text.
- (void)updateFolderLabel;

// Populates the UI with information from the models.
- (void)updateUIFromBookmark;

// Called when the Delete button is pressed.
- (void)deleteBookmark;

// Called when the Folder button is pressed.
- (void)moveBookmark;

// Called when the Cancel button is pressed.
- (void)cancel;

// Called when the Done button is pressed.
- (void)save;

@end

#pragma mark

@implementation BookmarkEditViewController

@synthesize bookmark = _bookmark;
@synthesize bookmarkModel = _bookmarkModel;
@synthesize delegate = _delegate;
@synthesize folder = _folder;
@synthesize folderViewController = _folderViewController;
@synthesize browserState = _browserState;
@synthesize cancelItem = _cancelItem;
@synthesize doneItem = _doneItem;
@synthesize nameItem = _nameItem;
@synthesize folderItem = _folderItem;
@synthesize URLItem = _URLItem;

#pragma mark - Lifecycle

- (instancetype)initWithBookmark:(const BookmarkNode*)bookmark
                    browserState:(ios::ChromeBrowserState*)browserState {
  DCHECK(bookmark);
  DCHECK(browserState);
  self = [super initWithStyle:CollectionViewControllerStyleAppBar];
  if (self) {
    _propertyReleaser_BookmarkEditViewController.Init(
        self, [BookmarkEditViewController class]);
    DCHECK(!bookmark->is_folder());
    DCHECK(!browserState->IsOffTheRecord());
    _bookmark = bookmark;
    _bookmarkModel =
        ios::BookmarkModelFactory::GetForBrowserState(browserState);

    _folder = bookmark->parent();

    // Set up the bookmark model oberver.
    _modelBridge.reset(
        new bookmarks::BookmarkModelBridge(self, _bookmarkModel));

    _browserState = browserState;
  }
  return self;
}

- (void)dealloc {
  _folderViewController.delegate = nil;
  [super dealloc];
}

#pragma mark View lifecycle

- (void)viewDidLoad {
  [super viewDidLoad];
  self.collectionView.backgroundColor = [UIColor whiteColor];
  self.view.accessibilityIdentifier = @"Single Bookmark Editor";

  self.title = l10n_util::GetNSString(IDS_IOS_BOOKMARK_EDIT_SCREEN_TITLE);

  self.navigationItem.hidesBackButton = YES;

  UIBarButtonItem* cancelItem =
      [ChromeIcon templateBarButtonItemWithImage:[ChromeIcon closeIcon]
                                          target:self
                                          action:@selector(cancel)];
  cancelItem.accessibilityIdentifier = @"Cancel";
  self.navigationItem.leftBarButtonItem = cancelItem;
  self.cancelItem = cancelItem;

  base::scoped_nsobject<UIBarButtonItem> doneItem([[UIBarButtonItem alloc]
      initWithTitle:l10n_util::GetNSString(IDS_IOS_BOOKMARK_DONE_BUTTON)
              style:UIBarButtonItemStylePlain
             target:self
             action:@selector(save)]);
  doneItem.get().accessibilityIdentifier = @"Done";
  self.navigationItem.rightBarButtonItem = doneItem;
  self.doneItem = doneItem;

  base::scoped_nsobject<BookmarksElevatedToolbar> buttonBar(
      [[BookmarksElevatedToolbar alloc] init]);
  base::scoped_nsobject<UIBarButtonItem> deleteItem([[UIBarButtonItem alloc]
      initWithTitle:l10n_util::GetNSString(IDS_IOS_BOOKMARK_DELETE)
              style:UIBarButtonItemStylePlain
             target:self
             action:@selector(deleteBookmark)]);
  deleteItem.get().accessibilityIdentifier = @"Delete_action";
  [deleteItem setTitleTextAttributes:@{
    NSForegroundColorAttributeName : [UIColor blackColor]
  }
                            forState:UIControlStateNormal];
  [buttonBar.get().layer
      addSublayer:[[[MDCShadowLayer alloc] init] autorelease]];
  buttonBar.get().shadowElevation = MDCShadowElevationSearchBarResting;
  buttonBar.get().backgroundColor = [UIColor whiteColor];
  buttonBar.get().items = @[ deleteItem ];
  [self.view addSubview:buttonBar];

  // Constraint |buttonBar| to be in bottom
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
  [self updateUIFromBookmark];
}

#pragma mark - Accessibility

- (BOOL)accessibilityPerformEscape {
  [self cancel];
  return YES;
}

#pragma mark - Private

- (BOOL)inputURLIsValid {
  return ConvertUserDataToGURL([self inputURLString]).is_valid();
}

// Retrieves input URL string from UI.
- (NSString*)inputURLString {
  return self.URLItem.text;
}

// Retrieves input bookmark name string from UI.
- (NSString*)inputBookmarkName {
  return self.nameItem.text;
}

- (void)commitBookmarkChanges {
  // To stop getting recursive events from committed bookmark editing changes
  // ignore bookmark model updates notifications.
  base::AutoReset<BOOL> autoReset(&_ignoresBookmarkModelChanges, YES);

  GURL url = ConvertUserDataToGURL([self inputURLString]);
  // If the URL was not valid, the |save| message shouldn't have been sent.
  DCHECK([self inputURLIsValid]);
  bookmark_utils_ios::CreateOrUpdateBookmarkWithUndoToast(
      self.bookmark, [self inputBookmarkName], url, self.folder,
      self.bookmarkModel, self.browserState);
}

- (void)changeFolder:(const BookmarkNode*)folder {
  DCHECK(folder->is_folder());
  self.folder = folder;
  [BookmarkInteractionController setFolderForNewBookmarks:self.folder
                                           inBrowserState:self.browserState];
  [self updateFolderLabel];
}

- (void)dismiss {
  [self.view resignFirstResponder];

  // Dismiss this controller.
  [self.delegate bookmarkEditorWantsDismissal:self];
}

#pragma mark - Layout

- (void)updateSaveButtonState {
  self.doneItem.enabled = [self inputURLIsValid];
}

- (void)updateFolderLabel {
  NSIndexPath* indexPath =
      [self.collectionViewModel indexPathForItemType:ItemTypeFolder
                                   sectionIdentifier:SectionIdentifierInfo];
  NSString* folderName = @"";
  if (self.bookmark) {
    folderName = bookmark_utils_ios::TitleForBookmarkNode(self.folder);
  }

  self.folderItem.title = folderName;
  [self.collectionView reloadItemsAtIndexPaths:@[ indexPath ]];
}

- (void)updateUIFromBookmark {
  // If there is no current bookmark, don't update.
  if (!self.bookmark)
    return;

  [self loadModel];
  CollectionViewModel* model = self.collectionViewModel;

  [model addSectionWithIdentifier:SectionIdentifierInfo];

  self.nameItem =
      [[[BookmarkTextFieldItem alloc] initWithType:ItemTypeName] autorelease];
  self.nameItem.accessibilityIdentifier = @"Title Field";
  self.nameItem.placeholder =
      l10n_util::GetNSString(IDS_IOS_BOOKMARK_NAME_FIELD_HEADER);
  self.nameItem.text = bookmark_utils_ios::TitleForBookmarkNode(self.bookmark);
  self.nameItem.delegate = self;
  [model addItem:self.nameItem toSectionWithIdentifier:SectionIdentifierInfo];

  self.folderItem = [[[BookmarkParentFolderItem alloc]
      initWithType:ItemTypeFolder] autorelease];
  self.folderItem.title = bookmark_utils_ios::TitleForBookmarkNode(self.folder);
  [model addItem:self.folderItem toSectionWithIdentifier:SectionIdentifierInfo];

  self.URLItem =
      [[[BookmarkTextFieldItem alloc] initWithType:ItemTypeURL] autorelease];
  self.URLItem.accessibilityIdentifier = @"URL Field";
  self.URLItem.placeholder =
      l10n_util::GetNSString(IDS_IOS_BOOKMARK_URL_FIELD_HEADER);
  self.URLItem.text = base::SysUTF8ToNSString(self.bookmark->url().spec());
  self.URLItem.delegate = self;
  [model addItem:self.URLItem toSectionWithIdentifier:SectionIdentifierInfo];

  // Save button state.
  [self updateSaveButtonState];
}

#pragma mark - Actions

- (void)deleteBookmark {
  if (self.bookmark && self.bookmarkModel->loaded()) {
    // To stop getting recursive events from committed bookmark editing changes
    // ignore bookmark model updates notifications.
    base::AutoReset<BOOL> autoReset(&_ignoresBookmarkModelChanges, YES);

    std::set<const BookmarkNode*> nodes;
    if ([self.delegate bookmarkEditor:self
            shoudDeleteAllOccurencesOfBookmark:self.bookmark]) {
      // When launched from the star button, removing the current bookmark
      // removes all matching nodes.
      std::vector<const BookmarkNode*> nodesVector;
      self.bookmarkModel->GetNodesByURL(self.bookmark->url(), &nodesVector);
      for (const BookmarkNode* node : nodesVector)
        nodes.insert(node);
    } else {
      // When launched from the info button, removing the current bookmark only
      // removes the current node.
      nodes.insert(self.bookmark);
    }
    bookmark_utils_ios::DeleteBookmarksWithUndoToast(nodes, self.bookmarkModel,
                                                     self.browserState);
    self.bookmark = nil;
  }
  [self.delegate bookmarkEditorWantsDismissal:self];
}

- (void)moveBookmark {
  DCHECK(self.bookmarkModel);
  DCHECK(!self.folderViewController);

  std::set<const BookmarkNode*> editedNodes;
  editedNodes.insert(self.bookmark);
  base::scoped_nsobject<BookmarkFolderViewController> folderViewController(
      [[BookmarkFolderViewController alloc]
          initWithBookmarkModel:self.bookmarkModel
               allowsNewFolders:YES
                    editedNodes:editedNodes
                   allowsCancel:NO
                 selectedFolder:self.folder]);
  folderViewController.get().delegate = self;
  self.folderViewController = folderViewController;

  [self.navigationController pushViewController:self.folderViewController
                                       animated:YES];
}

- (void)cancel {
  [self dismiss];
}

- (void)save {
  [self commitBookmarkChanges];
  [self dismiss];
}

#pragma mark - BookmarkTextFieldItemDelegate

- (void)textDidChangeForItem:(BookmarkTextFieldItem*)item {
  [self updateSaveButtonState];
}

- (BOOL)textFieldShouldReturn:(UITextField*)textField {
  [textField resignFirstResponder];
  return YES;
}

#pragma mark - TextFieldValidation

- (NSString*)validationErrorForTextField:(id<TextFieldStyling>)field {
  [self updateSaveButtonState];
  if ([self inputURLIsValid]) {
    return nil;
  } else {
    return l10n_util::GetNSString(IDS_IOS_BOOKMARK_URL_FIELD_VALIDATION_FAILED);
  }
}

#pragma mark - UICollectionViewDataSource

- (UICollectionViewCell*)collectionView:(UICollectionView*)collectionView
                 cellForItemAtIndexPath:(NSIndexPath*)indexPath {
  UICollectionViewCell* cell =
      [super collectionView:collectionView cellForItemAtIndexPath:indexPath];
  if ([self.collectionViewModel itemTypeForIndexPath:indexPath] ==
      ItemTypeURL) {
    BookmarkTextFieldCell* URLCell =
        base::mac::ObjCCastStrict<BookmarkTextFieldCell>(cell);
    URLCell.textField.textValidator = self;
  }
  return cell;
}

#pragma mark - UICollectionViewDelegate

- (void)collectionView:(UICollectionView*)collectionView
    didSelectItemAtIndexPath:(NSIndexPath*)indexPath {
  [super collectionView:collectionView didSelectItemAtIndexPath:indexPath];
  if ([self.collectionViewModel itemTypeForIndexPath:indexPath] ==
      ItemTypeFolder) {
    [self moveBookmark];
  }
}

#pragma mark - MDCCollectionViewStylingDelegate

- (CGFloat)collectionView:(UICollectionView*)collectionView
    cellHeightAtIndexPath:(NSIndexPath*)indexPath {
  switch ([self.collectionViewModel itemTypeForIndexPath:indexPath]) {
    case ItemTypeName:
    case ItemTypeURL:
      return 88;
    case ItemTypeFolder:
      return 50;
    default:
      NOTREACHED();
      return 0;
  }
}

#pragma mark - BookmarkFolderViewControllerDelegate

- (void)folderPicker:(BookmarkFolderViewController*)folderPicker
    didFinishWithFolder:(const BookmarkNode*)folder {
  [self changeFolder:folder];
  // This delegate method can be called on two occasions:
  // - the user selected a folder in the folder picker. In that case, the folder
  // picker should be popped;
  // - the user created a new folder, in which case the navigation stack
  // contains this bookmark editor (|self|), a folder picker and a folder
  // creator. In such a case, both the folder picker and creator shoud be popped
  // to reveal this bookmark editor. Thus the call to
  // |popToViewController:animated:|.
  [self.navigationController popToViewController:self animated:YES];
  self.folderViewController.delegate = nil;
  self.folderViewController = nil;
}

- (void)folderPickerDidCancel:(BookmarkFolderViewController*)folderPicker {
  // This delegate method can only be called from the folder picker, which is
  // the only view controller on top of this bookmark editor (|self|). Thus the
  // call to |popViewControllerAnimated:|.
  [self.navigationController popViewControllerAnimated:YES];
  self.folderViewController.delegate = nil;
  self.folderViewController = nil;
}

#pragma mark - BookmarkModelBridgeObserver

- (void)bookmarkModelLoaded {
  // No-op.
}

- (void)bookmarkNodeChanged:(const BookmarkNode*)bookmarkNode {
  if (_ignoresBookmarkModelChanges)
    return;

  if (self.bookmark == bookmarkNode)
    [self updateUIFromBookmark];
}

- (void)bookmarkNodeChildrenChanged:(const BookmarkNode*)bookmarkNode {
  if (_ignoresBookmarkModelChanges)
    return;

  [self updateFolderLabel];
}

- (void)bookmarkNode:(const BookmarkNode*)bookmarkNode
     movedFromParent:(const BookmarkNode*)oldParent
            toParent:(const BookmarkNode*)newParent {
  if (_ignoresBookmarkModelChanges)
    return;

  if (self.bookmark == bookmarkNode)
    [self.folderViewController changeSelectedFolder:newParent];
}

- (void)bookmarkNodeDeleted:(const BookmarkNode*)bookmarkNode
                 fromFolder:(const BookmarkNode*)folder {
  if (_ignoresBookmarkModelChanges)
    return;

  if (self.bookmark == bookmarkNode) {
    self.bookmark = nil;
    [self.delegate bookmarkEditorWantsDismissal:self];
  } else if (self.folder == bookmarkNode) {
    [self changeFolder:self.bookmarkModel->mobile_node()];
  }
}

- (void)bookmarkModelRemovedAllNodes {
  if (_ignoresBookmarkModelChanges)
    return;

  self.bookmark = nil;
  if (!self.bookmarkModel->is_permanent_node(self.folder)) {
    [self changeFolder:self.bookmarkModel->mobile_node()];
  }

  [self.delegate bookmarkEditorWantsDismissal:self];
}

#pragma mark - UIResponder

- (NSArray*)keyCommands {
  base::WeakNSObject<BookmarkEditViewController> weakSelf(self);
  return @[ [UIKeyCommand cr_keyCommandWithInput:UIKeyInputEscape
                                   modifierFlags:Cr_UIKeyModifierNone
                                           title:nil
                                          action:^{
                                            [weakSelf dismiss];
                                          }] ];
}

@end
