// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/ntp/ntp_home_coordinator.h"

#include "base/mac/foundation_util.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/sys_string_conversions.h"
#include "components/ntp_snippets/content_suggestions_service.h"
#include "components/ntp_tiles/most_visited_sites.h"
#include "components/reading_list/core/reading_list_model.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/content_suggestions/content_suggestions_alert_factory.h"
#import "ios/chrome/browser/content_suggestions/content_suggestions_mediator.h"
#include "ios/chrome/browser/favicon/ios_chrome_large_icon_cache_factory.h"
#include "ios/chrome/browser/favicon/ios_chrome_large_icon_service_factory.h"
#include "ios/chrome/browser/ntp_snippets/ios_chrome_content_suggestions_service_factory.h"
#include "ios/chrome/browser/ntp_tiles/ios_most_visited_sites_factory.h"
#include "ios/chrome/browser/reading_list/reading_list_model_factory.h"
#import "ios/chrome/browser/ui/alert_coordinator/alert_coordinator.h"
#import "ios/chrome/browser/ui/browser_list/browser.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_gesture_commands.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_item.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_most_visited_item.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_collection_utils.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_commands.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_header_controlling.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_header_synchronizer.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_header_view_controller_delegate.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_view_controller.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_view_controller_audience.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_view_controller_delegate.h"
#import "ios/chrome/browser/ui/coordinators/browser_coordinator+internal.h"
#import "ios/chrome/browser/ui/ntp/google_landing_mediator.h"
#import "ios/chrome/browser/ui/ntp/google_landing_view_controller.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_header_constants.h"
#import "ios/chrome/browser/ui/ntp/notification_promo_whats_new.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/clean/chrome/browser/ui/ntp/ntp_home_header_coordinator.h"
#import "ios/shared/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/third_party/material_components_ios/src/components/Snackbar/src/MaterialSnackbar.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface NTPHomeCoordinator ()<
    ContentSuggestionsCommands,
    ContentSuggestionsGestureCommands,
    ContentSuggestionsHeaderViewControllerCommandHandler,
    ContentSuggestionsHeaderViewControllerDelegate,
    ContentSuggestionsViewControllerAudience>

@property(nonatomic, strong) AlertCoordinator* alertCoordinator;

@property(nonatomic, strong) GoogleLandingMediator* googleLandingMediator;
@property(nonatomic, strong) ContentSuggestionsMediator* suggestionsMediator;
@property(nonatomic, strong) NTPHomeHeaderCoordinator* headerCoordinator;
@property(nonatomic, strong) ContentSuggestionsViewController* viewController;
@property(nonatomic, strong)
    ContentSuggestionsHeaderSynchronizer* headerCollectionInteractionHandler;

@end

@implementation NTPHomeCoordinator

@synthesize alertCoordinator = _alertCoordinator;
@synthesize headerCoordinator = _headerCoordinator;
@synthesize googleLandingMediator = _googleLandingMediator;
@synthesize viewController = _viewController;
@synthesize suggestionsMediator = _suggestionsMediator;
@synthesize headerCollectionInteractionHandler =
    _headerCollectionInteractionHandler;

#pragma mark - BrowserCoordinator

- (void)start {
  ntp_snippets::ContentSuggestionsService* contentSuggestionsService =
      IOSChromeContentSuggestionsServiceFactory::GetForBrowserState(
          self.browser->browser_state());
  contentSuggestionsService->remote_suggestions_scheduler()
      ->OnSuggestionsSurfaceOpened();

  self.headerCoordinator = [[NTPHomeHeaderCoordinator alloc] init];
  self.headerCoordinator.delegate = self;
  self.headerCoordinator.commandHandler = self;
  [self addChildCoordinator:self.headerCoordinator];
  [self.headerCoordinator start];

  self.googleLandingMediator = [[GoogleLandingMediator alloc]
      initWithConsumer:self.headerCoordinator.consumer
          browserState:self.browser->browser_state()
            dispatcher:static_cast<id>(self.browser->dispatcher())
          webStateList:&self.browser->web_state_list()];

  favicon::LargeIconService* largeIconService =
      IOSChromeLargeIconServiceFactory::GetForBrowserState(
          self.browser->browser_state());
  LargeIconCache* cache = IOSChromeLargeIconCacheFactory::GetForBrowserState(
      self.browser->browser_state());
  std::unique_ptr<ntp_tiles::MostVisitedSites> mostVisitedFactory =
      IOSMostVisitedSitesFactory::NewForBrowserState(
          self.browser->browser_state());
  self.suggestionsMediator = [[ContentSuggestionsMediator alloc]
      initWithContentService:contentSuggestionsService
            largeIconService:largeIconService
              largeIconCache:cache
             mostVisitedSite:std::move(mostVisitedFactory)];
  self.suggestionsMediator.commandHandler = self;
  self.suggestionsMediator.headerProvider =
      self.headerCoordinator.headerProvider;

  self.viewController = [[ContentSuggestionsViewController alloc]
      initWithStyle:CollectionViewControllerStyleDefault
         dataSource:self.suggestionsMediator];
  self.viewController.suggestionCommandHandler = self;
  self.viewController.suggestionsDelegate =
      self.headerCoordinator.collectionDelegate;
  self.viewController.audience = self;

  [self.viewController
      addChildViewController:self.headerCoordinator.viewController];
  [self.headerCoordinator.viewController
      didMoveToParentViewController:self.viewController];

  self.headerCollectionInteractionHandler =
      [[ContentSuggestionsHeaderSynchronizer alloc]
          initWithCollectionController:self.viewController
                      headerController:self.headerCoordinator.headerController];

  self.viewController.headerCommandHandler =
      self.headerCollectionInteractionHandler;
  self.headerCoordinator.collectionSynchronizer =
      self.headerCollectionInteractionHandler;

  [super start];
}

- (void)stop {
  [super stop];
  [self.googleLandingMediator shutdown];
  [self.headerCoordinator stop];
  self.viewController = nil;
  self.suggestionsMediator = nil;
  self.googleLandingMediator = nil;
  self.headerCoordinator = nil;
}

#pragma mark - ContentSuggestionsCommands

- (void)openReadingList {
  // TODO: implement this.
}

- (void)openPageForItem:(CollectionViewItem*)item {
  // TODO: implement this.
}

- (void)openMostVisitedItem:(CollectionViewItem*)item
                    atIndex:(NSInteger)mostVisitedIndex {
  // TODO: implement this.
}

- (void)displayContextMenuForSuggestion:(CollectionViewItem*)item
                                atPoint:(CGPoint)touchLocation
                            atIndexPath:(NSIndexPath*)indexPath
                        readLaterAction:(BOOL)readLaterAction {
  ContentSuggestionsItem* suggestionsItem =
      base::mac::ObjCCastStrict<ContentSuggestionsItem>(item);
  self.alertCoordinator = [ContentSuggestionsAlertFactory
      alertCoordinatorForSuggestionItem:suggestionsItem
                       onViewController:self.viewController
                                atPoint:touchLocation
                            atIndexPath:indexPath
                        readLaterAction:readLaterAction
                         commandHandler:self];

  [self.alertCoordinator start];
}

- (void)displayContextMenuForMostVisitedItem:(CollectionViewItem*)item
                                     atPoint:(CGPoint)touchLocation
                                 atIndexPath:(NSIndexPath*)indexPath {
  ContentSuggestionsMostVisitedItem* mostVisitedItem =
      base::mac::ObjCCastStrict<ContentSuggestionsMostVisitedItem>(item);
  self.alertCoordinator = [ContentSuggestionsAlertFactory
      alertCoordinatorForMostVisitedItem:mostVisitedItem
                        onViewController:self.viewController
                                 atPoint:touchLocation
                             atIndexPath:indexPath
                          commandHandler:self];

  [self.alertCoordinator start];
}

- (void)handlePromoTapped {
  // TODO: implement this.
}

- (void)handleLearnMoreTapped {
  // TODO: implement this.
}

- (void)dismissModals {
  [self.alertCoordinator stop];
  self.alertCoordinator = nil;
}

#pragma mark - ContentSuggestionsGestureCommands

- (void)openNewTabWithSuggestionsItem:(ContentSuggestionsItem*)item
                            incognito:(BOOL)incognito {
  // TODO: implement this.
}

- (void)addItemToReadingList:(ContentSuggestionsItem*)item {
  base::RecordAction(base::UserMetricsAction("MobileReadingListAdd"));
  ReadingListModel* readingModel = ReadingListModelFactory::GetForBrowserState(
      self.browser->browser_state());
  readingModel->AddEntry(item.URL, base::SysNSStringToUTF8(item.title),
                         reading_list::ADDED_VIA_CURRENT_APP);
}

- (void)dismissSuggestion:(ContentSuggestionsItem*)item
              atIndexPath:(NSIndexPath*)indexPath {
  NSIndexPath* itemIndexPath = indexPath;
  if (!itemIndexPath) {
    // If the caller uses a nil |indexPath|, find it from the model.
    itemIndexPath =
        [self.viewController.collectionViewModel indexPathForItem:item];
  }

  // TODO(crbug.com/691979): Add metrics.
  [self.suggestionsMediator dismissSuggestion:item.suggestionIdentifier];
  [self.viewController dismissEntryAtIndexPath:itemIndexPath];
}

- (void)openNewTabWithMostVisitedItem:(ContentSuggestionsMostVisitedItem*)item
                            incognito:(BOOL)incognito
                              atIndex:(NSInteger)index {
  // TODO: implement this.
}

- (void)openNewTabWithMostVisitedItem:(ContentSuggestionsMostVisitedItem*)item
                            incognito:(BOOL)incognito {
  // TODO: implement this.
}

- (void)removeMostVisited:(ContentSuggestionsMostVisitedItem*)item {
  base::RecordAction(base::UserMetricsAction("MostVisited_UrlBlacklisted"));
  [self.suggestionsMediator blacklistMostVisitedURL:item.URL];
  [self showMostVisitedUndoForURL:item.URL];
}

#pragma mark - ContentSuggestionsHeaderViewControllerDelegate

- (BOOL)isContextMenuVisible {
  return self.alertCoordinator.isVisible;
}

- (BOOL)isScrolledToTop {
  return self.viewController.scrolledToTop;
}

#pragma mark - ContentSuggestionsViewControllerAudience

- (void)contentOffsetDidChange {
  // TODO: implement this.
}

- (void)promoShown {
  NotificationPromoWhatsNew* notificationPromo =
      [self.suggestionsMediator notificationPromo];
  notificationPromo->HandleViewed();
  [self.headerCoordinator.consumer
      setPromoCanShow:notificationPromo->CanShow()];
}

#pragma mark - Private

// Shows a snackbar with an action to undo the removal of the most visited item
// with a |URL|.
- (void)showMostVisitedUndoForURL:(GURL)URL {
  GURL copiedURL = URL;

  MDCSnackbarMessageAction* action = [[MDCSnackbarMessageAction alloc] init];
  __weak NTPHomeCoordinator* weakSelf = self;
  action.handler = ^{
    NTPHomeCoordinator* strongSelf = weakSelf;
    if (!strongSelf)
      return;
    [strongSelf.suggestionsMediator whitelistMostVisitedURL:copiedURL];
  };
  action.title = l10n_util::GetNSString(IDS_NEW_TAB_UNDO_THUMBNAIL_REMOVE);
  action.accessibilityIdentifier = @"Undo";

  TriggerHapticFeedbackForNotification(UINotificationFeedbackTypeSuccess);
  MDCSnackbarMessage* message = [MDCSnackbarMessage
      messageWithText:l10n_util::GetNSString(
                          IDS_IOS_NEW_TAB_MOST_VISITED_ITEM_REMOVED)];
  message.action = action;
  message.category = @"MostVisitedUndo";
  [MDCSnackbarManager showMessage:message];
}

@end
