// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/ntp/ntp_home_coordinator.h"

#include "base/mac/foundation_util.h"
#include "components/ntp_snippets/content_suggestions_service.h"
#include "components/ntp_snippets/remote/remote_suggestions_scheduler.h"
#include "components/ntp_tiles/most_visited_sites.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/content_suggestions/content_suggestions_mediator.h"
#import "ios/chrome/browser/content_suggestions/content_suggestions_metrics_recorder.h"
#import "ios/chrome/browser/content_suggestions/ntp_home_mediator.h"
#import "ios/chrome/browser/content_suggestions/ntp_home_metrics.h"
#include "ios/chrome/browser/favicon/ios_chrome_large_icon_cache_factory.h"
#include "ios/chrome/browser/favicon/ios_chrome_large_icon_service_factory.h"
#include "ios/chrome/browser/favicon/large_icon_cache.h"
#include "ios/chrome/browser/ntp_snippets/ios_chrome_content_suggestions_service_factory.h"
#include "ios/chrome/browser/ntp_tiles/ios_most_visited_sites_factory.h"
#include "ios/chrome/browser/reading_list/reading_list_model_factory.h"
#import "ios/chrome/browser/ui/browser_list/browser.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_header_synchronizer.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_view_controller.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_view_controller_audience.h"
#import "ios/chrome/browser/ui/content_suggestions/ntp_home_constant.h"
#import "ios/chrome/browser/ui/coordinators/browser_coordinator+internal.h"
#import "ios/chrome/browser/ui/ntp/google_landing_consumer.h"
#import "ios/chrome/browser/ui/ntp/google_landing_mediator.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_header_constants.h"
#import "ios/chrome/browser/ui/ntp/notification_promo_whats_new.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/clean/chrome/browser/ui/adaptor/browser_commands_adaptor.h"
#import "ios/clean/chrome/browser/ui/adaptor/url_loader_adaptor.h"
#import "ios/clean/chrome/browser/ui/ntp/ntp_home_header_coordinator.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface NTPHomeCoordinator ()<
    ContentSuggestionsViewControllerAudience>

@property(nonatomic, strong) GoogleLandingMediator* googleLandingMediator;
@property(nonatomic, strong) ContentSuggestionsMediator* suggestionsMediator;
@property(nonatomic, strong) NTPHomeHeaderCoordinator* headerCoordinator;
@property(nonatomic, strong) ContentSuggestionsViewController* viewController;
@property(nonatomic, strong)
    ContentSuggestionsHeaderSynchronizer* headerCollectionInteractionHandler;
@property(nonatomic, strong) ContentSuggestionsMetricsRecorder* metricsRecorder;
@property(nonatomic, strong) NTPHomeMediator* NTPMediator;

@property(nonatomic, strong) CommandDispatcher* router;
@property(nonatomic, strong) URLLoaderAdaptor* URLAdaptor;
@property(nonatomic, strong) BrowserCommandsAdaptor* browserCommandAdaptor;

@end

@implementation NTPHomeCoordinator

@synthesize headerCoordinator = _headerCoordinator;
@synthesize googleLandingMediator = _googleLandingMediator;
@synthesize viewController = _viewController;
@synthesize suggestionsMediator = _suggestionsMediator;
@synthesize headerCollectionInteractionHandler =
    _headerCollectionInteractionHandler;
@synthesize metricsRecorder = _metricsRecorder;
@synthesize NTPMediator = _NTPMediator;

@synthesize router = _router;
@synthesize URLAdaptor = _URLAdaptor;
@synthesize browserCommandAdaptor = _browserCommandAdaptor;

#pragma mark - BrowserCoordinator

- (void)start {
  ntp_snippets::ContentSuggestionsService* contentSuggestionsService =
      IOSChromeContentSuggestionsServiceFactory::GetForBrowserState(
          self.browser->browser_state());
  contentSuggestionsService->remote_suggestions_scheduler()
      ->OnSuggestionsSurfaceOpened();

  self.NTPMediator = [[NTPHomeMediator alloc] init];

  self.viewController = [[ContentSuggestionsViewController alloc]
      initWithStyle:CollectionViewControllerStyleDefault];

  self.URLAdaptor = [[URLLoaderAdaptor alloc] init];
  self.URLAdaptor.viewControllerForAlert = self.viewController;
  self.browserCommandAdaptor = [[BrowserCommandsAdaptor alloc] init];
  self.browserCommandAdaptor.viewControllerForAlert = self.viewController;
  self.router = [[CommandDispatcher alloc] init];
  [self.router startDispatchingToTarget:self.URLAdaptor
                            forProtocol:@protocol(UrlLoader)];
  [self.router startDispatchingToTarget:self.browserCommandAdaptor
                            forProtocol:@protocol(BrowserCommands)];
  id<BrowserCommands, UrlLoader> callableRouter =
      static_cast<id<BrowserCommands, UrlLoader>>(self.router);

  self.googleLandingMediator = [[GoogleLandingMediator alloc]
      initWithBrowserState:self.browser->browser_state()
              webStateList:&self.browser->web_state_list()];
  self.googleLandingMediator.dispatcher = callableRouter;

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
  self.suggestionsMediator.commandHandler = self.NTPMediator;

  self.metricsRecorder = [[ContentSuggestionsMetricsRecorder alloc] init];
  self.metricsRecorder.delegate = self.suggestionsMediator;

  [self.viewController setDataSource:self.suggestionsMediator];
  self.viewController.suggestionCommandHandler = self.NTPMediator;
  self.viewController.audience = self;
  self.viewController.metricsRecorder = self.metricsRecorder;
  self.viewController.containsToolbar = NO;

  self.NTPMediator.webState =
      self.browser->web_state_list().GetActiveWebState();
  self.NTPMediator.dispatcher = callableRouter;
  self.NTPMediator.NTPMetrics = [[NTPHomeMetrics alloc]
      initWithBrowserState:self.browser->browser_state()];
  self.NTPMediator.metricsRecorder = self.metricsRecorder;
  self.NTPMediator.suggestionsViewController = self.viewController;
  self.NTPMediator.suggestionsMediator = self.suggestionsMediator;
  self.NTPMediator.suggestionsService = contentSuggestionsService;
  [self.NTPMediator setUp];

  self.headerCoordinator = [[NTPHomeHeaderCoordinator alloc] init];
  self.headerCoordinator.delegate = self.NTPMediator;
  self.headerCoordinator.commandHandler = self.NTPMediator;
  [self addChildCoordinator:self.headerCoordinator];
  [self.headerCoordinator start];

  self.googleLandingMediator.consumer = self.headerCoordinator.consumer;
  [self.googleLandingMediator setUp];

  self.suggestionsMediator.headerProvider =
      self.headerCoordinator.headerProvider;

  [super start];
}

- (void)childCoordinatorDidStart:(BrowserCoordinator*)childCoordinator {
  DCHECK(childCoordinator == self.headerCoordinator);

  [self.viewController
      addChildViewController:self.headerCoordinator.viewController];
  [self.headerCoordinator.viewController
      didMoveToParentViewController:self.viewController];

  self.headerCollectionInteractionHandler =
      [[ContentSuggestionsHeaderSynchronizer alloc]
          initWithCollectionController:self.viewController
                      headerController:self.headerCoordinator.headerController];
}

- (void)stop {
  [super stop];
  [self.googleLandingMediator shutdown];
  [self.headerCoordinator stop];
  self.viewController = nil;
  self.suggestionsMediator = nil;
  self.googleLandingMediator = nil;
  self.headerCoordinator = nil;
  [self.router stopDispatchingToTarget:self.URLAdaptor];
  [self.router stopDispatchingToTarget:self.browserCommandAdaptor];
  self.router = nil;
  self.URLAdaptor = nil;
  self.browserCommandAdaptor = nil;
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

@end
