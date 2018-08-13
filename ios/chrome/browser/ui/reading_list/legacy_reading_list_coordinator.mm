// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/reading_list/legacy_reading_list_coordinator.h"

#import "base/mac/foundation_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#import "base/strings/sys_string_conversions.h"
#include "components/feature_engagement/public/event_constants.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/reading_list/core/reading_list_model.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#include "ios/chrome/browser/favicon/ios_chrome_large_icon_service_factory.h"
#include "ios/chrome/browser/feature_engagement/tracker_factory.h"
#import "ios/chrome/browser/metrics/new_tab_page_uma.h"
#include "ios/chrome/browser/reading_list/offline_url_utils.h"
#include "ios/chrome/browser/reading_list/reading_list_download_service.h"
#include "ios/chrome/browser/reading_list/reading_list_download_service_factory.h"
#include "ios/chrome/browser/reading_list/reading_list_model_factory.h"
#import "ios/chrome/browser/ui/commands/open_new_tab_command.h"
#import "ios/chrome/browser/ui/reading_list/context_menu/reading_list_context_menu_commands.h"
#import "ios/chrome/browser/ui/reading_list/context_menu/reading_list_context_menu_coordinator.h"
#import "ios/chrome/browser/ui/reading_list/context_menu/reading_list_context_menu_params.h"
#import "ios/chrome/browser/ui/reading_list/legacy_reading_list_toolbar.h"
#import "ios/chrome/browser/ui/reading_list/legacy_reading_list_view_controller.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_collection_view_controller.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_collection_view_item.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_list_item_factory.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_mediator.h"
#import "ios/chrome/browser/ui/url_loader.h"
#import "ios/chrome/browser/ui/util/pasteboard_util.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/navigation_manager.h"
#include "ios/web/public/referrer.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/strings/grit/ui_strings.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface LegacyReadingListCoordinator ()<ReadingListContextMenuCommands>

@property(nonatomic, assign) ios::ChromeBrowserState* browserState;
// Used to load the Reading List pages.
@property(nonatomic, weak) id<UrlLoader> URLLoader;
// The container view containing both the collection view and the toolbar.
@property(nonatomic, strong)
    LegacyReadingListViewController* containerViewController;
// The collection view controller that displays the reading list items (owned by
// |containerViewController|).
@property(nonatomic, weak)
    ReadingListCollectionViewController* collectionViewController;
// The context menu displayed for long-presses on reading list items.
@property(nonatomic, strong)
    ReadingListContextMenuCoordinator* contextMenuCoordinator;

@end

@implementation LegacyReadingListCoordinator

@synthesize containerViewController = _containerViewController;
@synthesize collectionViewController = _collectionViewController;
@synthesize contextMenuCoordinator = _contextMenuCoordinator;
@synthesize URLLoader = _URLLoader;
@synthesize browserState = _browserState;
@synthesize mediator = _mediator;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                              browserState:
                                  (ios::ChromeBrowserState*)browserState
                                    loader:(id<UrlLoader>)loader {
  self = [super initWithBaseViewController:viewController
                              browserState:browserState];
  if (self) {
    _browserState = browserState;
    _URLLoader = loader;
  }
  return self;
}

#pragma mark - ChromeCoordinator

- (void)start {
  if (!self.containerViewController) {
    ReadingListModel* model =
        ReadingListModelFactory::GetInstance()->GetForBrowserState(
            self.browserState);
    favicon::LargeIconService* largeIconService =
        IOSChromeLargeIconServiceFactory::GetForBrowserState(self.browserState);

    ReadingListListItemFactory* itemFactory =
        [ReadingListListItemFactory collectionViewItemFactory];

    self.mediator = [[ReadingListMediator alloc] initWithModel:model
                                              largeIconService:largeIconService
                                               listItemFactory:itemFactory];
    LegacyReadingListToolbar* toolbar = [[LegacyReadingListToolbar alloc] init];
    ReadingListCollectionViewController* collectionViewController =
        [[ReadingListCollectionViewController alloc]
            initWithDataSource:self.mediator

                       toolbar:toolbar];
    collectionViewController.delegate = self;
    itemFactory.accessibilityDelegate = collectionViewController;
    self.collectionViewController = collectionViewController;

    self.containerViewController = [[LegacyReadingListViewController alloc]
        initWithCollectionViewController:collectionViewController
                                 toolbar:toolbar];
    self.containerViewController.delegate = self;
  }

  [self.baseViewController presentViewController:self.containerViewController
                                        animated:YES
                                      completion:nil];

  // Send the "Viewed Reading List" event to the feature_engagement::Tracker
  // when the user opens their reading list.
  feature_engagement::TrackerFactory::GetForBrowserState(self.browserState)
      ->NotifyEvent(feature_engagement::events::kViewedReadingList);
}

- (void)stop {
  [self.containerViewController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:nil];

  self.containerViewController = nil;
}

#pragma mark - ReadingListListViewControllerDelegate

- (void)dismissReadingListListViewController:(UIViewController*)viewController {
  DCHECK_EQ(viewController, self.collectionViewController);
  [self.collectionViewController willBeDismissed];
  [self stop];
}

- (void)readingListListViewController:(UIViewController*)viewController
            displayContextMenuForItem:(ListItem<ReadingListListItem>*)item
                              atPoint:(CGPoint)menuLocation {
  DCHECK_EQ(viewController, self.collectionViewController);
  if (!self.containerViewController) {
    return;
  }

  ListItem<ReadingListListItem>* readingListItem =
      base::mac::ObjCCastStrict<ReadingListCollectionViewItem>(item);

  const ReadingListEntry* entry = [self.mediator entryFromItem:readingListItem];

  if (!entry) {
    [self.collectionViewController reloadData];
    return;
  }
  const GURL entryURL = entry->URL();

  GURL offlineURL;
  if (entry->DistilledState() == ReadingListEntry::PROCESSED) {
    offlineURL = reading_list::OfflineURLForPath(
        entry->DistilledPath(), entryURL, entry->DistilledURL());
  }

  ReadingListContextMenuParams* params =
      [[ReadingListContextMenuParams alloc] init];
  params.title = readingListItem.title;
  params.message = base::SysUTF8ToNSString(readingListItem.entryURL.host());
  params.rect = CGRectMake(menuLocation.x, menuLocation.y, 0, 0);
  params.view = self.collectionViewController.collectionView;
  params.entryURL = entryURL;
  params.offlineURL = offlineURL;

  self.contextMenuCoordinator = [[ReadingListContextMenuCoordinator alloc]
      initWithBaseViewController:self.containerViewController
                          params:params];
  self.contextMenuCoordinator.commandHandler = self;
  [self.contextMenuCoordinator start];
}

- (void)readingListListViewController:(UIViewController*)viewController
                             openItem:(ListItem<ReadingListListItem>*)item {
  const ReadingListEntry* entry = [self.mediator entryFromItem:item];
  if (!entry) {
    [self.collectionViewController reloadData];
    return;
  }
  [self loadEntryURL:entry->URL()
      withOfflineURL:GURL::EmptyGURL()
            inNewTab:NO
           incognito:NO];
}

- (void)readingListListViewController:(UIViewController*)viewController
                     openItemInNewTab:(ListItem<ReadingListListItem>*)item
                            incognito:(BOOL)incognito {
  const ReadingListEntry* entry = [self.mediator entryFromItem:item];
  if (!entry) {
    [self.collectionViewController reloadData];
    return;
  }
  [self loadEntryURL:entry->URL()
      withOfflineURL:GURL::EmptyGURL()
            inNewTab:YES
           incognito:incognito];
}

- (void)readingListListViewController:(UIViewController*)viewController
              openItemOfflineInNewTab:(id<ReadingListListItem>)item {
  const ReadingListEntry* entry = [self.mediator entryFromItem:item];
  if (!entry)
    return;

  if (entry->DistilledState() == ReadingListEntry::PROCESSED) {
    const GURL entryURL = entry->URL();
    GURL offlineURL = reading_list::OfflineURLForPath(
        entry->DistilledPath(), entryURL, entry->DistilledURL());
    [self loadEntryURL:entry->URL()
        withOfflineURL:offlineURL
              inNewTab:YES
             incognito:NO];
  }
}

#pragma mark - ReadingListContextMenuCommands

- (void)openURLInNewTabForContextMenuWithParams:
    (ReadingListContextMenuParams*)params {
  [self loadEntryURL:params.entryURL
      withOfflineURL:GURL::EmptyGURL()
            inNewTab:YES
           incognito:NO];
  [self cleanUpContextMenu];
}

- (void)openURLInNewIncognitoTabForContextMenuWithParams:
    (ReadingListContextMenuParams*)params {
  [self loadEntryURL:params.entryURL
      withOfflineURL:GURL::EmptyGURL()
            inNewTab:YES
           incognito:YES];
  [self cleanUpContextMenu];
}

- (void)copyURLForContextMenuWithParams:(ReadingListContextMenuParams*)params {
  StoreURLInPasteboard(params.entryURL);
  [self cleanUpContextMenu];
}

- (void)openOfflineURLInNewTabForContextMenuWithParams:
    (ReadingListContextMenuParams*)params {
  [self loadEntryURL:params.entryURL
      withOfflineURL:params.offlineURL
            inNewTab:YES
           incognito:NO];
  [self cleanUpContextMenu];
}

- (void)cancelReadingListContextMenuWithParams:
    (ReadingListContextMenuParams*)params {
  [self cleanUpContextMenu];
}

#pragma mark - Context Menu Helpers

// Stops the context menu coordinator and resets the property.
- (void)cleanUpContextMenu {
  [self.contextMenuCoordinator stop];
  self.contextMenuCoordinator = nil;
}

#pragma mark - Private

// Loads reading list URLs.  If |offlineURL| is valid, the item will be loaded
// offline; otherwise |entryURL| is loaded.  |newTab| and |incognito| can be
// used to optionally open the URL in a new tab or in incognito.  The
// coordinator is also stopped after the load is requested.
- (void)loadEntryURL:(const GURL&)entryURL
      withOfflineURL:(const GURL&)offlineURL
            inNewTab:(BOOL)newTab
           incognito:(BOOL)incognito {
  DCHECK(entryURL.is_valid());
  base::RecordAction(base::UserMetricsAction("MobileReadingListOpen"));
  new_tab_page_uma::RecordAction(
      self.browserState, new_tab_page_uma::ACTION_OPENED_READING_LIST_ENTRY);

  // Load the offline URL if available.
  GURL loadURL = entryURL;
  if (offlineURL.is_valid()) {
    loadURL = offlineURL;
    // Offline URLs should always be opened in new tabs.
    newTab = YES;
    // Record the offline load and update the model.
    UMA_HISTOGRAM_BOOLEAN("ReadingList.OfflineVersionDisplayed", true);
    const GURL updateURL = entryURL;
    [self.mediator markEntryRead:updateURL];
  }

  // Prepare the collection for dismissal.
  [self.collectionViewController willBeDismissed];

  if (newTab) {
    // Use a referrer with a specific URL to signal that this entry should not
    // be taken into account for the Most Visited tiles.
    web::Referrer referrer = web::Referrer(GURL(kReadingListReferrerURL),
                                           web::ReferrerPolicyDefault);
    OpenNewTabCommand* command =
        [[OpenNewTabCommand alloc] initWithURL:loadURL
                                      referrer:referrer
                                   inIncognito:incognito
                                  inBackground:NO
                                      appendTo:kLastTab];
    [self.URLLoader webPageOrderedOpen:command];
  } else {
    web::NavigationManager::WebLoadParams params(loadURL);
    params.transition_type = ui::PAGE_TRANSITION_AUTO_BOOKMARK;
    params.referrer = web::Referrer(GURL(kReadingListReferrerURL),
                                    web::ReferrerPolicyDefault);
    [self.URLLoader loadURLWithParams:params];
  }

  [self stop];
}

@end
