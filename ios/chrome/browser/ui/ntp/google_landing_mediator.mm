// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/ntp/google_landing_mediator.h"

#import "base/ios/weak_nsobject.h"
#include "base/mac/scoped_nsobject.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/sys_string_conversions.h"
#include "components/ntp_tiles/metrics.h"
#include "components/ntp_tiles/most_visited_sites.h"
#include "components/ntp_tiles/ntp_tile.h"
#include "components/rappor/rappor_service_impl.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/template_url_service_observer.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/favicon/ios_chrome_large_icon_cache_factory.h"
#include "ios/chrome/browser/favicon/ios_chrome_large_icon_service_factory.h"
#import "ios/chrome/browser/metrics/new_tab_page_uma.h"
#include "ios/chrome/browser/ntp_tiles/ios_most_visited_sites_factory.h"
#import "ios/chrome/browser/ntp_tiles/most_visited_sites_observer_bridge.h"
#include "ios/chrome/browser/reading_list/reading_list_model_factory.h"
#include "ios/chrome/browser/search_engines/template_url_service_factory.h"
#import "ios/chrome/browser/ui/browser_view_controller.h"
#import "ios/chrome/browser/ui/commands/UIKit+ChromeExecuteCommand.h"
#import "ios/chrome/browser/ui/commands/generic_chrome_command.h"
#import "ios/chrome/browser/ui/ntp/google_landing_consumer.h"
#import "ios/chrome/browser/ui/ntp/notification_promo_whats_new.h"
#import "ios/chrome/browser/ui/toolbar/web_toolbar_controller.h"
#import "ios/chrome/browser/ui/url_loader.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_list_observer_bridge.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#include "ios/public/provider/chrome/browser/voice/voice_search_provider.h"
#import "ios/shared/chrome/browser/ui/commands/command_dispatcher.h"
#include "ios/web/public/web_state/web_state.h"

using base::UserMetricsAction;

namespace {

const NSInteger kMaxNumMostVisitedFavicons = 8;

}  // namespace

@interface GoogleLandingMediator (UsedBySearchEngineObserver)
// Check to see if the logo visibility should change.
- (void)updateShowLogo;
@end

namespace google_landing {

// Observer used to hide the Google logo and doodle if the TemplateURLService
// changes.
class SearchEngineObserver : public TemplateURLServiceObserver {
 public:
  SearchEngineObserver(GoogleLandingMediator* owner,
                       TemplateURLService* urlService);
  ~SearchEngineObserver() override;
  void OnTemplateURLServiceChanged() override;

 private:
  base::WeakNSObject<GoogleLandingMediator> _owner;
  TemplateURLService* _templateURLService;  // weak
};

SearchEngineObserver::SearchEngineObserver(GoogleLandingMediator* owner,
                                           TemplateURLService* urlService)
    : _owner(owner), _templateURLService(urlService) {
  _templateURLService->AddObserver(this);
}

SearchEngineObserver::~SearchEngineObserver() {
  _templateURLService->RemoveObserver(this);
}

void SearchEngineObserver::OnTemplateURLServiceChanged() {
  [_owner updateShowLogo];
}

}  // namespace google_landing

@interface GoogleLandingMediator ()<GoogleLandingDataSource,
                                    MostVisitedSitesObserving,
                                    WebStateListObserving> {
  // The ChromeBrowserState associated with this mediator.
  ios::ChromeBrowserState* _browserState;  // Weak.

  // |YES| if impressions were logged already and shouldn't be logged again.
  BOOL _recordedPageImpression;

  // Controller to fetch and show doodles or a default Google logo.
  base::scoped_nsprotocol<id<LogoVendor>> _doodleController;

  // Listen for default search engine changes.
  std::unique_ptr<google_landing::SearchEngineObserver> _observer;
  TemplateURLService* _templateURLService;  // weak

  // A MostVisitedSites::Observer bridge object to get notified of most visited
  // sites changes.
  std::unique_ptr<ntp_tiles::MostVisitedSitesObserverBridge>
      _mostVisitedObserverBridge;

  std::unique_ptr<ntp_tiles::MostVisitedSites> _mostVisitedSites;

  // Most visited data from the MostVisitedSites service currently in use.
  ntp_tiles::NTPTilesVector _mostVisitedData;

  // Observes the WebStateList so that this mediator can update the UI when the
  // active WebState changes.
  std::unique_ptr<WebStateListObserverBridge> _webStateListObserver;

  // What's new promo.
  std::unique_ptr<NotificationPromoWhatsNew> _notification_promo;
}

// Consumer to handle google landing update notifications.
@property(nonatomic) id<GoogleLandingConsumer> consumer;

// The WebStateList that is being observed by this mediator.
@property(nonatomic, assign) WebStateList* webStateList;

// The dispatcher for this mediator.
@property(nonatomic, assign) id<ChromeExecuteCommand, UrlLoader> dispatcher;

// Most visited data from the MostVisitedSites service (copied upon receiving
// the callback), not yet used.
@property(nonatomic, assign) ntp_tiles::NTPTilesVector freshMostVisitedData;

// Perform initial setup.
- (void)setUp;

// If there is some fresh most visited tiles, they become the current tiles and
// the consumer gets notified.
- (void)useFreshData;

@end

@implementation GoogleLandingMediator

@synthesize consumer = _consumer;
@synthesize dispatcher = _dispatcher;
@synthesize webStateList = _webStateList;
@synthesize freshMostVisitedData = _freshMostVisitedData;

- (instancetype)initWithConsumer:(id<GoogleLandingConsumer>)consumer
                    browserState:(ios::ChromeBrowserState*)browserState
                      dispatcher:(id<ChromeExecuteCommand, UrlLoader>)dispatcher
                    webStateList:(WebStateList*)webStateList {
  self = [super init];
  if (self) {
    _consumer = consumer;
    _browserState = browserState;
    _dispatcher = dispatcher;
    _webStateList = webStateList;

    _webStateListObserver = base::MakeUnique<WebStateListObserverBridge>(self);
    _webStateList->AddObserver(_webStateListObserver.get());

    [self setUp];
  }
  return self;
}

- (void)shutdown {
  _webStateList->RemoveObserver(_webStateListObserver.get());
  [[NSNotificationCenter defaultCenter] removeObserver:self.consumer];
}

- (void)setUp {
  [_consumer setVoiceSearchIsEnabled:ios::GetChromeBrowserProvider()
                                         ->GetVoiceSearchProvider()
                                         ->IsVoiceSearchEnabled()];
  [_consumer
      setMaximumMostVisitedSitesShown:[GoogleLandingMediator maxSitesShown]];
  [_consumer setTabCount:self.webStateList->count()];
  web::WebState* webState = _webStateList->GetActiveWebState();
  if (webState) {
    web::NavigationManager* nav = webState->GetNavigationManager();
    [_consumer setCanGoForward:nav->CanGoForward()];
    [_consumer setCanGoBack:nav->CanGoBack()];
  }

  // Set up template URL service to listen for default search engine changes.
  _templateURLService =
      ios::TemplateURLServiceFactory::GetForBrowserState(_browserState);
  _observer.reset(
      new google_landing::SearchEngineObserver(self, _templateURLService));
  _templateURLService->Load();
  _doodleController.reset(ios::GetChromeBrowserProvider()->CreateLogoVendor(
      _browserState, self.dispatcher));
  [_consumer setLogoVendor:_doodleController];
  [self updateShowLogo];

  // Set up most visited sites.  This call may have the side effect of
  // triggering -onMostVisitedURLsAvailable immediately, which can load the
  // view before dataSource is set.
  _mostVisitedSites =
      IOSMostVisitedSitesFactory::NewForBrowserState(_browserState);
  _mostVisitedObserverBridge.reset(
      new ntp_tiles::MostVisitedSitesObserverBridge(self));
  _mostVisitedSites->SetMostVisitedURLsObserver(
      _mostVisitedObserverBridge.get(), [GoogleLandingMediator maxSitesShown]);

  // Set up notifications;
  NSNotificationCenter* defaultCenter = [NSNotificationCenter defaultCenter];
  [defaultCenter
      addObserver:_consumer
         selector:@selector(locationBarBecomesFirstResponder)
             name:ios_internal::kLocationBarBecomesFirstResponderNotification
           object:nil];
  [defaultCenter
      addObserver:_consumer
         selector:@selector(locationBarResignsFirstResponder)
             name:ios_internal::kLocationBarResignsFirstResponderNotification
           object:nil];

  // Set up what's new.
  _notification_promo.reset(
      new NotificationPromoWhatsNew(GetApplicationContext()->GetLocalState()));
  _notification_promo->Init();
  [_consumer setPromoText:[base::SysUTF8ToNSString(
                              _notification_promo->promo_text()) copy]];
  [_consumer setPromoIcon:_notification_promo->icon()];
  [_consumer setPromoCanShow:_notification_promo->CanShow()];
}

- (void)updateShowLogo {
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

+ (NSUInteger)maxSitesShown {
  return kMaxNumMostVisitedFavicons;
}

#pragma mark - MostVisitedSitesObserving

- (void)onMostVisitedURLsAvailable:(const ntp_tiles::NTPTilesVector&)data {
  if (_mostVisitedData.size() > 0) {
    // If some content is already displayed to the user, do not update it to
    // prevent updating the all the tiles without any action from the user.
    self.freshMostVisitedData = data;
    return;
  }

  _mostVisitedData = data;
  [self.consumer mostVisitedDataUpdated];

  if (data.size() && !_recordedPageImpression) {
    _recordedPageImpression = YES;
    std::vector<ntp_tiles::metrics::TileImpression> tiles;
    for (const ntp_tiles::NTPTile& ntpTile : data) {
      tiles.emplace_back(ntpTile.source, ntp_tiles::UNKNOWN_TILE_TYPE,
                         ntpTile.url);
    }
    ntp_tiles::metrics::RecordPageImpression(
        tiles, GetApplicationContext()->GetRapporServiceImpl());
  }
}

- (void)onIconMadeAvailable:(const GURL&)siteUrl {
  for (size_t i = 0; i < _mostVisitedData.size(); ++i) {
    const ntp_tiles::NTPTile& ntpTile = _mostVisitedData[i];
    if (ntpTile.url == siteUrl) {
      [self.consumer mostVisitedIconMadeAvailableAtIndex:i];
      break;
    }
  }
}

#pragma mark - WebStateListObserving

- (void)webStateList:(WebStateList*)webStateList
    didInsertWebState:(web::WebState*)webState
              atIndex:(int)index {
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

#pragma mark - GoogleLandingDataSource

- (void)addBlacklistedURL:(const GURL&)url {
  _mostVisitedSites->AddOrRemoveBlacklistedUrl(url, true);
  [self useFreshData];
}

- (void)removeBlacklistedURL:(const GURL&)url {
  _mostVisitedSites->AddOrRemoveBlacklistedUrl(url, false);
  [self useFreshData];
}

- (ntp_tiles::NTPTile)mostVisitedAtIndex:(NSUInteger)index {
  return _mostVisitedData[index];
}

- (NSUInteger)mostVisitedSize {
  return _mostVisitedData.size();
}

- (void)logMostVisitedClick:(const NSUInteger)visitedIndex
                   tileType:(ntp_tiles::TileVisualType)tileType {
  new_tab_page_uma::RecordAction(
      _browserState, new_tab_page_uma::ACTION_OPENED_MOST_VISITED_ENTRY);
  base::RecordAction(UserMetricsAction("MobileNTPMostVisited"));
  const ntp_tiles::NTPTile& tile = _mostVisitedData[visitedIndex];
  ntp_tiles::metrics::RecordTileClick(visitedIndex, tile.source, tileType);
}

- (ReadingListModel*)readingListModel {
  return ReadingListModelFactory::GetForBrowserState(_browserState);
}

- (LargeIconCache*)largeIconCache {
  return IOSChromeLargeIconCacheFactory::GetForBrowserState(_browserState);
}

- (favicon::LargeIconService*)largeIconService {
  return IOSChromeLargeIconServiceFactory::GetForBrowserState(_browserState);
}

- (void)promoViewed {
  DCHECK(_notification_promo);
  _notification_promo->HandleViewed();
  [self.consumer setPromoCanShow:_notification_promo->CanShow()];
}

- (void)promoTapped {
  DCHECK(_notification_promo);
  _notification_promo->HandleClosed();
  [self.consumer setPromoCanShow:_notification_promo->CanShow()];

  if (_notification_promo->IsURLPromo()) {
    [self.dispatcher webPageOrderedOpen:_notification_promo->url()
                               referrer:web::Referrer()
                           inBackground:NO
                               appendTo:kCurrentTab];
    return;
  }

  if (_notification_promo->IsChromeCommand()) {
    base::scoped_nsobject<GenericChromeCommand> command(
        [[GenericChromeCommand alloc]
            initWithTag:_notification_promo->command_id()]);
    [self.dispatcher chromeExecuteCommand:command];
    return;
  }
  NOTREACHED();
}

#pragma mark - Private

- (void)useFreshData {
  _mostVisitedData = self.freshMostVisitedData;
  [self.consumer mostVisitedDataUpdated];
}

@end
