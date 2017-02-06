// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/reading_list/reading_list_view_controller_container.h"

#import <MobileCoreServices/MobileCoreServices.h>

#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "components/reading_list/ios/reading_list_model.h"
#include "ios/chrome/browser/reading_list/offline_url_utils.h"
#import "ios/chrome/browser/ui/alert_coordinator/action_sheet_coordinator.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_item.h"
#import "ios/chrome/browser/ui/keyboard/UIKeyCommand+Chrome.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_collection_view_item.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_toolbar.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_view_controller.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"
#import "ios/chrome/browser/ui/url_loader.h"
#import "ios/chrome/browser/ui/util/pasteboard_util.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ios/web/public/navigation_manager.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/strings/grit/ui_strings.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Action chosen by the user in the context menu, for UMA report.
// These match tools/metrics/histograms/histograms.xml.
enum UMAContextMenuAction {
  // The user opened the entry in a new tab.
  NEW_TAB = 0,
  // The user opened the entry in a new incognito tab.
  NEW_INCOGNITO_TAB = 1,
  // The user copied the url of the entry.
  COPY_LINK = 2,
  // The user chose to view the offline version of the entry.
  VIEW_OFFLINE = 3,
  // The user cancelled the context menu.
  CANCEL = 4,
  // Add new enum above ENUM_MAX.
  ENUM_MAX
};

// Height of the toolbar in normal state.
const int kToolbarNormalHeight = 48;
// Height of the expanded toolbar (buttons on multiple lines).
const int kToolbarExpandedHeight = 58;

typedef NS_ENUM(NSInteger, LayoutPriority) {
  LayoutPriorityLow = 750,
  LayoutPriorityHigh = 751
};
}

@interface ReadingListViewControllerContainer ()<
    ReadingListToolbarActions,
    ReadingListToolbarHeightDelegate> {
  // Toolbar with the actions.
  ReadingListToolbar* _toolbar;
  // This constraint control the expanded mode of the toolbar.
  NSLayoutConstraint* _expandedToolbarConstraint;
  // Coordinator for the alert displayed when the user long presses an entry.
  AlertCoordinator* _alertCoordinator;
}

// UrlLoader for navigating to entries.
@property(nonatomic, weak, readonly) id<UrlLoader> URLLoader;
@property(nonatomic, strong, readonly)
    ReadingListViewController* readingListViewController;

// Closes the ReadingList view.
- (void)dismiss;
// Opens |URL| in a new tab |incognito| or not.
- (void)openNewTabWithURL:(const GURL&)URL incognito:(BOOL)incognito;
// Opens the offline url |offlineURL| of the entry saved in the reading list
// model with the |entryURL| url.
- (void)openOfflineURL:(const GURL&)offlineURL
            correspondingEntryURL:(const GURL&)entryURL
    fromReadingListViewController:
        (ReadingListViewController*)readingListViewController;

@end

@implementation ReadingListViewControllerContainer

@synthesize readingListViewController = _readingListViewController;
@synthesize URLLoader = _URLLoader;

- (instancetype)initWithModel:(ReadingListModel*)model
                        loader:(id<UrlLoader>)loader
              largeIconService:(favicon::LargeIconService*)largeIconService
    readingListDownloadService:
        (ReadingListDownloadService*)readingListDownloadService {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _URLLoader = loader;
    _toolbar = [[ReadingListToolbar alloc] initWithFrame:CGRectZero];
    _toolbar.heightDelegate = self;
    _readingListViewController = [[ReadingListViewController alloc]
                     initWithModel:model
                  largeIconService:largeIconService
        readingListDownloadService:readingListDownloadService
                           toolbar:_toolbar];
    _readingListViewController.delegate = self;

    // Configure modal presentation.
    [self setModalPresentationStyle:UIModalPresentationFormSheet];
    [self setModalTransitionStyle:UIModalTransitionStyleCoverVertical];
  }
  return self;
}

- (void)viewDidLoad {
  [self addChildViewController:self.readingListViewController];
  [self.view addSubview:self.readingListViewController.view];
  [self.readingListViewController didMoveToParentViewController:self];

  [_toolbar setTranslatesAutoresizingMaskIntoConstraints:NO];
  [self.readingListViewController.view
      setTranslatesAutoresizingMaskIntoConstraints:NO];

  NSDictionary* views =
      @{ @"collection" : self.readingListViewController.view };
  NSArray* constraints = @[ @"V:|[collection]", @"H:|[collection]|" ];
  ApplyVisualConstraints(constraints, views);

  // This constraint has a low priority so it will only take effect when the
  // toolbar isn't present, allowing the collection to take the whole page.
  NSLayoutConstraint* constraint =
      [self.readingListViewController.view.bottomAnchor
          constraintEqualToAnchor:self.view.bottomAnchor];
  constraint.priority = LayoutPriorityLow;
  constraint.active = YES;
}

- (BOOL)prefersStatusBarHidden {
  return NO;
}

#pragma mark UIAccessibilityAction

- (BOOL)accessibilityPerformEscape {
  [self dismiss];
  return YES;
}

#pragma mark - ReadingListViewControllerDelegate

- (void)readingListViewController:
            (ReadingListViewController*)readingListViewController
                         hasItems:(BOOL)hasItems {
  if (hasItems) {
    // If there are items, add the toolbar.
    [self.view addSubview:_toolbar];
    NSDictionary* views = @{
      @"toolbar" : _toolbar,
      @"collection" : readingListViewController.view
    };
    NSArray* constraints = @[ @"V:[collection][toolbar]|", @"H:|[toolbar]|" ];
    ApplyVisualConstraints(constraints, views);
    NSLayoutConstraint* height =
        [_toolbar.heightAnchor constraintEqualToConstant:kToolbarNormalHeight];
    height.priority = LayoutPriorityHigh;
    height.active = YES;
    // When the toolbar is added, the only button is the "edit" button. No need
    // to go in expanded mode.
    _expandedToolbarConstraint = [_toolbar.heightAnchor
        constraintEqualToConstant:kToolbarExpandedHeight];
  } else {
    // If there is no item, remove the toolbar. The constraints will make sure
    // the collection takes the whole view.
    [_toolbar removeFromSuperview];
  }
}

- (void)dismissReadingListViewController:
    (ReadingListViewController*)readingListViewController {
  [readingListViewController willBeDismissed];
  [self dismiss];
}

- (void)
readingListViewController:(ReadingListViewController*)readingListViewController
displayContextMenuForItem:(ReadingListCollectionViewItem*)readingListItem
                  atPoint:(CGPoint)menuLocation {
  const ReadingListEntry* entry =
      readingListViewController.readingListModel->GetEntryByURL(
          readingListItem.url);

  if (!entry) {
    [readingListViewController reloadData];
    return;
  }
  const GURL entryURL = entry->URL();

  __weak ReadingListViewControllerContainer* weakSelf = self;

  _alertCoordinator = [[ActionSheetCoordinator alloc]
      initWithBaseViewController:self
                           title:readingListItem.text
                         message:readingListItem.detailText
                            rect:CGRectMake(menuLocation.x, menuLocation.y, 0,
                                            0)
                            view:readingListViewController.collectionView];

  NSString* openInNewTabTitle =
      l10n_util::GetNSString(IDS_IOS_CONTENT_CONTEXT_OPENLINKNEWTAB);
  [_alertCoordinator
      addItemWithTitle:openInNewTabTitle
                action:^{
                  [weakSelf openNewTabWithURL:entryURL incognito:NO];
                  UMA_HISTOGRAM_ENUMERATION("ReadingList.ContextMenu", NEW_TAB,
                                            ENUM_MAX);

                }
                 style:UIAlertActionStyleDefault];

  NSString* openInNewTabIncognitoTitle =
      l10n_util::GetNSString(IDS_IOS_CONTENT_CONTEXT_OPENLINKNEWINCOGNITOTAB);
  [_alertCoordinator
      addItemWithTitle:openInNewTabIncognitoTitle
                action:^{
                  UMA_HISTOGRAM_ENUMERATION("ReadingList.ContextMenu",
                                            NEW_INCOGNITO_TAB, ENUM_MAX);
                  [weakSelf openNewTabWithURL:entryURL incognito:YES];
                }
                 style:UIAlertActionStyleDefault];

  NSString* copyLinkTitle =
      l10n_util::GetNSString(IDS_IOS_CONTENT_CONTEXT_COPY);
  [_alertCoordinator
      addItemWithTitle:copyLinkTitle
                action:^{
                  UMA_HISTOGRAM_ENUMERATION("ReadingList.ContextMenu",
                                            COPY_LINK, ENUM_MAX);
                  StoreURLInPasteboard(entryURL);
                }
                 style:UIAlertActionStyleDefault];

  if (entry->DistilledState() == ReadingListEntry::PROCESSED) {
    GURL offlineURL = reading_list::OfflineURLForPath(
        entry->DistilledPath(), entryURL, entry->DistilledURL());
    NSString* viewOfflineVersionTitle =
        l10n_util::GetNSString(IDS_IOS_READING_LIST_CONTENT_CONTEXT_OFFLINE);
    [_alertCoordinator
        addItemWithTitle:viewOfflineVersionTitle
                  action:^{
                    UMA_HISTOGRAM_ENUMERATION("ReadingList.ContextMenu",
                                              VIEW_OFFLINE, ENUM_MAX);
                    [weakSelf openOfflineURL:offlineURL
                                correspondingEntryURL:entryURL
                        fromReadingListViewController:
                            readingListViewController];
                  }
                   style:UIAlertActionStyleDefault];
  }

  [_alertCoordinator
      addItemWithTitle:l10n_util::GetNSString(IDS_APP_CANCEL)
                action:^{
                  UMA_HISTOGRAM_ENUMERATION("ReadingList.ContextMenu", CANCEL,
                                            ENUM_MAX);
                }
                 style:UIAlertActionStyleCancel];

  [_alertCoordinator start];
}

- (void)
readingListViewController:(ReadingListViewController*)readingListViewController
                 openItem:(ReadingListCollectionViewItem*)readingListItem {
  const ReadingListEntry* entry =
      readingListViewController.readingListModel->GetEntryByURL(
          readingListItem.url);

  if (!entry) {
    [readingListViewController reloadData];
    return;
  }

  base::RecordAction(base::UserMetricsAction("MobileReadingListOpen"));

  [self.URLLoader loadURL:entry->URL()
                 referrer:web::Referrer()
               transition:ui::PAGE_TRANSITION_AUTO_BOOKMARK
        rendererInitiated:NO];

  [self dismiss];
}

#pragma mark - ReadingListToolbarActionTarget

- (void)markPressed {
  [self.readingListViewController markPressed];
}

- (void)deletePressed {
  [self.readingListViewController deletePressed];
}

- (void)enterEditingModePressed {
  [self.readingListViewController enterEditingModePressed];
}

- (void)exitEditingModePressed {
  [self.readingListViewController exitEditingModePressed];
}

#pragma mark - ReadingListToolbarHeightDelegate

- (void)toolbar:(id)toolbar onHeightChanged:(ReadingListToolbarHeight)height {
  dispatch_async(dispatch_get_main_queue(), ^{
    switch (height) {
      case NormalHeight:
        _expandedToolbarConstraint.active = NO;
        break;

      case ExpandedHeight:
        _expandedToolbarConstraint.active = YES;
        break;
    }
  });
}

#pragma mark - Private

- (void)dismiss {
  [self.presentingViewController dismissViewControllerAnimated:YES
                                                    completion:nil];
}

- (void)openNewTabWithURL:(const GURL&)URL incognito:(BOOL)incognito {
  base::RecordAction(base::UserMetricsAction("MobileReadingListOpen"));

  [self.URLLoader webPageOrderedOpen:URL
                            referrer:web::Referrer()
                          windowName:nil
                         inIncognito:incognito
                        inBackground:NO
                            appendTo:kLastTab];

  [self dismiss];
}

- (void)openOfflineURL:(const GURL&)offlineURL
            correspondingEntryURL:(const GURL&)entryURL
    fromReadingListViewController:
        (ReadingListViewController*)readingListViewController {
  [readingListViewController willBeDismissed];

  [self openNewTabWithURL:offlineURL incognito:NO];

  UMA_HISTOGRAM_BOOLEAN("ReadingList.OfflineVersionDisplayed", true);
  const GURL updateURL = entryURL;
  readingListViewController.readingListModel->SetReadStatus(updateURL, true);
}

#pragma mark - UIResponder

- (NSArray*)keyCommands {
  __weak ReadingListViewControllerContainer* weakSelf = self;
  return @[ [UIKeyCommand cr_keyCommandWithInput:UIKeyInputEscape
                                   modifierFlags:Cr_UIKeyModifierNone
                                           title:nil
                                          action:^{
                                            [weakSelf dismiss];
                                          }] ];
}

@end
