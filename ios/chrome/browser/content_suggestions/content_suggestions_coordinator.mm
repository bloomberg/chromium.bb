// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/content_suggestions_coordinator.h"

#include "base/mac/foundation_util.h"
#include "base/mac/scoped_nsobject.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/sys_string_conversions.h"
#include "components/ntp_snippets/content_suggestions_service.h"
#include "components/ntp_snippets/ntp_snippets_constants.h"
#include "components/ntp_snippets/remote/remote_suggestions_scheduler.h"
#include "components/ntp_tiles/metrics.h"
#include "components/ntp_tiles/most_visited_sites.h"
#include "components/reading_list/core/reading_list_model.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/content_suggestions/content_suggestions_alert_commands.h"
#import "ios/chrome/browser/content_suggestions/content_suggestions_alert_factory.h"
#import "ios/chrome/browser/content_suggestions/content_suggestions_header_view_controller.h"
#import "ios/chrome/browser/content_suggestions/content_suggestions_header_view_controller_delegate.h"
#import "ios/chrome/browser/content_suggestions/content_suggestions_mediator.h"
#include "ios/chrome/browser/favicon/ios_chrome_large_icon_cache_factory.h"
#include "ios/chrome/browser/favicon/ios_chrome_large_icon_service_factory.h"
#include "ios/chrome/browser/favicon/large_icon_cache.h"
#import "ios/chrome/browser/metrics/new_tab_page_uma.h"
#include "ios/chrome/browser/ntp_snippets/ios_chrome_content_suggestions_service_factory.h"
#include "ios/chrome/browser/ntp_tiles/ios_most_visited_sites_factory.h"
#include "ios/chrome/browser/reading_list/reading_list_model_factory.h"
#import "ios/chrome/browser/ui/alert_coordinator/alert_coordinator.h"
#import "ios/chrome/browser/ui/commands/UIKit+ChromeExecuteCommand.h"
#import "ios/chrome/browser/ui/commands/browser_commands.h"
#import "ios/chrome/browser/ui/commands/generic_chrome_command.h"
#include "ios/chrome/browser/ui/commands/ios_command_ids.h"
#import "ios/chrome/browser/ui/commands/open_new_tab_command.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_item.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_most_visited_item.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_collection_utils.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_commands.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_data_sink.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_header_synchronizer.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_view_controller.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_view_controller_audience.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_view_controller_delegate.h"
#import "ios/chrome/browser/ui/content_suggestions/identifier/content_suggestion_identifier.h"
#import "ios/chrome/browser/ui/ntp/google_landing_mediator.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_header_constants.h"
#import "ios/chrome/browser/ui/ntp/notification_promo_whats_new.h"
#import "ios/chrome/browser/ui/overscroll_actions/overscroll_actions_controller.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"
#import "ios/chrome/browser/ui/url_loader.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/third_party/material_components_ios/src/components/Snackbar/src/MaterialSnackbar.h"
#include "ios/web/public/referrer.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/strings/grit/ui_strings.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const char kNTPHelpURL[] = "https://support.google.com/chrome/?p=new_tab";
}  // namespace

@interface ContentSuggestionsCoordinator ()<
    ContentSuggestionsAlertCommands,
    ContentSuggestionsCommands,
    ContentSuggestionsHeaderViewControllerCommandHandler,
    ContentSuggestionsHeaderViewControllerDelegate,
    ContentSuggestionsViewControllerAudience,
    ContentSuggestionsViewControllerDelegate,
    OverscrollActionsControllerDelegate>

@property(nonatomic, strong) AlertCoordinator* alertCoordinator;
@property(nonatomic, strong)
    ContentSuggestionsViewController* suggestionsViewController;
@property(nonatomic, strong)
    ContentSuggestionsMediator* contentSuggestionsMediator;
@property(nonatomic, strong) GoogleLandingMediator* googleLandingMediator;
@property(nonatomic, strong)
    ContentSuggestionsHeaderSynchronizer* headerCollectionInteractionHandler;

// Redefined as readwrite.
@property(nonatomic, strong, readwrite)
    ContentSuggestionsHeaderViewController* headerController;

@end

@implementation ContentSuggestionsCoordinator

@synthesize alertCoordinator = _alertCoordinator;
@synthesize browserState = _browserState;
@synthesize suggestionsViewController = _suggestionsViewController;
@synthesize URLLoader = _URLLoader;
@synthesize visible = _visible;
@synthesize contentSuggestionsMediator = _contentSuggestionsMediator;
@synthesize headerCollectionInteractionHandler =
    _headerCollectionInteractionHandler;
@synthesize headerController = _headerController;
@synthesize googleLandingMediator = _googleLandingMediator;
@synthesize webStateList = _webStateList;
@synthesize dispatcher = _dispatcher;
@synthesize delegate = _delegate;

- (void)start {
  if (self.visible || !self.browserState) {
    // Prevent this coordinator from being started twice in a row or without a
    // browser state.
    return;
  }

  _visible = YES;

  ntp_snippets::ContentSuggestionsService* contentSuggestionsService =
      IOSChromeContentSuggestionsServiceFactory::GetForBrowserState(
          self.browserState);
  contentSuggestionsService->remote_suggestions_scheduler()
      ->OnSuggestionsSurfaceOpened();

  self.headerController = [[ContentSuggestionsHeaderViewController alloc] init];
  self.headerController.dispatcher = self.dispatcher;
  self.headerController.delegate = self;
  self.headerController.readingListModel =
      ReadingListModelFactory::GetForBrowserState(self.browserState);
  self.googleLandingMediator =
      [[GoogleLandingMediator alloc] initWithConsumer:self.headerController
                                         browserState:self.browserState
                                           dispatcher:self.dispatcher
                                         webStateList:self.webStateList];

  favicon::LargeIconService* largeIconService =
      IOSChromeLargeIconServiceFactory::GetForBrowserState(self.browserState);
  LargeIconCache* cache =
      IOSChromeLargeIconCacheFactory::GetForBrowserState(self.browserState);
  std::unique_ptr<ntp_tiles::MostVisitedSites> mostVisitedFactory =
      IOSMostVisitedSitesFactory::NewForBrowserState(self.browserState);
  self.contentSuggestionsMediator = [[ContentSuggestionsMediator alloc]
      initWithContentService:contentSuggestionsService
            largeIconService:largeIconService
              largeIconCache:cache
             mostVisitedSite:std::move(mostVisitedFactory)];
  self.contentSuggestionsMediator.commandHandler = self;
  self.contentSuggestionsMediator.headerProvider = self.headerController;

  self.suggestionsViewController = [[ContentSuggestionsViewController alloc]
      initWithStyle:CollectionViewControllerStyleDefault
         dataSource:self.contentSuggestionsMediator];
  self.suggestionsViewController.suggestionCommandHandler = self;
  self.suggestionsViewController.suggestionsDelegate = self;
  self.suggestionsViewController.audience = self;
  self.suggestionsViewController.overscrollDelegate = self;

  [self.suggestionsViewController addChildViewController:self.headerController];
  [self.headerController
      didMoveToParentViewController:self.suggestionsViewController];

  self.headerCollectionInteractionHandler =
      [[ContentSuggestionsHeaderSynchronizer alloc]
          initWithCollectionController:self.suggestionsViewController
                      headerController:self.headerController];

  self.suggestionsViewController.headerCommandHandler =
      self.headerCollectionInteractionHandler;
  self.headerController.collectionSynchronizer =
      self.headerCollectionInteractionHandler;
}

- (void)stop {
  self.contentSuggestionsMediator = nil;
  self.alertCoordinator = nil;
  self.headerController = nil;
  [self.googleLandingMediator shutdown];
  self.googleLandingMediator = nil;
  _visible = NO;
}

- (UIViewController*)viewController {
  return self.suggestionsViewController;
}

#pragma mark - ContentSuggestionsCommands

- (void)openReadingList {
  [self.dispatcher showReadingList];
}

- (void)openPageForItem:(CollectionViewItem*)item {
  // TODO(crbug.com/691979): Add metrics.

  ContentSuggestionsItem* suggestionItem =
      base::mac::ObjCCastStrict<ContentSuggestionsItem>(item);

  // Use a referrer with a specific URL to mark this entry as coming from
  // ContentSuggestions.
  web::Referrer referrer;
  referrer.url = GURL(ntp_snippets::kContentSuggestionsApiScope);

  [self.URLLoader loadURL:suggestionItem.URL
                 referrer:referrer
               transition:ui::PAGE_TRANSITION_AUTO_BOOKMARK
        rendererInitiated:NO];
}

- (void)openMostVisitedItem:(CollectionViewItem*)item
                    atIndex:(NSInteger)mostVisitedIndex {
  ContentSuggestionsMostVisitedItem* mostVisitedItem =
      base::mac::ObjCCastStrict<ContentSuggestionsMostVisitedItem>(item);

  [self logMostVisitedOpening:mostVisitedItem atIndex:mostVisitedIndex];

  [self.URLLoader loadURL:mostVisitedItem.URL
                 referrer:web::Referrer()
               transition:ui::PAGE_TRANSITION_AUTO_BOOKMARK
        rendererInitiated:NO];
}

- (void)displayContextMenuForArticle:(CollectionViewItem*)item
                             atPoint:(CGPoint)touchLocation
                         atIndexPath:(NSIndexPath*)indexPath {
  self.alertCoordinator = [ContentSuggestionsAlertFactory
      alertCoordinatorForSuggestionItem:item
                       onViewController:self.suggestionsViewController
                                atPoint:touchLocation
                            atIndexPath:indexPath
                         commandHandler:self];

  [self.alertCoordinator start];
}

- (void)displayContextMenuForMostVisitedItem:(CollectionViewItem*)item
                                     atPoint:(CGPoint)touchLocation
                                 atIndexPath:(NSIndexPath*)indexPath {
  self.alertCoordinator = [ContentSuggestionsAlertFactory
      alertCoordinatorForMostVisitedItem:item
                        onViewController:self.suggestionsViewController
                                 atPoint:touchLocation
                             atIndexPath:indexPath
                          commandHandler:self];

  [self.alertCoordinator start];
}

- (void)handlePromoTapped {
  NotificationPromoWhatsNew* notificationPromo =
      [self.contentSuggestionsMediator notificationPromo];
  DCHECK(notificationPromo);
  notificationPromo->HandleClosed();

  if (notificationPromo->IsURLPromo()) {
    [self.URLLoader webPageOrderedOpen:notificationPromo->url()
                              referrer:web::Referrer()
                          inBackground:NO
                              appendTo:kCurrentTab];
    return;
  }

  if (notificationPromo->IsChromeCommand()) {
    GenericChromeCommand* command = [[GenericChromeCommand alloc]
        initWithTag:notificationPromo->command_id()];
    [self.suggestionsViewController chromeExecuteCommand:command];
    return;
  }
  NOTREACHED();
}

- (void)handleLearnMoreTapped {
  // TODO(crbug.com/691979): Add metrics.
  [self.URLLoader loadURL:GURL(kNTPHelpURL)
                 referrer:web::Referrer()
               transition:ui::PAGE_TRANSITION_LINK
        rendererInitiated:NO];
}

#pragma mark - ContentSuggestionsAlertCommands

- (void)openNewTabWithSuggestionsItem:(CollectionViewItem*)item
                            incognito:(BOOL)incognito {
  ContentSuggestionsItem* suggestionsItem =
      base::mac::ObjCCastStrict<ContentSuggestionsItem>(item);
  [self openNewTabWithURL:suggestionsItem.URL incognito:incognito];
}

- (void)addItemToReadingList:(CollectionViewItem*)item {
  ContentSuggestionsItem* suggestionsItem =
      base::mac::ObjCCastStrict<ContentSuggestionsItem>(item);
  base::RecordAction(base::UserMetricsAction("MobileReadingListAdd"));
  ReadingListModel* readingModel =
      ReadingListModelFactory::GetForBrowserState(self.browserState);
  readingModel->AddEntry(suggestionsItem.URL,
                         base::SysNSStringToUTF8(suggestionsItem.title),
                         reading_list::ADDED_VIA_CURRENT_APP);
}

- (void)dismissSuggestion:(CollectionViewItem*)item
              atIndexPath:(NSIndexPath*)indexPath {
  ContentSuggestionsItem* suggestionsItem =
      base::mac::ObjCCastStrict<ContentSuggestionsItem>(item);

  // TODO(crbug.com/691979): Add metrics.
  [self.contentSuggestionsMediator
      dismissSuggestion:suggestionsItem.suggestionIdentifier];
  [self.suggestionsViewController dismissEntryAtIndexPath:indexPath];
}

- (void)openNewTabWithMostVisitedItem:(CollectionViewItem*)item
                            incognito:(BOOL)incognito
                              atIndex:(NSInteger)index {
  ContentSuggestionsMostVisitedItem* mostVisitedItem =
      base::mac::ObjCCastStrict<ContentSuggestionsMostVisitedItem>(item);
  [self logMostVisitedOpening:mostVisitedItem atIndex:index];
  [self openNewTabWithURL:mostVisitedItem.URL incognito:incognito];
}

- (void)removeMostVisited:(CollectionViewItem*)item {
  ContentSuggestionsMostVisitedItem* mostVisitedItem =
      base::mac::ObjCCastStrict<ContentSuggestionsMostVisitedItem>(item);
  base::RecordAction(base::UserMetricsAction("MostVisited_UrlBlacklisted"));
  [self.contentSuggestionsMediator blacklistMostVisitedURL:mostVisitedItem.URL];
  [self showMostVisitedUndoForURL:mostVisitedItem.URL];
}

#pragma mark - ContentSuggestionsHeaderViewControllerDelegate

- (BOOL)isContextMenuVisible {
  return self.alertCoordinator.isVisible;
}

- (BOOL)isScrolledToTop {
  return self.suggestionsViewController.scrolledToTop;
}

#pragma mark - ContentSuggestionsViewControllerDelegate

- (CGFloat)pinnedOffsetY {
  CGFloat headerHeight = content_suggestions::heightForLogoHeader(
      self.headerController.logoIsShowing,
      [self.contentSuggestionsMediator notificationPromo]->CanShow());
  CGFloat offsetY =
      headerHeight - ntp_header::kScrolledToTopOmniboxBottomMargin;
  if (!IsIPadIdiom())
    offsetY -= ntp_header::kToolbarHeight;

  return offsetY;
}

- (BOOL)isOmniboxFocused {
  return [self.headerController isOmniboxFocused];
}

- (CGFloat)headerHeight {
  return content_suggestions::heightForLogoHeader(
      self.headerController.logoIsShowing,
      [self.contentSuggestionsMediator notificationPromo]->CanShow());
}

#pragma mark - ContentSuggestionsViewControllerAudience

- (void)contentSuggestionsDidScroll {
  [self.delegate updateNtpBarShadowForPanelController:self];
}

- (void)promoShown {
  NotificationPromoWhatsNew* notificationPromo =
      [self.contentSuggestionsMediator notificationPromo];
  notificationPromo->HandleViewed();
  [self.headerController setPromoCanShow:notificationPromo->CanShow()];
}

#pragma mark - OverscrollActionsControllerDelegate

- (void)overscrollActionsController:(OverscrollActionsController*)controller
                   didTriggerAction:(OverscrollAction)action {
  switch (action) {
    case OverscrollAction::NEW_TAB: {
      [_dispatcher openNewTab:[OpenNewTabCommand command]];
    } break;
    case OverscrollAction::CLOSE_TAB: {
      [_dispatcher closeCurrentTab];
    } break;
    case OverscrollAction::REFRESH:
      [self reload];
      break;
    case OverscrollAction::NONE:
      NOTREACHED();
      break;
  }
}

- (BOOL)shouldAllowOverscrollActions {
  return YES;
}

- (UIView*)toolbarSnapshotView {
  return
      [[self.headerController toolBarView] snapshotViewAfterScreenUpdates:NO];
}

- (UIView*)headerView {
  return self.suggestionsViewController.view;
}

- (CGFloat)overscrollActionsControllerHeaderInset:
    (OverscrollActionsController*)controller {
  return 0;
}

- (CGFloat)overscrollHeaderHeight {
  return [self.headerController toolBarView].bounds.size.height;
}

#pragma mark - NewTabPagePanelProtocol

- (CGFloat)alphaForBottomShadow {
  UICollectionView* collection = self.suggestionsViewController.collectionView;

  CGFloat pixelsBelowFrame =
      collection.contentSize.height - CGRectGetMaxY(collection.bounds);
  CGFloat alpha = pixelsBelowFrame / kNewTabPageDistanceToFadeShadow;
  return MIN(MAX(alpha, 0), 1);
}

- (UIView*)view {
  return self.suggestionsViewController.view;
}

- (void)reload {
  [self.contentSuggestionsMediator.dataSink reloadAllData];
}

- (void)wasShown {
  self.headerController.isShowing = YES;
  [self.suggestionsViewController.collectionView
          .collectionViewLayout invalidateLayout];
}

- (void)wasHidden {
  self.headerController.isShowing = NO;
}

- (void)dismissModals {
  [self.alertCoordinator stop];
  self.alertCoordinator = nil;
}

- (void)dismissKeyboard {
}

- (void)setScrollsToTop:(BOOL)enable {
}

#pragma mark - Private

// Opens the |URL| in a new tab |incognito| or not.
- (void)openNewTabWithURL:(const GURL&)URL incognito:(BOOL)incognito {
  // TODO(crbug.com/691979): Add metrics.

  [self.URLLoader webPageOrderedOpen:URL
                            referrer:web::Referrer()
                         inIncognito:incognito
                        inBackground:NO
                            appendTo:kCurrentTab];
}

// Logs a histogram due to a Most Visited item being opened.
- (void)logMostVisitedOpening:(ContentSuggestionsMostVisitedItem*)item
                      atIndex:(NSInteger)mostVisitedIndex {
  new_tab_page_uma::RecordAction(
      self.browserState, new_tab_page_uma::ACTION_OPENED_MOST_VISITED_ENTRY);
  base::RecordAction(base::UserMetricsAction("MobileNTPMostVisited"));

  ntp_tiles::metrics::RecordTileClick(mostVisitedIndex, item.source,
                                      [item tileType]);
}

// Shows a snackbar with an action to undo the removal of the most visited item
// with a |URL|.
- (void)showMostVisitedUndoForURL:(GURL)URL {
  GURL copiedURL = URL;

  MDCSnackbarMessageAction* action = [[MDCSnackbarMessageAction alloc] init];
  __weak ContentSuggestionsCoordinator* weakSelf = self;
  action.handler = ^{
    ContentSuggestionsCoordinator* strongSelf = weakSelf;
    if (!strongSelf)
      return;
    [strongSelf.contentSuggestionsMediator whitelistMostVisitedURL:copiedURL];
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
