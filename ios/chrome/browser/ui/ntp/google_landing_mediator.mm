// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/ntp/google_landing_mediator.h"

#include "base/mac/bind_objc_block.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/cancelable_task_tracker.h"
#include "components/favicon/core/large_icon_service.h"
#include "components/favicon_base/fallback_icon_style.h"
#include "components/ntp_tiles/metrics.h"
#include "components/ntp_tiles/most_visited_sites.h"
#include "components/ntp_tiles/ntp_tile.h"
#include "components/ntp_tiles/ntp_tile_impression.h"
#include "components/rappor/rappor_service_impl.h"
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
#include "ios/chrome/browser/search_engines/template_url_service_factory.h"
#import "ios/chrome/browser/ui/browser_view_controller.h"
#include "ios/chrome/browser/ui/commands/browser_commands.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/ntp/google_landing_consumer.h"
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

using base::UserMetricsAction;

namespace {

// The What's New promo command that shows the Bookmarks Manager.
const char kBookmarkCommand[] = "bookmark";

// The What's New promo command that launches Rate This App.
const char kRateThisAppCommand[] = "ratethisapp";

const CGFloat kFaviconMinSize = 32;
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
  __weak GoogleLandingMediator* _owner;
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
  id<LogoVendor> _doodleController;

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

  // Most visited data from the MostVisitedSites service (copied upon receiving
  // the callback), not yet used by the collection. It will be used after a user
  // interaction.
  ntp_tiles::NTPTilesVector _freshMostVisitedData;

  // Most visited data used for logging the tiles impression. The data are
  // copied when receiving the first non-empty data. This copy is used to make
  // sure only the data received the first time are logged, and only once.
  ntp_tiles::NTPTilesVector _mostVisitedDataForLogging;

  // Observes the WebStateList so that this mediator can update the UI when the
  // active WebState changes.
  std::unique_ptr<WebStateListObserverBridge> _webStateListObserver;

  // What's new promo.
  std::unique_ptr<NotificationPromoWhatsNew> _notificationPromo;

  // Used to cancel tasks for the LargeIconService.
  base::CancelableTaskTracker _cancelable_task_tracker;
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
  [self.consumer
      setMaximumMostVisitedSitesShown:[GoogleLandingMediator maxSitesShown]];
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
  _observer.reset(
      new google_landing::SearchEngineObserver(self, _templateURLService));
  _templateURLService->Load();
  _doodleController = ios::GetChromeBrowserProvider()->CreateLogoVendor(
      _browserState, self.dispatcher);
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
  [defaultCenter addObserver:self.consumer
                    selector:@selector(locationBarBecomesFirstResponder)
                        name:kLocationBarBecomesFirstResponderNotification
                      object:nil];
  [defaultCenter addObserver:self.consumer
                    selector:@selector(locationBarResignsFirstResponder)
                        name:kLocationBarResignsFirstResponderNotification
                      object:nil];

  // Set up what's new.
  _notificationPromo.reset(
      new NotificationPromoWhatsNew(GetApplicationContext()->GetLocalState()));
  _notificationPromo->Init();
  [self.consumer setPromoText:[base::SysUTF8ToNSString(
                                  _notificationPromo->promo_text()) copy]];
  [self.consumer setPromoIcon:_notificationPromo->icon()];
  [self.consumer setPromoCanShow:_notificationPromo->CanShow()];
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
  // This is used by the content widget.
  ntp_tile_saver::SaveMostVisitedToDisk(
      data, self, app_group::ContentWidgetFaviconsFolder());

  if (_mostVisitedData.size() > 0) {
    // If some content is already displayed to the user, do not update it to
    // prevent updating the all the tiles without any action from the user.
    _freshMostVisitedData = data;
    return;
  }

  _mostVisitedData = data;
  [self.consumer mostVisitedDataUpdated];

  if (data.size() && !_recordedPageImpression) {
    _recordedPageImpression = YES;
    _mostVisitedDataForLogging = data;
    ntp_tiles::metrics::RecordPageImpression(data.size());
  }
}

- (void)onIconMadeAvailable:(const GURL&)siteUrl {
  ntp_tile_saver::UpdateSingleFavicon(siteUrl, self,
                                      app_group::ContentWidgetFaviconsFolder());
  for (size_t i = 0; i < _mostVisitedData.size(); ++i) {
    const ntp_tiles::NTPTile& ntpTile = _mostVisitedData[i];
    if (ntpTile.url == siteUrl) {
      [self.consumer mostVisitedIconMadeAvailableAtIndex:i];
      break;
    }
  }
}

- (void)getFaviconForURL:(const GURL&)URL
                    size:(CGFloat)size
                useCache:(BOOL)useCache
           imageCallback:(void (^)(UIImage* favicon))imageCallback
        fallbackCallback:(void (^)(UIColor* textColor,
                                   UIColor* backgroundColor,
                                   BOOL isDefaultColor))fallbackCallback {
  __weak GoogleLandingMediator* weakSelf = self;
  GURL localURL = URL;  // Persisting for use in block below.
  void (^faviconBlock)(const favicon_base::LargeIconResult&) = ^(
      const favicon_base::LargeIconResult& result) {
    ntp_tiles::TileVisualType tileType;

    if (result.bitmap.is_valid()) {
      scoped_refptr<base::RefCountedMemory> data =
          result.bitmap.bitmap_data.get();
      UIImage* favicon = [UIImage
          imageWithData:[NSData dataWithBytes:data->front() length:data->size()]
                  scale:[UIScreen mainScreen].scale];
      if (imageCallback) {
        imageCallback(favicon);
      }
      tileType = ntp_tiles::TileVisualType::ICON_REAL;
    } else if (result.fallback_icon_style) {
      UIColor* backgroundColor = skia::UIColorFromSkColor(
          result.fallback_icon_style->background_color);
      UIColor* textColor =
          skia::UIColorFromSkColor(result.fallback_icon_style->text_color);
      BOOL isDefaultColor =
          result.fallback_icon_style->is_default_background_color;
      if (fallbackCallback) {
        fallbackCallback(textColor, backgroundColor, isDefaultColor);
      }
      tileType = isDefaultColor ? ntp_tiles::TileVisualType::ICON_DEFAULT
                                : ntp_tiles::TileVisualType::ICON_COLOR;
    }

    GoogleLandingMediator* strongSelf = weakSelf;
    if (strongSelf) {
      if (result.bitmap.is_valid() || result.fallback_icon_style) {
        [strongSelf largeIconCache]->SetCachedResult(localURL, result);
      }
      [strongSelf faviconOfType:tileType fetchedForURL:localURL];
    }
  };

  if (useCache) {
    std::unique_ptr<favicon_base::LargeIconResult> cached_result =
        [self largeIconCache]->GetCachedResult(URL);
    if (cached_result) {
      faviconBlock(*cached_result);
    }
  }

  CGFloat faviconSize = [UIScreen mainScreen].scale * size;
  CGFloat faviconMinSize = [UIScreen mainScreen].scale * kFaviconMinSize;
  [self largeIconService]->GetLargeIconOrFallbackStyle(
      URL, faviconMinSize, faviconSize, base::BindBlockArc(faviconBlock),
      &_cancelable_task_tracker);
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
  ntp_tiles::metrics::RecordTileClick(ntp_tiles::NTPTileImpression(
      visitedIndex, tile.source, tile.title_source, tileType, GURL()));
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
  DCHECK(_notificationPromo);
  _notificationPromo->HandleViewed();
  [self.consumer setPromoCanShow:_notificationPromo->CanShow()];
}

// TODO(crbug.com/761096) : Promo handling should be DRY and tested.
- (void)promoTapped {
  DCHECK(_notificationPromo);
  _notificationPromo->HandleClosed();
  [self.consumer setPromoCanShow:_notificationPromo->CanShow()];

  if (_notificationPromo->IsURLPromo()) {
    [self.dispatcher webPageOrderedOpen:_notificationPromo->url()
                               referrer:web::Referrer()
                           inBackground:NO
                               appendTo:kCurrentTab];
    return;
  }

  if (_notificationPromo->IsChromeCommandPromo()) {
    std::string command = _notificationPromo->command();
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

#pragma mark - Private

// If there is some fresh most visited tiles, they become the current tiles and
// the consumer gets notified.
- (void)useFreshData {
  _mostVisitedData = _freshMostVisitedData;
  [self.consumer mostVisitedDataUpdated];
}

// If it is the first time we see the favicon corresponding to |URL|, we log the
// |tileType| impression.
- (void)faviconOfType:(ntp_tiles::TileVisualType)tileType
        fetchedForURL:(const GURL&)URL {
  for (size_t i = 0; i < _mostVisitedDataForLogging.size(); ++i) {
    ntp_tiles::NTPTile& ntpTile = _mostVisitedDataForLogging[i];
    if (ntpTile.url == URL) {
      ntp_tiles::metrics::RecordTileImpression(
          ntp_tiles::NTPTileImpression(i, ntpTile.source, ntpTile.title_source,
                                       tileType, URL),
          GetApplicationContext()->GetRapporServiceImpl());
      // Reset the URL to be sure to log the impression only once.
      ntpTile.url = GURL();
      break;
    }
  }
}

@end
