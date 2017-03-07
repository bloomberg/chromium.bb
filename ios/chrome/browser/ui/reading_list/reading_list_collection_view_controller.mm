// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/reading_list/reading_list_collection_view_controller.h"

#include "base/bind.h"
#include "base/logging.h"
#import "base/mac/foundation_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/sys_string_conversions.h"
#include "base/time/time.h"
#include "components/reading_list/ios/reading_list_entry.h"
#include "components/reading_list/ios/reading_list_model.h"
#import "components/reading_list/ios/reading_list_model_bridge_observer.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/url_formatter.h"
#include "ios/chrome/browser/reading_list/offline_url_utils.h"
#include "ios/chrome/browser/reading_list/reading_list_download_service.h"
#import "ios/chrome/browser/ui/alert_coordinator/action_sheet_coordinator.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_text_item.h"
#import "ios/chrome/browser/ui/collection_view/collection_view_model.h"
#import "ios/chrome/browser/ui/favicon_view.h"
#import "ios/chrome/browser/ui/material_components/utils.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_collection_view_item.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_empty_collection_background.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_toolbar.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"
#include "ios/chrome/browser/ui/url_loader.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/third_party/material_components_ios/src/components/AppBar/src/MaterialAppBar.h"
#import "ios/third_party/material_components_ios/src/components/Palettes/src/MaterialPalettes.h"
#include "ios/web/public/referrer.h"
#include "ios/web/public/web_state/web_state.h"
#include "net/base/network_change_notifier.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/base/window_open_disposition.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierUnread = kSectionIdentifierEnumZero,
  SectionIdentifierRead,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeUnreadHeader = kItemTypeEnumZero,
  ItemTypeUnread,
  ItemTypeReadHeader,
  ItemTypeRead,
};

// Typedef for a block taking a GURL as parameter and returning nothing.
typedef void (^EntryUpdater)(const GURL&);

// Type for map used to sort ReadingListEntry by timestamp. Multiple entries can
// have the same timestamp.
using ItemsMapByDate = std::multimap<int64_t, ReadingListCollectionViewItem*>;
}

@interface ReadingListCollectionViewController ()<
    ReadingListModelBridgeObserver> {
  // Toolbar with the actions.
  ReadingListToolbar* _toolbar;
  // Action sheet presenting the subactions of the toolbar.
  AlertCoordinator* _actionSheet;
  std::unique_ptr<ReadingListModelBridge> _modelBridge;
  UIView* _emptyCollectionBackground;

  // Whether the model has pending modifications.
  BOOL _modelHasBeenModified;
}

// Lazily instantiated.
@property(nonatomic, strong, readonly)
    FaviconAttributesProvider* attributesProvider;
// Whether the Reading List Model (by opposition to the CollectionViewModel)
// modifications should be taken into account.
@property(nonatomic, assign) BOOL shouldMonitorModel;

// Handles "Done" button touches.
- (void)donePressed;
// Loads all the items in all sections.
- (void)loadItems;
// Fills section |sectionIdentifier| with the items from |map| in reverse order
// of the map key.
- (void)loadItemsFromMap:(const ItemsMapByDate&)map
               toSection:(SectionIdentifier)sectionIdentifier;
// Whether the model has changed.
- (BOOL)hasModelChanged;
// Returns whether there is a difference between the elements contained in the
// |sectionIdentifier| and those in the |map|.
- (BOOL)section:(SectionIdentifier)sectionIdentifier
    isDifferentOfMap:(ItemsMapByDate&)map;
// Reloads the data if a changed occured during editing
- (void)applyPendingUpdates;
// Convenience method to create cell items for reading list entries.
- (ReadingListCollectionViewItem*)cellItemForReadingListEntry:
    (const ReadingListEntry&)entry;
// Fills the |unread_map| and the |read_map| with the corresponding
// ReadingListCollectionViewItem from the readingListModel.
- (void)fillUnreadMap:(ItemsMapByDate&)unread_map
              readMap:(ItemsMapByDate&)read_map;
// Returns whether there are elements in the section identified by
// |sectionIdentifier|.
- (BOOL)hasItemInSection:(SectionIdentifier)sectionIdentifier;
// Adds an empty background if needed.
- (void)collectionIsEmpty;
// Handles a long press.
- (void)handleLongPress:(UILongPressGestureRecognizer*)gestureRecognizer;
// Stops observing the ReadingListModel.
- (void)stopObservingReadingListModel;
// Updates the toolbar state according to the selected items.
- (void)updateToolbarState;
// Displays an action sheet to let the user choose to mark all the elements as
// read or as unread. Used when nothing is selected.
- (void)markAllItemsAs;
// Displays an action sheet to let the user choose to mark all the selected
// elements as read or as unread. Used if read and unread elements are selected.
- (void)markMixedItemsAs;
// Marks all items as read.
- (void)markAllRead;
// Marks all items as unread.
- (void)markAllUnread;
// Marks the selected items as read.
- (void)markItemsRead;
// Marks the selected items as unread.
- (void)markItemsUnread;
// Deletes all the read items.
- (void)deleteAllReadItems;
// Deletes all the selected items.
- (void)deleteSelectedItems;
// Initializes |_actionSheet| with |self| as base view controller, and the
// toolbar's mark button as anchor point.
- (void)initializeActionSheet;
// Exits the editing mode and update the toolbar state with animation.
- (void)exitEditingModeAnimated:(BOOL)animated;
// Applies |updater| to the URL of every cell in the section |identifier|. The
// updates are done in reverse order of the cells in the section to keep the
// order. The monitoring of the model updates are suspended during this time.
- (void)updateItemsInSectionIdentifier:(SectionIdentifier)identifier
                     usingEntryUpdater:(EntryUpdater)updater;
// Applies |updater| to the URL of every element in |indexPaths|. The updates
// are done in reverse order |indexPaths| to keep the order. The monitoring of
// the model updates are suspended during this time.
- (void)updateIndexPaths:(NSArray<NSIndexPath*>*)indexPaths
       usingEntryUpdater:(EntryUpdater)updater;
// Logs the deletions histograms for the entry with |url|.
- (void)logDeletionHistogramsForEntry:(const GURL&)url;
// Move all the items from |sourceSectionIdentifier| to
// |destinationSectionIdentifier| and removes the empty section from the
// collection.
- (void)moveItemsFromSection:(SectionIdentifier)sourceSectionIdentifier
                   toSection:(SectionIdentifier)destinationSectionIdentifier;
// Move the currently selected elements to |sectionIdentifier| and removes the
// empty sections.
- (void)moveSelectedItems:(NSArray*)sortedIndexPaths
                toSection:(SectionIdentifier)sectionIdentifier;
// Makes sure |sectionIdentifier| exists with the correct header.
// Returns the index of the new section in the collection view; NSIntegerMax if
// no section has been created.
- (NSInteger)initializeSection:(SectionIdentifier)sectionIdentifier;
// Returns the header for the |sectionIdentifier|.
- (CollectionViewTextItem*)headerForSection:
    (SectionIdentifier)sectionIdentifier;
// Removes the empty sections from the collection and the model.
- (void)removeEmptySections;

@end

@implementation ReadingListCollectionViewController

@synthesize audience = _audience;
@synthesize delegate = _delegate;

@synthesize readingListModel = _readingListModel;
@synthesize largeIconService = _largeIconService;
@synthesize readingListDownloadService = _readingListDownloadService;
@synthesize attributesProvider = _attributesProvider;
@synthesize shouldMonitorModel = _shouldMonitorModel;

#pragma mark lifecycle

- (instancetype)initWithModel:(ReadingListModel*)model
              largeIconService:(favicon::LargeIconService*)largeIconService
    readingListDownloadService:
        (ReadingListDownloadService*)readingListDownloadService
                       toolbar:(ReadingListToolbar*)toolbar {
  self = [super initWithStyle:CollectionViewControllerStyleAppBar];
  if (self) {
    DCHECK(model);
    _toolbar = toolbar;

    _readingListModel = model;
    _largeIconService = largeIconService;
    _readingListDownloadService = readingListDownloadService;
    _emptyCollectionBackground =
        [[ReadingListEmptyCollectionBackground alloc] init];

    _shouldMonitorModel = YES;
    _modelHasBeenModified = NO;

    _modelBridge.reset(new ReadingListModelBridge(self, model));
  }
  return self;
}

#pragma mark - properties

- (FaviconAttributesProvider*)attributesProvider {
  if (_attributesProvider) {
    return _attributesProvider;
  }

  // Accept any favicon even the smallest ones (16x16) as it is better than the
  // fallback icon.
  // Pass 1 as minimum size to avoid empty favicons.
  _attributesProvider = [[FaviconAttributesProvider alloc]
      initWithFaviconSize:kFaviconPreferredSize
           minFaviconSize:1
         largeIconService:self.largeIconService];
  return _attributesProvider;
}

- (void)setToolbarState:(ReadingListToolbarState)toolbarState {
  [_toolbar setState:toolbarState];
}

- (void)setAudience:(id<ReadingListCollectionViewControllerAudience>)audience {
  _audience = audience;
  if (self.readingListModel->loaded()) {
    [audience readingListHasItems:(self.readingListModel->size() > 0)];
  }
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  self.title = l10n_util::GetNSString(IDS_IOS_TOOLS_MENU_READING_LIST);

  // Add "Done" button.
  UIBarButtonItem* doneItem = [[UIBarButtonItem alloc]
      initWithTitle:l10n_util::GetNSString(IDS_IOS_READING_LIST_DONE_BUTTON)
              style:UIBarButtonItemStylePlain
             target:self
             action:@selector(donePressed)];
  doneItem.accessibilityIdentifier = @"Done";
  self.navigationItem.rightBarButtonItem = doneItem;

  // Customize collection view settings.
  self.styler.cellStyle = MDCCollectionViewCellStyleCard;
  self.styler.separatorInset = UIEdgeInsetsMake(0, 16, 0, 16);

  UILongPressGestureRecognizer* longPressRecognizer =
      [[UILongPressGestureRecognizer alloc]
          initWithTarget:self
                  action:@selector(handleLongPress:)];
  longPressRecognizer.numberOfTouchesRequired = 1;
  [self.collectionView addGestureRecognizer:longPressRecognizer];
}

#pragma mark - UICollectionViewDelegate

- (UICollectionReusableView*)collectionView:(UICollectionView*)collectionView
          viewForSupplementaryElementOfKind:(NSString*)kind
                                atIndexPath:(NSIndexPath*)indexPath {
  UICollectionReusableView* cell = [super collectionView:collectionView
                       viewForSupplementaryElementOfKind:kind
                                             atIndexPath:indexPath];
  MDCCollectionViewTextCell* textCell =
      base::mac::ObjCCast<MDCCollectionViewTextCell>(cell);
  if (textCell) {
    textCell.textLabel.textColor = [[MDCPalette greyPalette] tint500];
  }
  return cell;
};

- (void)collectionView:(UICollectionView*)collectionView
    didSelectItemAtIndexPath:(NSIndexPath*)indexPath {
  [super collectionView:collectionView didSelectItemAtIndexPath:indexPath];

  if (self.editor.editing) {
    [self updateToolbarState];
  } else {
    ReadingListCollectionViewItem* readingListItem =
        base::mac::ObjCCastStrict<ReadingListCollectionViewItem>(
            [self.collectionViewModel itemAtIndexPath:indexPath]);

    [self.delegate readingListCollectionViewController:self
                                              openItem:readingListItem];
  }
}

- (void)collectionView:(UICollectionView*)collectionView
    didDeselectItemAtIndexPath:(NSIndexPath*)indexPath {
  [super collectionView:collectionView didDeselectItemAtIndexPath:indexPath];
  if (self.editor.editing) {
    // When deselecting an item, if we are editing, we want to update the
    // toolbar base on the selected items.
    [self updateToolbarState];
  }
}

#pragma mark - MDCCollectionViewController

- (void)updateFooterInfoBarIfNecessary {
  // No-op. This prevents the default infobar from showing.
  // TODO(crbug.com/653547): Remove this once the MDC adds an option for
  // preventing the infobar from showing.
}

#pragma mark - MDCCollectionViewStylingDelegate

- (CGFloat)collectionView:(UICollectionView*)collectionView
    cellHeightAtIndexPath:(NSIndexPath*)indexPath {
  NSInteger type = [self.collectionViewModel itemTypeForIndexPath:indexPath];
  if (type == ItemTypeUnread || type == ItemTypeRead)
    return MDCCellDefaultTwoLineHeight;
  else
    return MDCCellDefaultOneLineHeight;
}

#pragma mark - MDCCollectionViewEditingDelegate

- (BOOL)collectionViewAllowsEditing:(nonnull UICollectionView*)collectionView {
  return YES;
}

#pragma mark - ReadingListModelBridgeObserver

- (void)readingListModelLoaded:(const ReadingListModel*)model {
  [self loadModel];
  UMA_HISTOGRAM_COUNTS_1000("ReadingList.Unread.Number", model->unread_size());
  UMA_HISTOGRAM_COUNTS_1000("ReadingList.Read.Number",
                            model->size() - model->unread_size());
  if ([self isViewLoaded]) {
    [self.collectionView reloadData];
  }
}

- (void)readingListModelDidApplyChanges:(const ReadingListModel*)model {
  if (!self.shouldMonitorModel) {
    return;
  }

  // If we are editing and monitoring the model updates, set a flag to reload
  // the data at the end of the editing.
  if ([self.editor isEditing]) {
    _modelHasBeenModified = YES;
    return;
  }

  // Ignore model updates when the view controller is doing batch updates.
  if (model->IsPerformingBatchUpdates()) {
    return;
  }

  if ([self hasModelChanged])
    [self reloadData];
}

- (void)readingListModelCompletedBatchUpdates:(const ReadingListModel*)model {
  if (!self.shouldMonitorModel) {
    return;
  }

  if ([self hasModelChanged])
    [self reloadData];
}

#pragma mark - Public methods

- (void)reloadData {
  [self loadModel];
  if ([self isViewLoaded]) {
    [self.collectionView reloadData];
  }
}

- (void)willBeDismissed {
  _readingListModel->MarkAllSeen();
  // Reset observer to prevent further model update notifications.
  [self stopObservingReadingListModel];
  [_actionSheet stop];
}

#pragma mark - Private methods

- (void)donePressed {
  if ([self.editor isEditing]) {
    [self exitEditingModeAnimated:NO];
  }
  [self dismiss];
}

- (void)dismiss {
  [self.delegate dismissReadingListCollectionViewController:self];
}

- (void)loadModel {
  [super loadModel];

  if (self.readingListModel->size() == 0) {
    [self collectionIsEmpty];
  } else {
    self.collectionView.alwaysBounceVertical = YES;
    [self loadItems];
    self.collectionView.backgroundView = nil;
    [self.audience readingListHasItems:YES];
  }
}

- (void)loadItemsFromMap:(const ItemsMapByDate&)map
               toSection:(SectionIdentifier)sectionIdentifier {
  if (map.size() == 0) {
    return;
  }
  CollectionViewModel* model = self.collectionViewModel;
  [model addSectionWithIdentifier:sectionIdentifier];
  [model setHeader:[self headerForSection:sectionIdentifier]
      forSectionWithIdentifier:sectionIdentifier];
  // Reverse iterate to add newer entries at the top.
  ItemsMapByDate::const_reverse_iterator iterator = map.rbegin();
  for (; iterator != map.rend(); iterator++) {
    [model addItem:iterator->second toSectionWithIdentifier:sectionIdentifier];
  }
}

- (void)loadItems {
  ItemsMapByDate read_map;
  ItemsMapByDate unread_map;
  [self fillUnreadMap:unread_map readMap:read_map];
  [self loadItemsFromMap:unread_map toSection:SectionIdentifierUnread];
  [self loadItemsFromMap:read_map toSection:SectionIdentifierRead];

  BOOL hasRead = read_map.size() > 0;
  [_toolbar setHasReadItem:hasRead];
}

- (void)fillUnreadMap:(ItemsMapByDate&)unread_map
              readMap:(ItemsMapByDate&)read_map {
  for (const auto& url : self.readingListModel->Keys()) {
    const ReadingListEntry* entry = self.readingListModel->GetEntryByURL(url);
    ReadingListCollectionViewItem* item =
        [self cellItemForReadingListEntry:*entry];
    if (entry->IsRead()) {
      read_map.insert(std::make_pair(entry->UpdateTime(), item));
    } else {
      unread_map.insert(std::make_pair(entry->UpdateTime(), item));
    }
  }
}

- (void)applyPendingUpdates {
  if (_modelHasBeenModified) {
    [self reloadData];
  }
}

- (BOOL)hasModelChanged {
  ItemsMapByDate read_map;
  ItemsMapByDate unread_map;
  [self fillUnreadMap:unread_map readMap:read_map];

  if ([self section:SectionIdentifierRead isDifferentOfMap:read_map])
    return YES;
  if ([self section:SectionIdentifierUnread isDifferentOfMap:unread_map])
    return YES;

  return NO;
}

- (BOOL)section:(SectionIdentifier)sectionIdentifier
    isDifferentOfMap:(ItemsMapByDate&)map {
  if (![self.collectionViewModel
          hasSectionForSectionIdentifier:sectionIdentifier]) {
    return !map.empty();
  }

  NSArray* items =
      [self.collectionViewModel itemsInSectionWithIdentifier:sectionIdentifier];
  if ([items count] != map.size())
    return YES;

  NSInteger index = 0;
  ItemsMapByDate::const_reverse_iterator iterator = map.rbegin();
  for (; iterator != map.rend(); iterator++) {
    ReadingListCollectionViewItem* oldItem =
        base::mac::ObjCCastStrict<ReadingListCollectionViewItem>(items[index]);
    ReadingListCollectionViewItem* newItem = iterator->second;
    if (oldItem.url == newItem.url) {
      oldItem.text = newItem.text;
      oldItem.distillationState = newItem.distillationState;
      oldItem.faviconPageURL = newItem.faviconPageURL;
    }
    if (![oldItem isEqual:newItem]) {
      return YES;
    }
    index++;
  }
  return NO;
}

- (ReadingListCollectionViewItem*)cellItemForReadingListEntry:
    (const ReadingListEntry&)entry {
  GURL url = entry.URL();
  ReadingListCollectionViewItem* item = [[ReadingListCollectionViewItem alloc]
            initWithType:entry.IsRead() ? ItemTypeRead : ItemTypeUnread
      attributesProvider:self.attributesProvider
                     url:url
       distillationState:entry.DistilledState()];
  NSString* urlString = base::SysUTF16ToNSString(url_formatter::FormatUrl(url));
  NSString* title = base::SysUTF8ToNSString(entry.Title());
  item.text = [title length] ? title : urlString;
  item.detailText = urlString;
  item.faviconPageURL =
      entry.DistilledURL().is_valid() ? entry.DistilledURL() : url;
  return item;
}

- (BOOL)hasItemInSection:(SectionIdentifier)sectionIdentifier {
  if (![self.collectionViewModel
          hasSectionForSectionIdentifier:sectionIdentifier]) {
    // No section.
    return NO;
  }

  NSInteger section =
      [self.collectionViewModel sectionForSectionIdentifier:sectionIdentifier];
  NSInteger numberOfItems =
      [self.collectionViewModel numberOfItemsInSection:section];

  return numberOfItems > 0;
}

- (void)collectionIsEmpty {
  if (self.collectionView.backgroundView) {
    return;
  }
  // The collection is empty, add background.
  self.collectionView.alwaysBounceVertical = NO;
  self.collectionView.backgroundView = _emptyCollectionBackground;
  [self.audience readingListHasItems:NO];
}

- (void)handleLongPress:(UILongPressGestureRecognizer*)gestureRecognizer {
  if (self.editor.editing ||
      gestureRecognizer.state != UIGestureRecognizerStateBegan) {
    return;
  }

  CGPoint touchLocation =
      [gestureRecognizer locationOfTouch:0 inView:self.collectionView];
  NSIndexPath* touchedItemIndexPath =
      [self.collectionView indexPathForItemAtPoint:touchLocation];
  if (!touchedItemIndexPath ||
      ![self.collectionViewModel hasItemAtIndexPath:touchedItemIndexPath]) {
    // Make sure there is an item at this position.
    return;
  }
  CollectionViewItem* touchedItem =
      [self.collectionViewModel itemAtIndexPath:touchedItemIndexPath];

  if (touchedItem == [self.collectionViewModel
                         headerForSection:touchedItemIndexPath.section] ||
      ![touchedItem isKindOfClass:[ReadingListCollectionViewItem class]]) {
    // Do not trigger context menu on headers.
    return;
  }

  ReadingListCollectionViewItem* readingListItem =
      base::mac::ObjCCastStrict<ReadingListCollectionViewItem>(touchedItem);

  [self.delegate readingListCollectionViewController:self
                           displayContextMenuForItem:readingListItem
                                             atPoint:touchLocation];
}

- (void)stopObservingReadingListModel {
  _modelBridge.reset();
}

#pragma mark - ReadingListToolbarDelegate

- (void)markPressed {
  if (![self.editor isEditing]) {
    return;
  }
  switch ([_toolbar state]) {
    case NoneSelected:
      [self markAllItemsAs];
      break;
    case OnlyUnreadSelected:
      [self markItemsRead];
      break;
    case OnlyReadSelected:
      [self markItemsUnread];
      break;
    case MixedItemsSelected:
      [self markMixedItemsAs];
      break;
  }
}

- (void)deletePressed {
  if (![self.editor isEditing]) {
    return;
  }
  if ([_toolbar state] == NoneSelected) {
    [self deleteAllReadItems];
  } else {
    [self deleteSelectedItems];
  }
}
- (void)enterEditingModePressed {
  if ([self.editor isEditing]) {
    return;
  }
  self.toolbarState = NoneSelected;
  [self.editor setEditing:YES animated:YES];
  [_toolbar setEditing:YES];
}

- (void)exitEditingModePressed {
  if (![self.editor isEditing]) {
    return;
  }
  [self exitEditingModeAnimated:YES];
}

#pragma mark - Private methods - Toolbar

- (void)updateToolbarState {
  BOOL readSelected = NO;
  BOOL unreadSelected = NO;

  if ([self.collectionView.indexPathsForSelectedItems count] == 0) {
    // No entry selected.
    self.toolbarState = NoneSelected;
    return;
  }

  // Sections for section identifiers.
  NSInteger sectionRead = NSNotFound;
  NSInteger sectionUnread = NSNotFound;

  if ([self hasItemInSection:SectionIdentifierRead]) {
    sectionRead = [self.collectionViewModel
        sectionForSectionIdentifier:SectionIdentifierRead];
  }
  if ([self hasItemInSection:SectionIdentifierUnread]) {
    sectionUnread = [self.collectionViewModel
        sectionForSectionIdentifier:SectionIdentifierUnread];
  }

  // Check selected sections.
  for (NSIndexPath* index in self.collectionView.indexPathsForSelectedItems) {
    if (index.section == sectionRead) {
      readSelected = YES;
    } else if (index.section == sectionUnread) {
      unreadSelected = YES;
    }
  }

  // Update toolbar state.
  if (readSelected) {
    if (unreadSelected) {
      // Read and Unread selected.
      self.toolbarState = MixedItemsSelected;
    } else {
      // Read selected.
      self.toolbarState = OnlyReadSelected;
    }
  } else if (unreadSelected) {
    // Unread selected.
    self.toolbarState = OnlyUnreadSelected;
  }
}

- (void)markAllItemsAs {
  [self initializeActionSheet];
  __weak ReadingListCollectionViewController* weakSelf = self;
  [_actionSheet addItemWithTitle:l10n_util::GetNSStringWithFixup(
                                     IDS_IOS_READING_LIST_MARK_ALL_READ_ACTION)
                          action:^{
                            [weakSelf markAllRead];
                          }
                           style:UIAlertActionStyleDefault];
  [_actionSheet
      addItemWithTitle:l10n_util::GetNSStringWithFixup(
                           IDS_IOS_READING_LIST_MARK_ALL_UNREAD_ACTION)
                action:^{
                  [weakSelf markAllUnread];
                }
                 style:UIAlertActionStyleDefault];
  [_actionSheet start];
}

- (void)markMixedItemsAs {
  [self initializeActionSheet];
  __weak ReadingListCollectionViewController* weakSelf = self;
  [_actionSheet addItemWithTitle:l10n_util::GetNSStringWithFixup(
                                     IDS_IOS_READING_LIST_MARK_READ_BUTTON)
                          action:^{
                            [weakSelf markItemsRead];
                          }
                           style:UIAlertActionStyleDefault];
  [_actionSheet addItemWithTitle:l10n_util::GetNSStringWithFixup(
                                     IDS_IOS_READING_LIST_MARK_UNREAD_BUTTON)
                          action:^{
                            [weakSelf markItemsUnread];
                          }
                           style:UIAlertActionStyleDefault];
  [_actionSheet start];
}

- (void)initializeActionSheet {
  _actionSheet = [_toolbar actionSheetForMarkWithBaseViewController:self];

  [_actionSheet addItemWithTitle:l10n_util::GetNSStringWithFixup(IDS_CANCEL)
                          action:nil
                           style:UIAlertActionStyleCancel];
}

- (void)markAllRead {
  if (![self.editor isEditing]) {
    return;
  }
  if (![self hasItemInSection:SectionIdentifierUnread]) {
    [self exitEditingModeAnimated:YES];
    return;
  }

  [self updateItemsInSectionIdentifier:SectionIdentifierUnread
                     usingEntryUpdater:^(const GURL& url) {
                       [self readingListModel]->SetReadStatus(url, true);
                     }];
  [self exitEditingModeAnimated:YES];
  [self moveItemsFromSection:SectionIdentifierUnread
                   toSection:SectionIdentifierRead];
}

- (void)markAllUnread {
  if (![self hasItemInSection:SectionIdentifierRead]) {
    [self exitEditingModeAnimated:YES];
    return;
  }

  [self updateItemsInSectionIdentifier:SectionIdentifierRead
                     usingEntryUpdater:^(const GURL& url) {
                       [self readingListModel]->SetReadStatus(url, false);
                     }];
  [self exitEditingModeAnimated:YES];
  [self moveItemsFromSection:SectionIdentifierRead
                   toSection:SectionIdentifierUnread];
}

- (void)markItemsRead {
  base::RecordAction(base::UserMetricsAction("MobileReadingListMarkRead"));
  NSArray* sortedIndexPaths = [self.collectionView.indexPathsForSelectedItems
      sortedArrayUsingSelector:@selector(compare:)];
  [self updateIndexPaths:sortedIndexPaths
       usingEntryUpdater:^(const GURL& url) {
         [self readingListModel]->SetReadStatus(url, true);
       }];

  [self exitEditingModeAnimated:YES];
  [self moveSelectedItems:sortedIndexPaths toSection:SectionIdentifierRead];
}

- (void)markItemsUnread {
  base::RecordAction(base::UserMetricsAction("MobileReadingListMarkUnread"));
  NSArray* sortedIndexPaths = [self.collectionView.indexPathsForSelectedItems
      sortedArrayUsingSelector:@selector(compare:)];
  [self updateIndexPaths:sortedIndexPaths
       usingEntryUpdater:^(const GURL& url) {
         [self readingListModel]->SetReadStatus(url, false);
       }];

  [self exitEditingModeAnimated:YES];
  [self moveSelectedItems:sortedIndexPaths toSection:SectionIdentifierUnread];
}

- (void)deleteAllReadItems {
  base::RecordAction(base::UserMetricsAction("MobileReadingListDeleteRead"));
  if (![self hasItemInSection:SectionIdentifierRead]) {
    [self exitEditingModeAnimated:YES];
    return;
  }

  [self updateItemsInSectionIdentifier:SectionIdentifierRead
                     usingEntryUpdater:^(const GURL& url) {
                       [self logDeletionHistogramsForEntry:url];
                       [self readingListModel]->RemoveEntryByURL(url);
                     }];

  [self exitEditingModeAnimated:YES];
  [self.collectionView performBatchUpdates:^{
    NSInteger readSection = [self.collectionViewModel
        sectionForSectionIdentifier:SectionIdentifierRead];
    [self.collectionView
        deleteSections:[NSIndexSet indexSetWithIndex:readSection]];
    [self.collectionViewModel
        removeSectionWithIdentifier:SectionIdentifierRead];
  }
      completion:^(BOOL) {
        // Reload data to take into account possible sync events.
        [self applyPendingUpdates];
      }];
  // As we modified the section in the batch update block, remove the section in
  // another block.
  [self removeEmptySections];
}

- (void)deleteSelectedItems {
  NSArray* indexPaths = [self.collectionView.indexPathsForSelectedItems copy];
  [self updateIndexPaths:indexPaths
       usingEntryUpdater:^(const GURL& url) {
         [self logDeletionHistogramsForEntry:url];
         [self readingListModel]->RemoveEntryByURL(url);
       }];

  [self exitEditingModeAnimated:YES];

  [self.collectionView performBatchUpdates:^{
    [self collectionView:self.collectionView
        willDeleteItemsAtIndexPaths:indexPaths];

    [self.collectionView deleteItemsAtIndexPaths:indexPaths];
  }
      completion:^(BOOL) {
        // Reload data to take into account possible sync events.
        [self applyPendingUpdates];
      }];
  // As we modified the section in the batch update block, remove the section in
  // another block.
  [self removeEmptySections];
}

- (void)updateItemsInSectionIdentifier:(SectionIdentifier)identifier
                     usingEntryUpdater:(EntryUpdater)updater {
  self.shouldMonitorModel = NO;
  auto token = self.readingListModel->BeginBatchUpdates();
  NSArray* readItems =
      [self.collectionViewModel itemsInSectionWithIdentifier:identifier];
  // Read the objects in reverse order to keep the order (last modified first).
  for (id item in [readItems reverseObjectEnumerator]) {
    ReadingListCollectionViewItem* readingListItem =
        base::mac::ObjCCastStrict<ReadingListCollectionViewItem>(item);
    if (updater)
      updater(readingListItem.url);
  }
  token.reset();
  self.shouldMonitorModel = YES;
}

- (void)updateIndexPaths:(NSArray<NSIndexPath*>*)indexPaths
       usingEntryUpdater:(EntryUpdater)updater {
  self.shouldMonitorModel = NO;
  auto token = self.readingListModel->BeginBatchUpdates();
  // Read the objects in reverse order to keep the order (last modified first).
  for (NSIndexPath* index in [indexPaths reverseObjectEnumerator]) {
    CollectionViewItem* cell = [self.collectionViewModel itemAtIndexPath:index];
    ReadingListCollectionViewItem* readingListItem =
        base::mac::ObjCCastStrict<ReadingListCollectionViewItem>(cell);
    if (updater)
      updater(readingListItem.url);
  }
  // Leave the batch update while it is not monitored.
  token.reset();
  self.shouldMonitorModel = YES;
}

- (void)logDeletionHistogramsForEntry:(const GURL&)url {
  const ReadingListEntry* entry = [self readingListModel]->GetEntryByURL(url);

  if (!entry)
    return;

  int64_t firstRead = entry->FirstReadTime();
  if (firstRead > 0) {
    // Log 0 if the entry has never been read.
    firstRead = (base::Time::Now() - base::Time::UnixEpoch()).InMicroseconds() -
                firstRead;
    // Convert it to hours.
    firstRead = firstRead / base::Time::kMicrosecondsPerHour;
  }
  UMA_HISTOGRAM_COUNTS_10000("ReadingList.FirstReadAgeOnDeletion", firstRead);

  int64_t age = (base::Time::Now() - base::Time::UnixEpoch()).InMicroseconds() -
                entry->CreationTime();
  // Convert it to hours.
  age = age / base::Time::kMicrosecondsPerHour;
  if (entry->IsRead())
    UMA_HISTOGRAM_COUNTS_10000("ReadingList.Read.AgeOnDeletion", age);
  else
    UMA_HISTOGRAM_COUNTS_10000("ReadingList.Unread.AgeOnDeletion", age);
}

- (void)moveItemsFromSection:(SectionIdentifier)sourceSectionIdentifier
                   toSection:(SectionIdentifier)destinationSectionIdentifier {
  [self initializeSection:destinationSectionIdentifier];

  NSInteger sourceSection = [self.collectionViewModel
      sectionForSectionIdentifier:sourceSectionIdentifier];
  NSInteger destinationSection = [self.collectionViewModel
      sectionForSectionIdentifier:destinationSectionIdentifier];
  NSInteger numberOfSourceItems =
      [self.collectionViewModel numberOfItemsInSection:sourceSection];

  [self.collectionView performBatchUpdates:^{
    for (int index = 0; index < numberOfSourceItems; index++) {
      NSIndexPath* firstItemIndex =
          [NSIndexPath indexPathForItem:0 inSection:sourceSection];
      NSIndexPath* sourceItemIndex =
          [NSIndexPath indexPathForItem:index inSection:sourceSection];
      NSIndexPath* destinationItemIndex =
          [NSIndexPath indexPathForItem:index inSection:destinationSection];

      // The collection view model gets updated instantaneously, the collection
      // view does batch updates.
      [self collectionView:self.collectionView
          willMoveItemAtIndexPath:firstItemIndex
                      toIndexPath:destinationItemIndex];
      [self.collectionView moveItemAtIndexPath:sourceItemIndex
                                   toIndexPath:destinationItemIndex];
    }
  }
      completion:^(BOOL) {
        // Reload data to take into account possible sync events.
        [self applyPendingUpdates];
      }];
  // As we modified the section in the batch update block, remove the section in
  // another block.
  [self removeEmptySections];
}

- (void)moveSelectedItems:(NSArray*)sortedIndexPaths
                toSection:(SectionIdentifier)sectionIdentifier {
  NSInteger sectionCreatedIndex = [self initializeSection:sectionIdentifier];

  [self.collectionView performBatchUpdates:^{
    NSInteger section = [self.collectionViewModel
        sectionForSectionIdentifier:sectionIdentifier];

    NSInteger newItemIndex = 0;
    for (NSIndexPath* index in sortedIndexPaths) {
      // The |sortedIndexPaths| is a copy of the index paths before the
      // destination section has been added if necessary. The section part of
      // the index potentially needs to be updated.
      NSInteger updatedSection = index.section;
      if (updatedSection >= sectionCreatedIndex)
        updatedSection++;
      if (updatedSection == section) {
        // The item is already in the targeted section, there is no need to move
        // it.
        continue;
      }

      NSIndexPath* updatedIndex =
          [NSIndexPath indexPathForItem:index.item inSection:updatedSection];
      NSIndexPath* indexForModel =
          [NSIndexPath indexPathForItem:index.item - newItemIndex
                              inSection:updatedSection];

      // Index of the item in the new section. The newItemIndex is the index of
      // this item in the targeted section.
      NSIndexPath* newIndexPath =
          [NSIndexPath indexPathForItem:newItemIndex++ inSection:section];
      [self collectionView:self.collectionView
          willMoveItemAtIndexPath:indexForModel
                      toIndexPath:newIndexPath];
      [self.collectionView moveItemAtIndexPath:updatedIndex
                                   toIndexPath:newIndexPath];
    }
  }
      completion:^(BOOL) {
        // Reload data to take into account possible sync events.
        [self applyPendingUpdates];
      }];
  // As we modified the section in the batch update block, remove the section in
  // another block.
  [self removeEmptySections];
}

- (NSInteger)initializeSection:(SectionIdentifier)sectionIdentifier {
  if (![self.collectionViewModel
          hasSectionForSectionIdentifier:sectionIdentifier]) {
    // The new section IndexPath will be 1 if it is the read section with
    // items in the unread section, 0 otherwise.
    BOOL hasNonEmptySectionAbove =
        sectionIdentifier == SectionIdentifierRead &&
        [self hasItemInSection:SectionIdentifierUnread];
    NSInteger sectionIndex = hasNonEmptySectionAbove ? 1 : 0;

    [self.collectionView performBatchUpdates:^{
      [self.collectionViewModel insertSectionWithIdentifier:sectionIdentifier
                                                    atIndex:sectionIndex];

      [self.collectionViewModel
                         setHeader:[self headerForSection:sectionIdentifier]
          forSectionWithIdentifier:sectionIdentifier];

      [self.collectionView
          insertSections:[NSIndexSet indexSetWithIndex:sectionIndex]];
    }
                                  completion:nil];

    return sectionIndex;
  }
  return NSIntegerMax;
}

- (CollectionViewTextItem*)headerForSection:
    (SectionIdentifier)sectionIdentifier {
  CollectionViewTextItem* header = nil;

  switch (sectionIdentifier) {
    case SectionIdentifierRead:
      header = [[CollectionViewTextItem alloc] initWithType:ItemTypeReadHeader];
      header.text = l10n_util::GetNSString(IDS_IOS_READING_LIST_READ_HEADER);
      break;

    case SectionIdentifierUnread:
      header =
          [[CollectionViewTextItem alloc] initWithType:ItemTypeUnreadHeader];
      header.text = l10n_util::GetNSString(IDS_IOS_READING_LIST_UNREAD_HEADER);
      break;
  }
  return header;
}

- (void)removeEmptySections {
  [self.collectionView performBatchUpdates:^{

    SectionIdentifier a[] = {SectionIdentifierRead, SectionIdentifierUnread};
    for (size_t i = 0; i < arraysize(a); i++) {
      SectionIdentifier sectionIdentifier = a[i];

      if ([self.collectionViewModel
              hasSectionForSectionIdentifier:sectionIdentifier] &&
          ![self hasItemInSection:sectionIdentifier]) {
        NSInteger section = [self.collectionViewModel
            sectionForSectionIdentifier:sectionIdentifier];

        [self.collectionView
            deleteSections:[NSIndexSet indexSetWithIndex:section]];
        [self.collectionViewModel
            removeSectionWithIdentifier:sectionIdentifier];
      }
    }
  }
                                completion:nil];
  if (_readingListModel->size() == 0) {
    [self collectionIsEmpty];
  } else {
    [_toolbar setHasReadItem:_readingListModel->unread_size() !=
                             _readingListModel->size()];
  }
}

- (void)exitEditingModeAnimated:(BOOL)animated {
  [_actionSheet stop];
  [self.editor setEditing:NO animated:animated];
  [_toolbar setEditing:NO];
}

@end
