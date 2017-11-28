// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/ntp/google_landing_mediator.h"

#include "base/mac/bind_objc_block.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/cancelable_task_tracker.h"
#include "components/favicon/core/fallback_url_util.h"
#include "components/favicon/core/large_icon_service.h"
#include "components/favicon_base/fallback_icon_style.h"
#include "components/ntp_tiles/metrics.h"
#include "components/ntp_tiles/most_visited_sites.h"
#include "components/ntp_tiles/ntp_tile.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/template_url_service_observer.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/favicon/ios_chrome_large_icon_cache_factory.h"
#include "ios/chrome/browser/favicon/ios_chrome_large_icon_service_factory.h"
#include "ios/chrome/browser/favicon/large_icon_cache.h"
#import "ios/chrome/browser/metrics/new_tab_page_uma.h"
#include "ios/chrome/browser/ntp_tiles/ios_most_visited_sites_factory.h"
#import "ios/chrome/browser/ntp_tiles/most_visited_sites_observer_bridge.h"
#include "ios/chrome/browser/reading_list/reading_list_model_factory.h"
#import "ios/chrome/browser/search_engines/search_engine_observer_bridge.h"
#include "ios/chrome/browser/search_engines/template_url_service_factory.h"
#import "ios/chrome/browser/ui/browser_view_controller.h"
#include "ios/chrome/browser/ui/commands/browser_commands.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/favicon/favicon_attributes_with_payload.h"
#import "ios/chrome/browser/ui/location_bar_notification_names.h"
#import "ios/chrome/browser/ui/ntp/google_landing_consumer.h"
#include "ios/chrome/browser/ui/ntp/metrics.h"
#import "ios/chrome/browser/ui/ntp/notification_promo_whats_new.h"
#include "ios/chrome/browser/ui/ntp/ntp_tile_saver.h"
#import "ios/chrome/browser/ui/url_loader.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_list_observer_bridge.h"
#include "ios/chrome/common/app_group/app_group_constants.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#include "ios/public/provider/chrome/browser/voice/voice_search_provider.h"
#include "ios/web/public/web_state/web_state.h"
#include "skia/ext/skia_utils_ios.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface GoogleLandingMediator ()<SearchEngineObserving,
                                    WebStateListObserving> {
  // The ChromeBrowserState associated with this mediator.
  ios::ChromeBrowserState* _browserState;  // Weak.

  // Controller to fetch and show doodles or a default Google logo.
  id<LogoVendor> _doodleController;

  // Listen for default search engine changes.
  std::unique_ptr<SearchEngineObserverBridge> _observer;
  TemplateURLService* _templateURLService;  // weak

  // Observes the WebStateList so that this mediator can update the UI when the
  // active WebState changes.
  std::unique_ptr<WebStateListObserverBridge> _webStateListObserver;
}

// The WebStateList that is being observed by this mediator.
@property(nonatomic, assign, readonly) WebStateList* webStateList;

@end

@implementation GoogleLandingMediator

@synthesize webStateList = _webStateList;
@synthesize consumer = _consumer;
@synthesize dispatcher = _dispatcher;

- (instancetype)initWithBrowserState:(ios::ChromeBrowserState*)browserState
                        webStateList:(WebStateList*)webStateList {
  self = [super init];
  if (self) {
    _browserState = browserState;
    _webStateList = webStateList;

    _webStateListObserver = base::MakeUnique<WebStateListObserverBridge>(self);
    _webStateList->AddObserver(_webStateListObserver.get());
  }
  return self;
}

- (void)shutdown {
  _webStateList->RemoveObserver(_webStateListObserver.get());
  [[NSNotificationCenter defaultCenter] removeObserver:self.consumer];
  _observer.reset();
}

- (void)setUp {
  [self.consumer setVoiceSearchIsEnabled:ios::GetChromeBrowserProvider()
                                             ->GetVoiceSearchProvider()
                                             ->IsVoiceSearchEnabled()];
  [self.consumer setTabCount:self.webStateList->count()];
  web::WebState* webState = _webStateList->GetActiveWebState();
  if (webState) {
    web::NavigationManager* nav = webState->GetNavigationManager();
    [self.consumer setCanGoForward:nav->CanGoForward()];
    [self.consumer setCanGoBack:nav->CanGoBack()];
  }

  // Set up template URL service to listen for default search engine changes.
  _templateURLService =
      ios::TemplateURLServiceFactory::GetForBrowserState(_browserState);
  _observer =
      std::make_unique<SearchEngineObserverBridge>(self, _templateURLService);
  _templateURLService->Load();
  _doodleController = ios::GetChromeBrowserProvider()->CreateLogoVendor(
      _browserState, self.dispatcher);
  [_consumer setLogoVendor:_doodleController];
  [self searchEngineChanged];

  // Set up notifications;
  NSNotificationCenter* defaultCenter = [NSNotificationCenter defaultCenter];
  [defaultCenter addObserver:self.consumer
                    selector:@selector(locationBarBecomesFirstResponder)
                        name:kLocationBarBecomesFirstResponderNotification
                      object:nil];
  [defaultCenter addObserver:self.consumer
                    selector:@selector(locationBarResignsFirstResponder)
                        name:kLocationBarResignsFirstResponderNotification
                      object:nil];
}

- (void)searchEngineChanged {
  BOOL showLogo = NO;
  const TemplateURL* defaultURL =
      _templateURLService->GetDefaultSearchProvider();
  if (defaultURL) {
    showLogo =
        defaultURL->GetEngineType(_templateURLService->search_terms_data()) ==
        SEARCH_ENGINE_GOOGLE;
  }
  [self.consumer setLogoIsShowing:showLogo];
}

#pragma mark - WebStateListObserving

- (void)webStateList:(WebStateList*)webStateList
    didInsertWebState:(web::WebState*)webState
              atIndex:(int)index
           activating:(BOOL)activating {
  [self.consumer setTabCount:self.webStateList->count()];
}

- (void)webStateList:(WebStateList*)webStateList
    didDetachWebState:(web::WebState*)webState
              atIndex:(int)atIndex {
  [self.consumer setTabCount:self.webStateList->count()];
}

// If the actual webState associated with this mediator were passed in, this
// would not be necessary.  However, since the active webstate can change when
// the new tab page is created (and animated in), listen for changes here and
// always display what's active.
- (void)webStateList:(WebStateList*)webStateList
    didChangeActiveWebState:(web::WebState*)newWebState
                oldWebState:(web::WebState*)oldWebState
                    atIndex:(int)atIndex
                 userAction:(BOOL)userAction {
  if (newWebState) {
    web::NavigationManager* nav = newWebState->GetNavigationManager();
    [self.consumer setCanGoForward:nav->CanGoForward()];
    [self.consumer setCanGoBack:nav->CanGoBack()];
  }
}

@end
