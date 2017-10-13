// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/ntp_home_mediator.h"

#include "base/mac/foundation_util.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "components/ntp_snippets/content_suggestions_service.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/content_suggestions/content_suggestions_alert_factory.h"
#import "ios/chrome/browser/content_suggestions/content_suggestions_mediator.h"
#import "ios/chrome/browser/content_suggestions/content_suggestions_metrics_recorder.h"
#import "ios/chrome/browser/content_suggestions/ntp_home_metrics.h"
#import "ios/chrome/browser/metrics/new_tab_page_uma.h"
#include "ios/chrome/browser/tabs/tab_constants.h"
#import "ios/chrome/browser/ui/alert_coordinator/alert_coordinator.h"
#import "ios/chrome/browser/ui/commands/browser_commands.h"
#import "ios/chrome/browser/ui/commands/open_new_tab_command.h"
#import "ios/chrome/browser/ui/commands/reading_list_add_command.h"
#import "ios/chrome/browser/ui/commands/snackbar_commands.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_item.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_most_visited_item.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_view_controller.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_view_controller_audience.h"
#import "ios/chrome/browser/ui/favicon/favicon_attributes.h"
#include "ios/chrome/browser/ui/ntp/metrics.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_header_constants.h"
#import "ios/chrome/browser/ui/ntp/notification_promo_whats_new.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"
#import "ios/chrome/browser/ui/url_loader.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/third_party/material_components_ios/src/components/Snackbar/src/MaterialSnackbar.h"
#import "ios/web/public/navigation_item.h"
#import "ios/web/public/navigation_manager.h"
#include "ios/web/public/referrer.h"
#import "ios/web/public/web_state/web_state.h"
#import "ios/web/public/web_state/web_state_observer_bridge.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// URL for the page displaying help for the NTP.
const char kNTPHelpURL[] = "https://support.google.com/chrome/?p=ios_new_tab";

// The What's New promo command that shows the Bookmarks Manager.
const char kBookmarkCommand[] = "bookmark";

// The What's New promo command that launches Rate This App.
const char kRateThisAppCommand[] = "ratethisapp";
}  // namespace

@interface NTPHomeMediator ()<CRWWebStateObserver> {
  std::unique_ptr<web::WebStateObserverBridge> _webStateObserver;
}

@property(nonatomic, strong) AlertCoordinator* alertCoordinator;

@end

@implementation NTPHomeMediator

@synthesize webState = _webState;
@synthesize dispatcher = _dispatcher;
@synthesize suggestionsService = _suggestionsService;
@synthesize NTPMetrics = _NTPMetrics;
@synthesize suggestionsViewController = _suggestionsViewController;
@synthesize suggestionsMediator = _suggestionsMediator;
@synthesize alertCoordinator = _alertCoordinator;
@synthesize metricsRecorder = _metricsRecorder;

- (void)setUp {
  DCHECK(self.suggestionsService);
  _webStateObserver =
      base::MakeUnique<web::WebStateObserverBridge>(self.webState, self);
}

- (void)shutdown {
  _webStateObserver.reset();
}

#pragma mark - CRWWebStateObserver

- (void)webState:(web::WebState*)webState didLoadPageWithSuccess:(BOOL)success {
  if (!success)
    return;

  web::NavigationManager* navigationManager = webState->GetNavigationManager();
  web::NavigationItem* item = navigationManager->GetVisibleItem();
  if (item && item->GetPageDisplayState().scroll_state().offset_y() > 0) {
    CGFloat offset = item->GetPageDisplayState().scroll_state().offset_y();
    UICollectionView* collection =
        self.suggestionsViewController.collectionView;
    // Don't set the offset such as the content of the collection is smaller
    // than the part of the collection which should be displayed with that
    // offset, taking into account the size of the toolbar.
    offset = MAX(0, MIN(offset, collection.contentSize.height -
                                    collection.bounds.size.height -
                                    ntp_header::kToolbarHeight));
    collection.contentOffset = CGPointMake(0, offset);
    // Update the constraints in case the omnibox needs to be moved.
    [self.suggestionsViewController updateConstraints];
  }
}

#pragma mark - ContentSuggestionsCommands

- (void)openReadingList {
  [self.dispatcher showReadingList];
}

- (void)openPageForItemAtIndexPath:(NSIndexPath*)indexPath {
  CollectionViewItem* item = [self.suggestionsViewController.collectionViewModel
      itemAtIndexPath:indexPath];
  ContentSuggestionsItem* suggestionItem =
      base::mac::ObjCCastStrict<ContentSuggestionsItem>(item);

  [self.metricsRecorder
         onSuggestionOpened:suggestionItem
                atIndexPath:indexPath
         sectionsShownAbove:[self.suggestionsViewController
                                numberOfSectionsAbove:indexPath.section]
      suggestionsShownAbove:[self.suggestionsViewController
                                numberOfSuggestionsAbove:indexPath.section]
                 withAction:WindowOpenDisposition::CURRENT_TAB];
  self.suggestionsService->user_classifier()->OnEvent(
      ntp_snippets::UserClassifier::Metric::SUGGESTIONS_USED);

  // Use a referrer with a specific URL to mark this entry as coming from
  // ContentSuggestions.
  web::Referrer referrer;
  referrer.url = GURL(tab_constants::kDoNotConsiderForMostVisited);

  [self.dispatcher loadURL:suggestionItem.URL
                  referrer:referrer
                transition:ui::PAGE_TRANSITION_AUTO_BOOKMARK
         rendererInitiated:NO];
  [self.NTPMetrics recordAction:new_tab_page_uma::ACTION_OPENED_SUGGESTION];
}

- (void)openMostVisitedItem:(CollectionViewItem*)item
                    atIndex:(NSInteger)mostVisitedIndex {
  ContentSuggestionsMostVisitedItem* mostVisitedItem =
      base::mac::ObjCCastStrict<ContentSuggestionsMostVisitedItem>(item);

  [self logMostVisitedOpening:mostVisitedItem atIndex:mostVisitedIndex];

  [self.dispatcher loadURL:mostVisitedItem.URL
                  referrer:web::Referrer()
                transition:ui::PAGE_TRANSITION_AUTO_BOOKMARK
         rendererInitiated:NO];
}

- (void)displayContextMenuForSuggestion:(CollectionViewItem*)item
                                atPoint:(CGPoint)touchLocation
                            atIndexPath:(NSIndexPath*)indexPath
                        readLaterAction:(BOOL)readLaterAction {
  ContentSuggestionsItem* suggestionsItem =
      base::mac::ObjCCastStrict<ContentSuggestionsItem>(item);

  [self.metricsRecorder
      onMenuOpenedForSuggestion:suggestionsItem
                    atIndexPath:indexPath
          suggestionsShownAbove:[self.suggestionsViewController
                                    numberOfSuggestionsAbove:indexPath
                                                                 .section]];

  self.alertCoordinator = [ContentSuggestionsAlertFactory
      alertCoordinatorForSuggestionItem:suggestionsItem
                       onViewController:self.suggestionsViewController
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
                        onViewController:self.suggestionsViewController
                                 atPoint:touchLocation
                             atIndexPath:indexPath
                          commandHandler:self];

  [self.alertCoordinator start];
}

- (void)dismissModals {
  [self.alertCoordinator stop];
  self.alertCoordinator = nil;
}

// TODO(crbug.com/761096) : Promo handling should be DRY and tested.
- (void)handlePromoTapped {
  NotificationPromoWhatsNew* notificationPromo =
      [self.suggestionsMediator notificationPromo];
  DCHECK(notificationPromo);
  notificationPromo->HandleClosed();
  [self.NTPMetrics recordAction:new_tab_page_uma::ACTION_OPENED_PROMO];

  if (notificationPromo->IsURLPromo()) {
    [self.dispatcher webPageOrderedOpen:notificationPromo->url()
                               referrer:web::Referrer()
                           inBackground:NO
                               appendTo:kCurrentTab];
    return;
  }

  if (notificationPromo->IsChromeCommandPromo()) {
    std::string command = notificationPromo->command();
    if (command == kBookmarkCommand) {
      [self.dispatcher showBookmarksManager];
    } else if (command == kRateThisAppCommand) {
      [self.dispatcher showRateThisAppDialog];
    } else {
      NOTREACHED() << "Promo command is not valid.";
    }
    return;
  }
  NOTREACHED() << "Promo type is neither URL or command.";
}

- (void)handleLearnMoreTapped {
  [self.dispatcher loadURL:GURL(kNTPHelpURL)
                  referrer:web::Referrer()
                transition:ui::PAGE_TRANSITION_LINK
         rendererInitiated:NO];
  [self.NTPMetrics recordAction:new_tab_page_uma::ACTION_OPENED_LEARN_MORE];
}

#pragma mark - ContentSuggestionsGestureCommands

- (void)openNewTabWithSuggestionsItem:(ContentSuggestionsItem*)item
                            incognito:(BOOL)incognito {
  [self.NTPMetrics recordAction:new_tab_page_uma::ACTION_OPENED_SUGGESTION];
  self.suggestionsService->user_classifier()->OnEvent(
      ntp_snippets::UserClassifier::Metric::SUGGESTIONS_USED);

  NSIndexPath* indexPath = [self.suggestionsViewController.collectionViewModel
      indexPathForItem:item];
  if (indexPath) {
    WindowOpenDisposition disposition =
        incognito ? WindowOpenDisposition::OFF_THE_RECORD
                  : WindowOpenDisposition::NEW_BACKGROUND_TAB;
    [self.metricsRecorder
           onSuggestionOpened:item
                  atIndexPath:indexPath
           sectionsShownAbove:[self.suggestionsViewController
                                  numberOfSectionsAbove:indexPath.section]
        suggestionsShownAbove:[self.suggestionsViewController
                                  numberOfSuggestionsAbove:indexPath.section]
                   withAction:disposition];
  }

  [self openNewTabWithURL:item.URL incognito:incognito];
}

- (void)addItemToReadingList:(ContentSuggestionsItem*)item {
  NSIndexPath* indexPath = [self.suggestionsViewController.collectionViewModel
      indexPathForItem:item];
  if (indexPath) {
    [self.metricsRecorder
           onSuggestionOpened:item
                  atIndexPath:indexPath
           sectionsShownAbove:[self.suggestionsViewController
                                  numberOfSectionsAbove:indexPath.section]
        suggestionsShownAbove:[self.suggestionsViewController
                                  numberOfSuggestionsAbove:indexPath.section]
                   withAction:WindowOpenDisposition::SAVE_TO_DISK];
  }

  self.suggestionsMediator.readingListNeedsReload = YES;
  ReadingListAddCommand* command =
      [[ReadingListAddCommand alloc] initWithURL:item.URL title:item.title];
  [self.dispatcher addToReadingList:command];
}

- (void)dismissSuggestion:(ContentSuggestionsItem*)item
              atIndexPath:(NSIndexPath*)indexPath {
  NSIndexPath* itemIndexPath = indexPath;
  if (!itemIndexPath) {
    // If the caller uses a nil |indexPath|, find it from the model.
    itemIndexPath = [self.suggestionsViewController.collectionViewModel
        indexPathForItem:item];
  }

  [self.suggestionsMediator dismissSuggestion:item.suggestionIdentifier];
  [self.suggestionsViewController dismissEntryAtIndexPath:itemIndexPath];
}

- (void)openNewTabWithMostVisitedItem:(ContentSuggestionsMostVisitedItem*)item
                            incognito:(BOOL)incognito
                              atIndex:(NSInteger)index {
  [self logMostVisitedOpening:item atIndex:index];
  [self openNewTabWithURL:item.URL incognito:incognito];
}

- (void)openNewTabWithMostVisitedItem:(ContentSuggestionsMostVisitedItem*)item
                            incognito:(BOOL)incognito {
  NSInteger index =
      [self.suggestionsViewController.collectionViewModel indexPathForItem:item]
          .item;
  [self openNewTabWithMostVisitedItem:item incognito:incognito atIndex:index];
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
  return self.suggestionsViewController.scrolledToTop;
}

#pragma mark - Private

// Opens the |URL| in a new tab |incognito| or not.
- (void)openNewTabWithURL:(const GURL&)URL incognito:(BOOL)incognito {
  // Open the tab in background if it is non-incognito only.
  [self.dispatcher webPageOrderedOpen:URL
                             referrer:web::Referrer()
                          inIncognito:incognito
                         inBackground:!incognito
                             appendTo:kCurrentTab];
}

// Logs a histogram due to a Most Visited item being opened.
- (void)logMostVisitedOpening:(ContentSuggestionsMostVisitedItem*)item
                      atIndex:(NSInteger)mostVisitedIndex {
  [self.NTPMetrics
      recordAction:new_tab_page_uma::ACTION_OPENED_MOST_VISITED_ENTRY];
  base::RecordAction(base::UserMetricsAction("MobileNTPMostVisited"));

  // TODO(crbug.com/763946): Plumb generation time.
  RecordNTPTileClick(mostVisitedIndex, item.source, item.titleSource,
                     item.attributes, base::Time(), GURL());
}

// Shows a snackbar with an action to undo the removal of the most visited item
// with a |URL|.
- (void)showMostVisitedUndoForURL:(GURL)URL {
  GURL copiedURL = URL;

  MDCSnackbarMessageAction* action = [[MDCSnackbarMessageAction alloc] init];
  __weak NTPHomeMediator* weakSelf = self;
  action.handler = ^{
    NTPHomeMediator* strongSelf = weakSelf;
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
  [self.dispatcher showSnackbarMessage:message];
}

@end
