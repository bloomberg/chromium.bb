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
#include "components/ntp_snippets/remote/remote_suggestions_scheduler.h"
#include "components/ntp_tiles/metrics.h"
#include "components/ntp_tiles/most_visited_sites.h"
#include "components/reading_list/core/reading_list_model.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/content_suggestions/content_suggestions_mediator.h"
#include "ios/chrome/browser/favicon/ios_chrome_large_icon_service_factory.h"
#import "ios/chrome/browser/metrics/new_tab_page_uma.h"
#include "ios/chrome/browser/ntp_snippets/ios_chrome_content_suggestions_service_factory.h"
#include "ios/chrome/browser/ntp_tiles/ios_most_visited_sites_factory.h"
#include "ios/chrome/browser/reading_list/reading_list_model_factory.h"
#import "ios/chrome/browser/ui/alert_coordinator/action_sheet_coordinator.h"
#import "ios/chrome/browser/ui/commands/UIKit+ChromeExecuteCommand.h"
#import "ios/chrome/browser/ui/commands/generic_chrome_command.h"
#include "ios/chrome/browser/ui/commands/ios_command_ids.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_item.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_most_visited_item.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_commands.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_view_controller.h"
#import "ios/chrome/browser/ui/content_suggestions/identifier/content_suggestion_identifier.h"
#import "ios/chrome/browser/ui/ntp/notification_promo_whats_new.h"
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

@interface ContentSuggestionsCoordinator ()<ContentSuggestionsCommands>

@property(nonatomic, strong) AlertCoordinator* alertCoordinator;
@property(nonatomic, strong) UINavigationController* navigationController;
@property(nonatomic, strong)
    ContentSuggestionsViewController* suggestionsViewController;
@property(nonatomic, strong)
    ContentSuggestionsMediator* contentSuggestionsMediator;

// Opens the |URL| in a new tab |incognito| or not.
- (void)openNewTabWithURL:(const GURL&)URL incognito:(BOOL)incognito;
// Dismisses the |article|, removing it from the content service, and dismisses
// the item at |indexPath| in the view controller.
- (void)dismissArticle:(ContentSuggestionsItem*)article
           atIndexPath:(NSIndexPath*)indexPath;

@end

@implementation ContentSuggestionsCoordinator

@synthesize alertCoordinator = _alertCoordinator;
@synthesize browserState = _browserState;
@synthesize navigationController = _navigationController;
@synthesize suggestionsViewController = _suggestionsViewController;
@synthesize URLLoader = _URLLoader;
@synthesize visible = _visible;
@synthesize contentSuggestionsMediator = _contentSuggestionsMediator;

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
  contentSuggestionsService->remote_suggestions_scheduler()->OnNTPOpened();

  self.contentSuggestionsMediator = [[ContentSuggestionsMediator alloc]
      initWithContentService:contentSuggestionsService
            largeIconService:IOSChromeLargeIconServiceFactory::
                                 GetForBrowserState(self.browserState)
             mostVisitedSite:IOSMostVisitedSitesFactory::NewForBrowserState(
                                 self.browserState)];
  self.contentSuggestionsMediator.commandHandler = self;

  self.suggestionsViewController = [[ContentSuggestionsViewController alloc]
      initWithStyle:CollectionViewControllerStyleDefault
         dataSource:self.contentSuggestionsMediator];

  self.suggestionsViewController.suggestionCommandHandler = self;
  _navigationController = [[UINavigationController alloc]
      initWithRootViewController:self.suggestionsViewController];

  self.suggestionsViewController.navigationItem.leftBarButtonItem =
      [[UIBarButtonItem alloc]
          initWithTitle:l10n_util::GetNSString(IDS_IOS_SUGGESTIONS_DONE)
                  style:UIBarButtonItemStylePlain
                 target:self
                 action:@selector(stop)];

  [self.baseViewController presentViewController:_navigationController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  [[self.navigationController presentingViewController]
      dismissViewControllerAnimated:YES
                         completion:nil];
  self.navigationController = nil;
  self.contentSuggestionsMediator = nil;
  self.alertCoordinator = nil;
  _visible = NO;
}

#pragma mark - ContentSuggestionsCommands

- (void)openReadingList {
  [self.baseViewController
      chromeExecuteCommand:[GenericChromeCommand
                               commandWithTag:IDC_SHOW_READING_LIST]];
}

- (void)openPageForItem:(CollectionViewItem*)item {
  // TODO(crbug.com/691979): Add metrics.

  ContentSuggestionsItem* suggestionItem =
      base::mac::ObjCCastStrict<ContentSuggestionsItem>(item);

  [self.URLLoader loadURL:suggestionItem.URL
                 referrer:web::Referrer()
               transition:ui::PAGE_TRANSITION_AUTO_BOOKMARK
        rendererInitiated:NO];

  [self stop];
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

  [self stop];
}

- (void)displayContextMenuForArticle:(CollectionViewItem*)item
                             atPoint:(CGPoint)touchLocation
                         atIndexPath:(NSIndexPath*)indexPath {
  ContentSuggestionsItem* articleItem =
      base::mac::ObjCCastStrict<ContentSuggestionsItem>(item);
  self.alertCoordinator = [[ActionSheetCoordinator alloc]
      initWithBaseViewController:self.navigationController
                           title:nil
                         message:nil
                            rect:CGRectMake(touchLocation.x, touchLocation.y, 0,
                                            0)
                            view:self.suggestionsViewController.collectionView];

  __weak ContentSuggestionsCoordinator* weakSelf = self;
  GURL articleURL = articleItem.URL;
  NSString* articleTitle = articleItem.title;
  __weak ContentSuggestionsItem* weakArticle = articleItem;

  NSString* openInNewTabTitle =
      l10n_util::GetNSString(IDS_IOS_CONTENT_CONTEXT_OPENLINKNEWTAB);
  [self.alertCoordinator
      addItemWithTitle:openInNewTabTitle
                action:^{
                  // TODO(crbug.com/691979): Add metrics.
                  [weakSelf openNewTabWithURL:articleURL incognito:NO];
                }
                 style:UIAlertActionStyleDefault];

  NSString* openInNewTabIncognitoTitle =
      l10n_util::GetNSString(IDS_IOS_CONTENT_CONTEXT_OPENLINKNEWINCOGNITOTAB);
  [self.alertCoordinator
      addItemWithTitle:openInNewTabIncognitoTitle
                action:^{
                  // TODO(crbug.com/691979): Add metrics.
                  [weakSelf openNewTabWithURL:articleURL incognito:YES];
                }
                 style:UIAlertActionStyleDefault];

  NSString* readLaterTitle =
      l10n_util::GetNSString(IDS_IOS_CONTENT_CONTEXT_ADDTOREADINGLIST);
  [self.alertCoordinator
      addItemWithTitle:readLaterTitle
                action:^{
                  ContentSuggestionsCoordinator* strongSelf = weakSelf;
                  if (!strongSelf)
                    return;

                  base::RecordAction(
                      base::UserMetricsAction("MobileReadingListAdd"));
                  // TODO(crbug.com/691979): Add metrics.

                  ReadingListModel* readingModel =
                      ReadingListModelFactory::GetForBrowserState(
                          strongSelf.browserState);
                  readingModel->AddEntry(articleURL,
                                         base::SysNSStringToUTF8(articleTitle),
                                         reading_list::ADDED_VIA_CURRENT_APP);
                }
                 style:UIAlertActionStyleDefault];

  NSString* deleteTitle =
      l10n_util::GetNSString(IDS_IOS_CONTENT_SUGGESTIONS_REMOVE);
  [self.alertCoordinator addItemWithTitle:deleteTitle
                                   action:^{
                                     // TODO(crbug.com/691979): Add metrics.
                                     [weakSelf dismissArticle:weakArticle
                                                  atIndexPath:indexPath];
                                   }
                                    style:UIAlertActionStyleDestructive];

  [self.alertCoordinator addItemWithTitle:l10n_util::GetNSString(IDS_APP_CANCEL)
                                   action:^{
                                     // TODO(crbug.com/691979): Add metrics.
                                   }
                                    style:UIAlertActionStyleCancel];

  [self.alertCoordinator start];
}

- (void)displayContextMenuForMostVisitedItem:(CollectionViewItem*)item
                                     atPoint:(CGPoint)touchLocation
                                 atIndexPath:(NSIndexPath*)indexPath {
  ContentSuggestionsMostVisitedItem* mostVisitedItem =
      base::mac::ObjCCastStrict<ContentSuggestionsMostVisitedItem>(item);
  self.alertCoordinator = [[ActionSheetCoordinator alloc]
      initWithBaseViewController:self.navigationController
                           title:nil
                         message:nil
                            rect:CGRectMake(touchLocation.x, touchLocation.y, 0,
                                            0)
                            view:self.suggestionsViewController.collectionView];

  __weak ContentSuggestionsCoordinator* weakSelf = self;
  __weak ContentSuggestionsMostVisitedItem* weakItem = mostVisitedItem;

  [self.alertCoordinator
      addItemWithTitle:l10n_util::GetNSStringWithFixup(
                           IDS_IOS_CONTENT_CONTEXT_OPENLINKNEWTAB)
                action:^{
                  ContentSuggestionsCoordinator* strongSelf = weakSelf;
                  ContentSuggestionsMostVisitedItem* strongItem = weakItem;
                  if (!strongSelf || !strongItem)
                    return;
                  [strongSelf logMostVisitedOpening:strongItem
                                            atIndex:indexPath.item];
                  [strongSelf openNewTabWithURL:strongItem.URL incognito:NO];
                }
                 style:UIAlertActionStyleDefault];

  if (!self.browserState->IsOffTheRecord()) {
    [self.alertCoordinator
        addItemWithTitle:l10n_util::GetNSStringWithFixup(
                             IDS_IOS_CONTENT_CONTEXT_OPENLINKNEWINCOGNITOTAB)
                  action:^{
                    ContentSuggestionsCoordinator* strongSelf = weakSelf;
                    ContentSuggestionsMostVisitedItem* strongItem = weakItem;
                    if (!strongSelf || !strongItem)
                      return;
                    [strongSelf logMostVisitedOpening:strongItem
                                              atIndex:indexPath.item];
                    [strongSelf openNewTabWithURL:strongItem.URL incognito:YES];
                  }
                   style:UIAlertActionStyleDefault];
  }

  [self.alertCoordinator
      addItemWithTitle:l10n_util::GetNSStringWithFixup(
                           IDS_IOS_CONTENT_SUGGESTIONS_REMOVE)
                action:^{
                  ContentSuggestionsCoordinator* strongSelf = weakSelf;
                  ContentSuggestionsMostVisitedItem* strongItem = weakItem;
                  if (!strongSelf || !strongItem)
                    return;
                  base::RecordAction(
                      base::UserMetricsAction("MostVisited_UrlBlacklisted"));
                  [strongSelf.contentSuggestionsMediator
                      blacklistMostVisitedURL:strongItem.URL];
                  [strongSelf showMostVisitedUndoForURL:strongItem.URL];
                }
                 style:UIAlertActionStyleDestructive];

  [self.alertCoordinator addItemWithTitle:l10n_util::GetNSString(IDS_APP_CANCEL)
                                   action:nil
                                    style:UIAlertActionStyleCancel];

  [self.alertCoordinator start];
}

- (void)dismissContextMenu {
  [self.alertCoordinator stop];
  self.alertCoordinator = nil;
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
    [self.baseViewController chromeExecuteCommand:command];
    return;
  }
  NOTREACHED();
}

#pragma mark - Private

- (void)openNewTabWithURL:(const GURL&)URL incognito:(BOOL)incognito {
  // TODO(crbug.com/691979): Add metrics.

  [self.URLLoader webPageOrderedOpen:URL
                            referrer:web::Referrer()
                         inIncognito:incognito
                        inBackground:NO
                            appendTo:kCurrentTab];

  [self stop];
}

- (void)dismissArticle:(ContentSuggestionsItem*)article
           atIndexPath:(NSIndexPath*)indexPath {
  if (!article)
    return;

  // TODO(crbug.com/691979): Add metrics.
  [self.contentSuggestionsMediator
      dismissSuggestion:article.suggestionIdentifier];
  [self.suggestionsViewController dismissEntryAtIndexPath:indexPath];
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
