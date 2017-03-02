// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/ntp/google_landing_controller.h"

#include <algorithm>

#include "base/i18n/case_conversion.h"
#import "base/ios/weak_nsobject.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/mac/bind_objc_block.h"
#include "base/mac/foundation_util.h"
#include "base/mac/scoped_nsobject.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/sys_string_conversions.h"
#include "components/favicon/core/large_icon_service.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/ntp_tiles/most_visited_sites.h"
#include "components/ntp_tiles/ntp_tile.h"
#include "components/rappor/rappor_service_impl.h"
#include "components/reading_list/core/reading_list_switches.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/template_url_service_observer.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/favicon/favicon_loader.h"
#include "ios/chrome/browser/favicon/favicon_service_factory.h"
#include "ios/chrome/browser/favicon/ios_chrome_favicon_loader_factory.h"
#include "ios/chrome/browser/favicon/large_icon_cache.h"
#import "ios/chrome/browser/metrics/new_tab_page_uma.h"
#include "ios/chrome/browser/notification_promo.h"
#include "ios/chrome/browser/ntp_tiles/ios_most_visited_sites_factory.h"
#include "ios/chrome/browser/reading_list/reading_list_model_factory.h"
#include "ios/chrome/browser/search_engines/template_url_service_factory.h"
#include "ios/chrome/browser/suggestions/suggestions_service_factory.h"
#import "ios/chrome/browser/tabs/tab_model.h"
#import "ios/chrome/browser/ui/browser_view_controller.h"
#import "ios/chrome/browser/ui/commands/UIKit+ChromeExecuteCommand.h"
#import "ios/chrome/browser/ui/commands/generic_chrome_command.h"
#include "ios/chrome/browser/ui/commands/ios_command_ids.h"
#import "ios/chrome/browser/ui/context_menu/context_menu_coordinator.h"
#import "ios/chrome/browser/ui/ntp/most_visited_cell.h"
#import "ios/chrome/browser/ui/ntp/most_visited_layout.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_header_constants.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_header_view.h"
#import "ios/chrome/browser/ui/ntp/notification_promo_whats_new.h"
#import "ios/chrome/browser/ui/ntp/whats_new_header_view.h"
#import "ios/chrome/browser/ui/orientation_limiting_navigation_controller.h"
#import "ios/chrome/browser/ui/overscroll_actions/overscroll_actions_controller.h"
#import "ios/chrome/browser/ui/toolbar/toolbar_owner.h"
#import "ios/chrome/browser/ui/toolbar/web_toolbar_controller.h"
#include "ios/chrome/browser/ui/ui_util.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"
#import "ios/chrome/browser/ui/url_loader.h"
#include "ios/chrome/common/string_util.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#include "ios/public/provider/chrome/browser/ui/logo_vendor.h"
#include "ios/public/provider/chrome/browser/voice/voice_search_provider.h"
#import "ios/third_party/material_components_ios/src/components/Snackbar/src/MaterialSnackbar.h"
#import "ios/third_party/material_components_ios/src/components/Typography/src/MaterialTypography.h"
#include "ios/web/public/referrer.h"
#import "ios/web/public/web_state/context_menu_params.h"
#import "net/base/mac/url_conversions.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "ui/base/l10n/l10n_util.h"

using base::UserMetricsAction;

namespace {

enum {
  SectionWithOmnibox,
  SectionWithMostVisited,
  NumberOfCollectionViewSections,
};

enum InterfaceOrientation {
  ALL,
  IPHONE_LANDSCAPE,
};

const CGFloat kVoiceSearchButtonWidth = 48;
const UIEdgeInsets kSearchBoxStretchInsets = {3, 3, 3, 3};

// Height for the doodle frame when Google is not the default search engine.
const CGFloat kNonGoogleSearchDoodleHeight = 60;
// Height for the header view on tablet when Google is not the default search
// engine.
const CGFloat kNonGoogleSearchHeaderHeightIPad = 10;

const CGFloat kHintLabelSidePadding = 12;
const CGFloat kNTPSearchFieldBottomPadding = 16;
const CGFloat kWhatsNewHeaderHiddenHeight = 8;
const CGFloat kDoodleTopMarginIPadPortrait = 82;
const CGFloat kDoodleTopMarginIPadLandscape = 82;
const NSInteger kMaxNumMostVisitedFavicons = 8;
const NSInteger kMaxNumMostVisitedFaviconRows = 2;
const CGFloat kMaxSearchFieldFrameMargin = 200;
const CGFloat kShiftTilesDownAnimationDuration = 0.2;

const CGFloat kMostVisitedPaddingIPhone = 16;
const CGFloat kMostVisitedPaddingIPadFavicon = 24;

}  // namespace

@interface GoogleLandingController ()
- (void)onMostVisitedURLsAvailable:(const ntp_tiles::NTPTilesVector&)data;
- (void)onIconMadeAvailable:(const GURL&)siteUrl;
@end

namespace google_landing {

// MostVisitedSitesObserverBridge allow registration as a
// MostVisitedSites::Observer.
class MostVisitedSitesObserverBridge
    : public ntp_tiles::MostVisitedSites::Observer {
 public:
  MostVisitedSitesObserverBridge(GoogleLandingController* owner);
  ~MostVisitedSitesObserverBridge() override;

  // MostVisitedSites::Observer implementation.
  void OnMostVisitedURLsAvailable(
      const ntp_tiles::NTPTilesVector& most_visited) override;
  void OnIconMadeAvailable(const GURL& site_url) override;

 private:
  GoogleLandingController* _owner;
};

MostVisitedSitesObserverBridge::MostVisitedSitesObserverBridge(
    GoogleLandingController* owner)
    : _owner(owner) {}

MostVisitedSitesObserverBridge::~MostVisitedSitesObserverBridge() {}

void MostVisitedSitesObserverBridge::OnMostVisitedURLsAvailable(
    const ntp_tiles::NTPTilesVector& tiles) {
  [_owner onMostVisitedURLsAvailable:tiles];
}

void MostVisitedSitesObserverBridge::OnIconMadeAvailable(const GURL& site_url) {
  [_owner onIconMadeAvailable:site_url];
}

// Observer used to hide the Google logo and doodle if the TemplateURLService
// changes.
class SearchEngineObserver : public TemplateURLServiceObserver {
 public:
  SearchEngineObserver(GoogleLandingController* owner,
                       TemplateURLService* urlService);
  ~SearchEngineObserver() override;
  void OnTemplateURLServiceChanged() override;

 private:
  base::WeakNSObject<GoogleLandingController> _owner;
  TemplateURLService* _templateURLService;  // weak
};

SearchEngineObserver::SearchEngineObserver(GoogleLandingController* owner,
                                           TemplateURLService* urlService)
    : _owner(owner), _templateURLService(urlService) {
  _templateURLService->AddObserver(this);
}

SearchEngineObserver::~SearchEngineObserver() {
  _templateURLService->RemoveObserver(this);
}

void SearchEngineObserver::OnTemplateURLServiceChanged() {
  [_owner reload];
}

}  // namespace google_landing

@interface GoogleLandingController (UsedByGoogleLandingView)
// Update frames for subviews depending on the interface orientation.
- (void)updateSubviewFrames;
// Resets the collection view's inset to 0.
- (void)resetSectionInset;
- (void)reloadData;
@end

// Subclassing the main UIScrollView allows calls for setFrame.
@interface GoogleLandingView : UIView {
  GoogleLandingController* _googleLanding;
}

- (void)setFrameDelegate:(GoogleLandingController*)delegate;

@end

@implementation GoogleLandingView

- (void)layoutSubviews {
  [super layoutSubviews];
  [_googleLanding updateSubviewFrames];
}

- (void)setFrameDelegate:(GoogleLandingController*)delegate {
  _googleLanding = delegate;
}

- (void)setFrame:(CGRect)frame {
  // On iPad and in fullscreen, the collection view's inset is very large.
  // When Chrome enters slide over mode, the previously set inset is larger than
  // the newly set collection view's width, which makes the collection view
  // throw an exception.
  // To prevent this from happening, we reset the inset to 0 before changing the
  // frame.
  [_googleLanding resetSectionInset];
  [super setFrame:frame];
  [_googleLanding updateSubviewFrames];
  [_googleLanding reloadData];
}

@end

@interface GoogleLandingController ()<OverscrollActionsControllerDelegate,
                                      UICollectionViewDataSource,
                                      UICollectionViewDelegate,
                                      UICollectionViewDelegateFlowLayout,
                                      UIGestureRecognizerDelegate,
                                      WhatsNewHeaderViewDelegate> {
  // The main view.
  base::scoped_nsobject<GoogleLandingView> _view;

  // Fake omnibox.
  base::scoped_nsobject<UIButton> _searchTapTarget;

  // Controller to fetch and show doodles or a default Google logo.
  base::scoped_nsprotocol<id<LogoVendor>> _doodleController;

  // Most visited data from the MostVisitedSites service (copied upon receiving
  // the callback).
  ntp_tiles::NTPTilesVector _mostVisitedData;

  // |YES| if impressions were logged already and shouldn't be logged again.
  BOOL _recordedPageImpression;

  // A collection view for the most visited sites.
  base::scoped_nsobject<UICollectionView> _mostVisitedView;

  // The overscroll actions controller managing accelerators over the toolbar.
  base::scoped_nsobject<OverscrollActionsController>
      _overscrollActionsController;

  // |YES| when notifications indicate the omnibox is focused.
  BOOL _omniboxFocused;

  // Delegate to focus and blur the omnibox.
  base::WeakNSProtocol<id<OmniboxFocuser>> _focuser;

  // Tap and swipe gesture recognizers when the omnibox is focused.
  base::scoped_nsobject<UITapGestureRecognizer> _tapGestureRecognizer;
  base::scoped_nsobject<UISwipeGestureRecognizer> _swipeGestureRecognizer;

  // Handles displaying the context menu for all form factors.
  base::scoped_nsobject<ContextMenuCoordinator> _contextMenuCoordinator;

  // What's new promo.
  std::unique_ptr<NotificationPromoWhatsNew> _notification_promo;

  // A MostVisitedSites::Observer bridge object to get notified of most visited
  // sites changes.
  std::unique_ptr<google_landing::MostVisitedSitesObserverBridge>
      _most_visited_observer_bridge;

  std::unique_ptr<ntp_tiles::MostVisitedSites> _most_visited_sites;

  // URL of the last deleted most viewed entry. If present the UI to restore it
  // is shown.
  base::scoped_nsobject<NSURL> _deletedUrl;

  // Listen for default search engine changes.
  std::unique_ptr<google_landing::SearchEngineObserver> _observer;
  TemplateURLService* _templateURLService;  // weak

  // |YES| if the view has finished its first layout. This is useful when
  // determining if the view has sized itself for tablet.
  BOOL _viewLoaded;

  BOOL _animateHeader;
  BOOL _scrolledToTop;
  BOOL _isShowing;
  CFTimeInterval _shiftTilesDownStartTime;
  CGSize _mostVisitedCellSize;
  NSUInteger _maxNumMostVisited;
  ios::ChromeBrowserState* _browserState;  // Weak.
  id<UrlLoader> _loader;                   // Weak.
  std::unique_ptr<
      suggestions::SuggestionsService::ResponseCallbackList::Subscription>
      _suggestionsServiceResponseSubscription;
  base::scoped_nsobject<NSLayoutConstraint> _hintLabelLeadingConstraint;
  base::scoped_nsobject<NSLayoutConstraint> _voiceTapTrailingConstraint;
  base::scoped_nsobject<NSMutableArray> _supplementaryViews;
  base::scoped_nsobject<NewTabPageHeaderView> _headerView;
  base::scoped_nsobject<WhatsNewHeaderView> _promoHeaderView;
  base::WeakNSProtocol<id<WebToolbarDelegate>> _webToolbarDelegate;
  base::scoped_nsobject<TabModel> _tabModel;
}

// Whether the Google logo or doodle is being shown.
@property(nonatomic, readonly, getter=isShowingLogo) BOOL showingLogo;

@property(nonatomic) id<UrlLoader> loader;

// iPhone landscape uses a slightly different layout for the doodle and search
// field frame. Returns the proper frame from |frames| based on orientation,
// centered in the view.
- (CGRect)getOrientationFrame:(const CGRect[])frames;
// Returns the proper frame for the doodle.
- (CGRect)doodleFrame;
// Returns the proper frame for the search field.
- (CGRect)searchFieldFrame;
// Returns the height to use for the What's New promo view.
- (CGFloat)promoHeaderHeight;
// Add the LogoController view.
- (void)addDoodle;
// Add fake search field and voice search microphone.
- (void)addSearchField;
// Add most visited collection view.
- (void)addMostVisited;
// Update the iPhone fakebox's frame based on the current scroll view offset.
- (void)updateSearchField;
// Scrolls  most visited to the top of the view when the omnibox is focused.
- (void)locationBarBecomesFirstResponder;
// Scroll the view back to 0,0 when the omnibox loses focus.
- (void)locationBarResignsFirstResponder;
// When the search field is tapped.
- (void)searchFieldTapped:(id)sender;
// Tells WebToolbarController to resign focus to the omnibox.
- (void)blurOmnibox;
// Called when a user does a long press on a most visited item.
- (void)handleMostVisitedLongPress:
    (UILongPressGestureRecognizer*)longPressGesture;
// When the user removes a most visited a bubble pops up to undo the action.
- (void)showMostVisitedUndoForURL:(NSURL*)url;
// If Google is not the default search engine, hide the logo, doodle and
// fakebox.
- (void)updateLogoAndFakeboxDisplay;
// Helper method to set UICollectionViewFlowLayout insets for most visited.
- (void)setFlowLayoutInset:(UICollectionViewFlowLayout*)layout;
// Instructs the UICollectionView and UIView to reload it's data and layout.
- (void)reloadData;
// Logs a histogram due to a Most Visited item being opened.
- (void)logMostVisitedClick:(const NSUInteger)visitedIndex
                   tileType:(ntp_tiles::metrics::MostVisitedTileType)tileType;
// Returns the size of |_mostVisitedData|.
- (NSUInteger)numberOfItems;
// Returns the number of non empty tiles (as opposed to the placeholder tiles).
- (NSInteger)numberOfNonEmptyTilesShown;
// Returns the URL for the mosted visited item in |_mostVisitedData|.
- (GURL)urlForIndex:(NSUInteger)index;
// Removes a blacklisted URL in both |_mostVisitedData|.
- (void)removeBlacklistedURL:(const GURL&)url;
// Adds URL to the blacklist in both |_mostVisitedData|.
- (void)addBlacklistedURL:(const GURL&)url;
// Returns the expected height of the NewTabPageHeaderView.
- (CGFloat)heightForSectionWithOmnibox;
// Returns the nearest ancestor view that is kind of |aClass|.
- (UIView*)nearestAncestorOfView:(UIView*)view withClass:(Class)aClass;
// Updates the collection view's scroll view offset for the next frame of the
// shiftTilesDown animation.
- (void)shiftTilesDownAnimationDidFire:(CADisplayLink*)link;
// Returns the size to use for Most Visited cells in the NTP contained in
// |view|.
+ (CGSize)mostVisitedCellSizeForView:(UIView*)view;
// Returns the padding for use between Most Visited cells.
+ (CGFloat)mostVisitedCellPadding;

@end

@implementation GoogleLandingController

@synthesize loader = _loader;
// Property declared in NewTabPagePanelProtocol.
@synthesize delegate = _delegate;

+ (NSUInteger)maxSitesShown {
  return kMaxNumMostVisitedFavicons;
}

- (id)initWithLoader:(id<UrlLoader>)loader
          browserState:(ios::ChromeBrowserState*)browserState
               focuser:(id<OmniboxFocuser>)focuser
    webToolbarDelegate:(id<WebToolbarDelegate>)webToolbarDelegate
              tabModel:(TabModel*)tabModel {
  self = [super init];
  if (self) {
    DCHECK(browserState);
    _browserState = browserState;
    _loader = loader;
    _isShowing = YES;
    _maxNumMostVisited = [GoogleLandingController maxSitesShown];

    NSNotificationCenter* defaultCenter = [NSNotificationCenter defaultCenter];
    [defaultCenter
        addObserver:self
           selector:@selector(locationBarBecomesFirstResponder)
               name:ios_internal::kLocationBarBecomesFirstResponderNotification
             object:nil];
    [defaultCenter
        addObserver:self
           selector:@selector(locationBarResignsFirstResponder)
               name:ios_internal::kLocationBarResignsFirstResponderNotification
             object:nil];
    [defaultCenter
        addObserver:self
           selector:@selector(orientationDidChange:)
               name:UIApplicationDidChangeStatusBarOrientationNotification
             object:nil];

    _notification_promo.reset(new NotificationPromoWhatsNew(
        GetApplicationContext()->GetLocalState()));
    _notification_promo->Init();
    _tapGestureRecognizer.reset([[UITapGestureRecognizer alloc]
        initWithTarget:self
                action:@selector(blurOmnibox)]);
    [_tapGestureRecognizer setDelegate:self];
    _swipeGestureRecognizer.reset([[UISwipeGestureRecognizer alloc]
        initWithTarget:self
                action:@selector(blurOmnibox)]);
    [_swipeGestureRecognizer
        setDirection:UISwipeGestureRecognizerDirectionDown];

    _view.reset(
        [[GoogleLandingView alloc] initWithFrame:[UIScreen mainScreen].bounds]);
    [_view setAutoresizingMask:UIViewAutoresizingFlexibleHeight |
                               UIViewAutoresizingFlexibleWidth];
    [_view setFrameDelegate:self];

    _focuser.reset(focuser);
    _webToolbarDelegate.reset(webToolbarDelegate);
    _tabModel.reset([tabModel retain]);

    _scrolledToTop = NO;
    _animateHeader = YES;
    // Initialise |shiftTilesDownStartTime| to a sentinel value to indicate that
    // the animation has not yet started.
    _shiftTilesDownStartTime = -1;
    _mostVisitedCellSize =
        [GoogleLandingController mostVisitedCellSizeForView:_view];
    [self addDoodle];
    [self addSearchField];
    [self addMostVisited];
    [self addOverscrollActions];
    [self reload];
  }
  return self;
}

+ (CGSize)mostVisitedCellSizeForView:(UIView*)view {
  if (IsIPadIdiom()) {
    // On iPads, split-screen and slide-over may require showing smaller cells.
    CGSize maximumCellSize = [MostVisitedCell maximumSize];
    CGSize viewSize = view.bounds.size;
    CGFloat smallestDimension =
        viewSize.height > viewSize.width ? viewSize.width : viewSize.height;
    CGFloat cellWidth = AlignValueToPixel(
        (smallestDimension - 3 * [self.class mostVisitedCellPadding]) / 2);
    if (cellWidth < maximumCellSize.width) {
      return CGSizeMake(cellWidth, cellWidth);
    } else {
      return maximumCellSize;
    }
  } else {
    return [MostVisitedCell maximumSize];
  }
}

+ (CGFloat)mostVisitedCellPadding {
  return IsIPadIdiom() ? kMostVisitedPaddingIPadFavicon
                       : kMostVisitedPaddingIPhone;
}

- (void)orientationDidChange:(NSNotification*)notification {
  if (IsIPadIdiom() && _scrolledToTop) {
    // Keep the most visited thumbnails scrolled to the top.
    base::WeakNSObject<GoogleLandingController> weakSelf(self);
    dispatch_after(
        dispatch_time(DISPATCH_TIME_NOW, 0), dispatch_get_main_queue(), ^{
          base::scoped_nsobject<GoogleLandingController> strongSelf(
              [weakSelf retain]);
          if (!strongSelf)
            return;

          [strongSelf.get()->_mostVisitedView
              setContentOffset:CGPointMake(0, [strongSelf pinnedOffsetY])];
        });
    return;
  }

  // Call inside a block to avoid the animation that -orientationDidChange is
  // wrapped inside.
  base::WeakNSObject<GoogleLandingController> weakSelf(self);
  void (^layoutBlock)(void) = ^{
    base::scoped_nsobject<GoogleLandingController> strongSelf(
        [weakSelf retain]);
    // Invalidate the layout so that the collection view's header size is reset
    // for the new orientation.
    if (!_scrolledToTop) {
      [[strongSelf.get()->_mostVisitedView collectionViewLayout]
          invalidateLayout];
      [[strongSelf view] setNeedsLayout];
    }

    // Call -scrollViewDidScroll: so that the omnibox's frame is adjusted for
    // the scroll view's offset.
    [self scrollViewDidScroll:_mostVisitedView];
  };

  dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 0), dispatch_get_main_queue(),
                 layoutBlock);
}

- (CGFloat)viewWidth {
  return [_view frame].size.width;
}

- (int)numberOfColumns {
  CGFloat width = [self viewWidth];
  CGFloat padding = [self.class mostVisitedCellPadding];
  // Try to fit 4 columns.
  if (width >= 5 * padding + _mostVisitedCellSize.width * 4)
    return 4;
  // Try to fit 3 columns.
  if (width >= 4 * padding + _mostVisitedCellSize.width * 3)
    return 3;
  // Try to fit 2 columns.
  if (width >= 3 * padding + _mostVisitedCellSize.width * 2)
    return 2;
  // We never want to have a layout with only one column, however: At launch,
  // the view's size is initialized to the width of 320, which can only fit
  // one column on iPhone 6 and 6+. TODO(crbug.com/506183): Get rid of the
  // unecessary resize, and add a NOTREACHED() here.
  return 1;
}

- (CGFloat)leftMargin {
  int columns = [self numberOfColumns];
  CGFloat whitespace = [self viewWidth] - columns * _mostVisitedCellSize.width -
                       (columns - 1) * [self.class mostVisitedCellPadding];
  CGFloat margin = AlignValueToPixel(whitespace / 2);
  DCHECK(margin >= [self.class mostVisitedCellPadding]);
  return margin;
}

- (void)dealloc {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [_mostVisitedView setDelegate:nil];
  [_mostVisitedView setDataSource:nil];
  [_overscrollActionsController invalidate];
  [super dealloc];
}

- (CGRect)doodleFrame {
  const CGRect kDoodleFrame[2] = {
      {{0, 66}, {0, 120}}, {{0, 56}, {0, 120}},
  };
  CGRect doodleFrame = [self getOrientationFrame:kDoodleFrame];
  if (!IsIPadIdiom() && !self.showingLogo)
    doodleFrame.size.height = kNonGoogleSearchDoodleHeight;
  if (IsIPadIdiom()) {
    doodleFrame.origin.y = IsPortrait() ? kDoodleTopMarginIPadPortrait
                                        : kDoodleTopMarginIPadLandscape;
  }
  return doodleFrame;
}

- (CGRect)searchFieldFrame {
  CGFloat y = CGRectGetMaxY([self doodleFrame]);
  CGFloat leftMargin = [self leftMargin];
  if (leftMargin > kMaxSearchFieldFrameMargin)
    leftMargin = kMaxSearchFieldFrameMargin;
  const CGRect kSearchFieldFrame[2] = {
      {{leftMargin, y + 32}, {0, 50}}, {{leftMargin, y + 16}, {0, 50}},
  };
  CGRect searchFieldFrame = [self getOrientationFrame:kSearchFieldFrame];
  if (IsIPadIdiom()) {
    CGFloat iPadTopMargin = IsPortrait() ? kDoodleTopMarginIPadPortrait
                                         : kDoodleTopMarginIPadLandscape;
    searchFieldFrame.origin.y += iPadTopMargin - 32;
  }
  return searchFieldFrame;
}

- (CGRect)getOrientationFrame:(const CGRect[])frames {
  UIInterfaceOrientation orient =
      [[UIApplication sharedApplication] statusBarOrientation];
  InterfaceOrientation inter_orient =
      (IsIPadIdiom() || UIInterfaceOrientationIsPortrait(orient))
          ? ALL
          : IPHONE_LANDSCAPE;

  // Calculate width based on screen width and origin x.
  CGRect frame = frames[inter_orient];
  frame.size.width = fmax(self.view.bounds.size.width - 2 * frame.origin.x, 50);
  return frame;
}

- (CGFloat)promoHeaderHeight {
  CGFloat promoMaxWidth = [self viewWidth] - 2 * [self leftMargin];
  NSString* text = base::SysUTF8ToNSString(_notification_promo->promo_text());
  return [WhatsNewHeaderView heightToFitText:text inWidth:promoMaxWidth];
}

- (ToolbarController*)relinquishedToolbarController {
  return [_headerView relinquishedToolbarController];
}

- (void)reparentToolbarController {
  [_headerView reparentToolbarController];
}

- (BOOL)isShowingLogo {
  return [_doodleController isShowingLogo];
}

- (void)updateLogoAndFakeboxDisplay {
  BOOL showLogo = NO;
  TemplateURL* defaultURL = _templateURLService->GetDefaultSearchProvider();
  if (defaultURL) {
    showLogo =
        defaultURL->GetEngineType(_templateURLService->search_terms_data()) ==
        SEARCH_ENGINE_GOOGLE;
  }

  if (self.showingLogo != showLogo) {
    [_doodleController setShowingLogo:showLogo];
    if (_viewLoaded) {
      [self updateSubviewFrames];

      // Adjust the height of |_headerView| to fit its content which may have
      // been shifted due to the visibility of the doodle.
      CGRect headerFrame = [_headerView frame];
      headerFrame.size.height = [self heightForSectionWithOmnibox];
      [_headerView setFrame:headerFrame];

      // Adjust vertical positioning of |_promoHeaderView|.
      CGFloat omniboxHeaderHeight =
          [self collectionView:_mostVisitedView
                                       layout:[_mostVisitedView
                                                  collectionViewLayout]
              referenceSizeForHeaderInSection:0]
              .height;
      CGRect whatsNewFrame = [_promoHeaderView frame];
      whatsNewFrame.origin.y = omniboxHeaderHeight;
      [_promoHeaderView setFrame:whatsNewFrame];
    }
    if (IsIPadIdiom())
      [_searchTapTarget setHidden:!self.showingLogo];
  }
}

// Initialize and add a Google Doodle widget, show a Google logo by default.
- (void)addDoodle {
  if (!_doodleController) {
    _doodleController.reset(ios::GetChromeBrowserProvider()->CreateLogoVendor(
        _browserState, _loader));
  }
  [[_doodleController view] setFrame:[self doodleFrame]];

  _templateURLService =
      ios::TemplateURLServiceFactory::GetForBrowserState(_browserState);
  _observer.reset(
      new google_landing::SearchEngineObserver(self, _templateURLService));
  _templateURLService->Load();
}

// Initialize and add a search field tap target and a voice search button.
- (void)addSearchField {
  CGRect searchFieldFrame = [self searchFieldFrame];
  _searchTapTarget.reset([[UIButton alloc] initWithFrame:searchFieldFrame]);
  if (IsIPadIdiom()) {
    UIImage* searchBoxImage = [[UIImage imageNamed:@"ntp_google_search_box"]
        resizableImageWithCapInsets:kSearchBoxStretchInsets];
    [_searchTapTarget setBackgroundImage:searchBoxImage
                                forState:UIControlStateNormal];
  }
  [_searchTapTarget setAdjustsImageWhenHighlighted:NO];
  [_searchTapTarget addTarget:self
                       action:@selector(searchFieldTapped:)
             forControlEvents:UIControlEventTouchUpInside];
  [_searchTapTarget
      setAccessibilityLabel:l10n_util::GetNSString(IDS_OMNIBOX_EMPTY_HINT)];
  // Set isAccessibilityElement to NO so that Voice Search button is accessible.
  [_searchTapTarget setIsAccessibilityElement:NO];

  // Set up fakebox hint label.
  CGRect hintFrame = CGRectInset([_searchTapTarget bounds], 12, 3);
  const CGFloat kVoiceSearchOffset = 48;
  hintFrame.size.width = searchFieldFrame.size.width - kVoiceSearchOffset;
  base::scoped_nsobject<UILabel> searchHintLabel(
      [[UILabel alloc] initWithFrame:hintFrame]);
  [_searchTapTarget addSubview:searchHintLabel];
  [searchHintLabel setTranslatesAutoresizingMaskIntoConstraints:NO];
  [searchHintLabel
      addConstraint:[NSLayoutConstraint
                        constraintWithItem:searchHintLabel
                                 attribute:NSLayoutAttributeHeight
                                 relatedBy:NSLayoutRelationEqual
                                    toItem:nil
                                 attribute:NSLayoutAttributeNotAnAttribute
                                multiplier:1
                                  constant:hintFrame.size.height]];
  [_searchTapTarget
      addConstraint:[NSLayoutConstraint
                        constraintWithItem:searchHintLabel
                                 attribute:NSLayoutAttributeCenterY
                                 relatedBy:NSLayoutRelationEqual
                                    toItem:_searchTapTarget
                                 attribute:NSLayoutAttributeCenterY
                                multiplier:1
                                  constant:0]];
  _hintLabelLeadingConstraint.reset(
      [[NSLayoutConstraint constraintWithItem:searchHintLabel
                                    attribute:NSLayoutAttributeLeading
                                    relatedBy:NSLayoutRelationEqual
                                       toItem:_searchTapTarget
                                    attribute:NSLayoutAttributeLeading
                                   multiplier:1
                                     constant:kHintLabelSidePadding] retain]);
  [_searchTapTarget addConstraint:_hintLabelLeadingConstraint];
  [searchHintLabel setText:l10n_util::GetNSString(IDS_OMNIBOX_EMPTY_HINT)];
  if (base::i18n::IsRTL()) {
    [searchHintLabel setTextAlignment:NSTextAlignmentRight];
  }
  [searchHintLabel
      setTextColor:[UIColor
                       colorWithWhite:kiPhoneOmniboxPlaceholderColorBrightness
                                alpha:1.0]];
  [searchHintLabel setFont:[MDCTypography subheadFont]];

  // Add a voice search button.
  UIImage* micImage = [UIImage imageNamed:@"voice_icon"];
  base::scoped_nsobject<UIButton> voiceTapTarget(
      [[UIButton alloc] initWithFrame:CGRectZero]);
  [_searchTapTarget addSubview:voiceTapTarget];

  [voiceTapTarget setTranslatesAutoresizingMaskIntoConstraints:NO];
  [_searchTapTarget
      addConstraint:[NSLayoutConstraint
                        constraintWithItem:voiceTapTarget
                                 attribute:NSLayoutAttributeCenterY
                                 relatedBy:NSLayoutRelationEqual
                                    toItem:_searchTapTarget
                                 attribute:NSLayoutAttributeCenterY
                                multiplier:1
                                  constant:0]];
  _voiceTapTrailingConstraint.reset(
      [[NSLayoutConstraint constraintWithItem:voiceTapTarget
                                    attribute:NSLayoutAttributeTrailing
                                    relatedBy:NSLayoutRelationEqual
                                       toItem:_searchTapTarget
                                    attribute:NSLayoutAttributeTrailing
                                   multiplier:1
                                     constant:0] retain]);
  [_searchTapTarget addConstraint:_voiceTapTrailingConstraint];
  [voiceTapTarget
      addConstraint:[NSLayoutConstraint
                        constraintWithItem:voiceTapTarget
                                 attribute:NSLayoutAttributeHeight
                                 relatedBy:NSLayoutRelationEqual
                                    toItem:nil
                                 attribute:NSLayoutAttributeNotAnAttribute
                                multiplier:0
                                  constant:kVoiceSearchButtonWidth]];
  [voiceTapTarget
      addConstraint:[NSLayoutConstraint
                        constraintWithItem:voiceTapTarget
                                 attribute:NSLayoutAttributeWidth
                                 relatedBy:NSLayoutRelationEqual
                                    toItem:nil
                                 attribute:NSLayoutAttributeNotAnAttribute
                                multiplier:0
                                  constant:kVoiceSearchButtonWidth]];
  [_searchTapTarget
      addConstraint:[NSLayoutConstraint
                        constraintWithItem:searchHintLabel
                                 attribute:NSLayoutAttributeTrailing
                                 relatedBy:NSLayoutRelationEqual
                                    toItem:voiceTapTarget
                                 attribute:NSLayoutAttributeLeading
                                multiplier:1
                                  constant:0]];
  [voiceTapTarget setAdjustsImageWhenHighlighted:NO];
  [voiceTapTarget setImage:micImage forState:UIControlStateNormal];
  [voiceTapTarget setTag:IDC_VOICE_SEARCH];
  [voiceTapTarget setAccessibilityLabel:l10n_util::GetNSString(
                                            IDS_IOS_ACCNAME_VOICE_SEARCH)];
  [voiceTapTarget setAccessibilityIdentifier:@"Voice Search"];

  if (ios::GetChromeBrowserProvider()
          ->GetVoiceSearchProvider()
          ->IsVoiceSearchEnabled()) {
    [voiceTapTarget addTarget:self
                       action:@selector(loadVoiceSearch:)
             forControlEvents:UIControlEventTouchUpInside];
    [voiceTapTarget addTarget:self
                       action:@selector(preloadVoiceSearch:)
             forControlEvents:UIControlEventTouchDown];
  } else {
    [voiceTapTarget setEnabled:NO];
  }
}

- (void)loadVoiceSearch:(id)sender {
  DCHECK(ios::GetChromeBrowserProvider()
             ->GetVoiceSearchProvider()
             ->IsVoiceSearchEnabled());
  base::RecordAction(UserMetricsAction("MobileNTPMostVisitedVoiceSearch"));
  [sender chromeExecuteCommand:sender];
}

- (void)preloadVoiceSearch:(id)sender {
  DCHECK(ios::GetChromeBrowserProvider()
             ->GetVoiceSearchProvider()
             ->IsVoiceSearchEnabled());
  [sender removeTarget:self
                action:@selector(preloadVoiceSearch:)
      forControlEvents:UIControlEventTouchDown];

  // Use a GenericChromeCommand because |sender| already has a tag set for a
  // different command.
  base::scoped_nsobject<GenericChromeCommand> command(
      [[GenericChromeCommand alloc] initWithTag:IDC_PRELOAD_VOICE_SEARCH]);
  [sender chromeExecuteCommand:command];
}

- (void)setFlowLayoutInset:(UICollectionViewFlowLayout*)layout {
  CGFloat leftMargin = [self leftMargin];
  [layout setSectionInset:UIEdgeInsetsMake(0, leftMargin, 0, leftMargin)];
}

- (void)resetSectionInset {
  UICollectionViewFlowLayout* flowLayout =
      (UICollectionViewFlowLayout*)[_mostVisitedView collectionViewLayout];
  [flowLayout setSectionInset:UIEdgeInsetsZero];
}

- (void)updateSubviewFrames {
  _mostVisitedCellSize =
      [GoogleLandingController mostVisitedCellSizeForView:_view];
  UICollectionViewFlowLayout* flowLayout =
      base::mac::ObjCCastStrict<UICollectionViewFlowLayout>(
          [_mostVisitedView collectionViewLayout]);
  [flowLayout setItemSize:_mostVisitedCellSize];
  [[_doodleController view] setFrame:[self doodleFrame]];

  [self setFlowLayoutInset:flowLayout];
  [flowLayout invalidateLayout];
  [_promoHeaderView setSideMargin:[self leftMargin]];

  // On the iPhone 6 Plus, if the app is started in landscape after a fresh
  // install, the UICollectionViewLayout incorrectly sizes the widths of the
  // supplementary views to the portrait width.  Correct that here to ensure
  // that the header is property laid out to the UICollectionView's width.
  // crbug.com/491131
  CGFloat collectionViewWidth = CGRectGetWidth([_mostVisitedView bounds]);
  CGFloat collectionViewMinX = CGRectGetMinX([_mostVisitedView bounds]);
  for (UIView* supplementaryView in _supplementaryViews.get()) {
    CGRect supplementaryViewFrame = supplementaryView.frame;
    supplementaryViewFrame.origin.x = collectionViewMinX;
    supplementaryViewFrame.size.width = collectionViewWidth;
    supplementaryView.frame = supplementaryViewFrame;
  }

  BOOL isScrollableNTP = !IsIPadIdiom() || IsCompactTablet();
  if (isScrollableNTP && _scrolledToTop) {
    // Set the scroll view's offset to the pinned offset to keep the omnibox
    // at the top of the screen if it isn't already.
    CGFloat pinnedOffsetY = [self pinnedOffsetY];
    if ([_mostVisitedView contentOffset].y < pinnedOffsetY) {
      [_mostVisitedView setContentOffset:CGPointMake(0, pinnedOffsetY)];
    } else {
      [self updateSearchField];
    }
  } else {
    [_searchTapTarget setFrame:[self searchFieldFrame]];
  }

  if (!_viewLoaded) {
    _viewLoaded = YES;
    [_doodleController fetchDoodle];
  }
  [self.delegate updateNtpBarShadowForPanelController:self];
}

// Initialize and add a panel with most visited sites.
- (void)addMostVisited {
  CGRect mostVisitedFrame = [_view bounds];
  base::scoped_nsobject<UICollectionViewFlowLayout> flowLayout;
  if (IsIPadIdiom())
    flowLayout.reset([[UICollectionViewFlowLayout alloc] init]);
  else
    flowLayout.reset([[MostVisitedLayout alloc] init]);

  [flowLayout setScrollDirection:UICollectionViewScrollDirectionVertical];
  [flowLayout setItemSize:_mostVisitedCellSize];
  [flowLayout setMinimumInteritemSpacing:8];
  [flowLayout setMinimumLineSpacing:[self.class mostVisitedCellPadding]];
  DCHECK(!_mostVisitedView);
  _mostVisitedView.reset([[UICollectionView alloc]
             initWithFrame:mostVisitedFrame
      collectionViewLayout:flowLayout]);
  [_mostVisitedView setAutoresizingMask:UIViewAutoresizingFlexibleHeight |
                                        UIViewAutoresizingFlexibleWidth];
  [_mostVisitedView setDelegate:self];
  [_mostVisitedView setDataSource:self];
  [_mostVisitedView registerClass:[MostVisitedCell class]
       forCellWithReuseIdentifier:@"classCell"];
  [_mostVisitedView setBackgroundColor:[UIColor clearColor]];
  [_mostVisitedView setBounces:YES];
  [_mostVisitedView setShowsHorizontalScrollIndicator:NO];
  [_mostVisitedView setShowsVerticalScrollIndicator:NO];
  [_mostVisitedView registerClass:[WhatsNewHeaderView class]
       forSupplementaryViewOfKind:UICollectionElementKindSectionHeader
              withReuseIdentifier:@"whatsNew"];
  [_mostVisitedView registerClass:[NewTabPageHeaderView class]
       forSupplementaryViewOfKind:UICollectionElementKindSectionHeader
              withReuseIdentifier:@"header"];
  [_mostVisitedView setAccessibilityIdentifier:@"Google Landing"];

  [_view addSubview:_mostVisitedView];
  _most_visited_sites =
      IOSMostVisitedSitesFactory::NewForBrowserState(_browserState);
  _most_visited_observer_bridge.reset(
      new google_landing::MostVisitedSitesObserverBridge(self));
  _most_visited_sites->SetMostVisitedURLsObserver(
      _most_visited_observer_bridge.get(), kMaxNumMostVisitedFavicons);
}

- (void)updateSearchField {
  NSArray* constraints =
      @[ _hintLabelLeadingConstraint, _voiceTapTrailingConstraint ];
  [_headerView updateSearchField:_searchTapTarget
                withInitialFrame:[self searchFieldFrame]
              subviewConstraints:constraints
                       forOffset:[_mostVisitedView contentOffset].y];
}

- (void)addOverscrollActions {
  if (!IsIPadIdiom()) {
    _overscrollActionsController.reset([[OverscrollActionsController alloc]
        initWithScrollView:_mostVisitedView]);
    [_overscrollActionsController setStyle:OverscrollStyle::NTP_NON_INCOGNITO];
    [_overscrollActionsController setDelegate:self];
  }
}

// Check to see if the promo label should be hidden.
- (void)hideWhatsNewIfNecessary {
  if (![_promoHeaderView isHidden] && _notification_promo &&
      !_notification_promo->CanShow()) {
    [_promoHeaderView setHidden:YES];
    _notification_promo.reset();
    [self.view setNeedsLayout];
  }
}

- (void)locationBarBecomesFirstResponder {
  if (!_isShowing)
    return;

  _omniboxFocused = YES;
  [self shiftTilesUp];
}

- (void)shiftTilesUp {
  _scrolledToTop = YES;
  // Add gesture recognizer to background |_view| when omnibox is focused.
  [_view addGestureRecognizer:_tapGestureRecognizer];
  [_view addGestureRecognizer:_swipeGestureRecognizer];

  CGFloat pinnedOffsetY = [self pinnedOffsetY];
  _animateHeader = !IsIPadIdiom();

  [UIView animateWithDuration:0.25
      animations:^{
        if ([_mostVisitedView contentOffset].y < pinnedOffsetY) {
          [_mostVisitedView setContentOffset:CGPointMake(0, pinnedOffsetY)];
          [[_mostVisitedView collectionViewLayout] invalidateLayout];
        }
      }
      completion:^(BOOL finished) {
        // Check to see if we are still scrolled to the top -- it's possible
        // (and difficult) to resign the first responder and initiate a
        // -shiftTilesDown before the animation here completes.
        if (_scrolledToTop) {
          _animateHeader = NO;
          if (!IsIPadIdiom()) {
            [_focuser onFakeboxAnimationComplete];
            [_headerView fadeOutShadow];
            [_searchTapTarget setHidden:YES];
          }
        }
      }];
}

- (void)searchFieldTapped:(id)sender {
  [_focuser focusFakebox];
}

- (void)blurOmnibox {
  if (_omniboxFocused) {
    [_focuser cancelOmniboxEdit];
  } else {
    [self locationBarResignsFirstResponder];
  }
}

- (void)locationBarResignsFirstResponder {
  if (!_isShowing && !_scrolledToTop)
    return;

  _omniboxFocused = NO;
  if ([_contextMenuCoordinator isVisible]) {
    return;
  }

  [self shiftTilesDown];
}

- (void)shiftTilesDown {
  _animateHeader = YES;
  _scrolledToTop = NO;
  if (!IsIPadIdiom()) {
    [_searchTapTarget setHidden:NO];
    [_focuser onFakeboxBlur];
  }

  // Reload most visited sites in case the number of placeholder cells needs to
  // be updated after an orientation change.
  [_mostVisitedView reloadData];

  // Reshow views that are within range of the most visited collection view
  // (if necessary).
  [_view removeGestureRecognizer:_tapGestureRecognizer];
  [_view removeGestureRecognizer:_swipeGestureRecognizer];

  // CADisplayLink is used for this animation instead of the standard UIView
  // animation because the standard animation did not properly convert the
  // fakebox from its scrolled up mode to its scrolled down mode. Specifically,
  // calling |UICollectionView reloadData| adjacent to the standard animation
  // caused the fakebox's views to jump incorrectly. CADisplayLink avoids this
  // problem because it allows |shiftTilesDownAnimationDidFire| to directly
  // control each frame.
  CADisplayLink* link = [CADisplayLink
      displayLinkWithTarget:self
                   selector:@selector(shiftTilesDownAnimationDidFire:)];
  [link addToRunLoop:[NSRunLoop mainRunLoop] forMode:NSDefaultRunLoopMode];

  // Dismisses modal UI elements if displayed. Must be called at the end of
  // -locationBarResignsFirstResponder since it could result in -dealloc being
  // called.
  [self dismissModals];
}

- (void)shiftTilesDownAnimationDidFire:(CADisplayLink*)link {
  // If this is the first frame of the animation, store the starting timestamp
  // and do nothing.
  if (_shiftTilesDownStartTime == -1) {
    _shiftTilesDownStartTime = link.timestamp;
    return;
  }

  CFTimeInterval timeElapsed = link.timestamp - _shiftTilesDownStartTime;
  double percentComplete = timeElapsed / kShiftTilesDownAnimationDuration;
  // Ensure that the percentage cannot be above 1.0.
  if (percentComplete > 1.0)
    percentComplete = 1.0;

  // Find how much the collection view should be scrolled up in the next frame.
  CGFloat yOffset = (1.0 - percentComplete) * [self pinnedOffsetY];
  [_mostVisitedView setContentOffset:CGPointMake(0, yOffset)];

  if (percentComplete == 1.0) {
    [link invalidate];
    // Reset |shiftTilesDownStartTime to its sentinal value.
    _shiftTilesDownStartTime = -1;
    [[_mostVisitedView collectionViewLayout] invalidateLayout];
  }
}

- (void)logMostVisitedClick:(const NSUInteger)visitedIndex
                   tileType:(ntp_tiles::metrics::MostVisitedTileType)tileType {
  new_tab_page_uma::RecordAction(
      _browserState, new_tab_page_uma::ACTION_OPENED_MOST_VISITED_ENTRY);
  base::RecordAction(UserMetricsAction("MobileNTPMostVisited"));
  const ntp_tiles::NTPTile& tile = _mostVisitedData[visitedIndex];
  ntp_tiles::metrics::RecordTileClick(visitedIndex, tile.source, tileType);
}

- (void)onMostVisitedURLsAvailable:(const ntp_tiles::NTPTilesVector&)data {
  _mostVisitedData = data;
  [self reloadData];

  if (data.size() && !_recordedPageImpression) {
    _recordedPageImpression = YES;
    std::vector<ntp_tiles::metrics::TileImpression> tiles;
    for (const ntp_tiles::NTPTile& ntpTile : data) {
      tiles.emplace_back(ntpTile.source, ntp_tiles::metrics::UNKNOWN_TILE_TYPE,
                         ntpTile.url);
    }
    ntp_tiles::metrics::RecordPageImpression(
        tiles, GetApplicationContext()->GetRapporServiceImpl());
  }
}

- (void)onIconMadeAvailable:(const GURL&)siteUrl {
  for (size_t i = 0; i < [self numberOfItems]; ++i) {
    const ntp_tiles::NTPTile& ntpTile = _mostVisitedData[i];
    if (ntpTile.url == siteUrl) {
      NSIndexPath* indexPath =
          [NSIndexPath indexPathForRow:i inSection:SectionWithMostVisited];
      [_mostVisitedView reloadItemsAtIndexPaths:@[ indexPath ]];
      break;
    }
  }
}

- (void)reloadData {
  // -reloadData updates from |_mostVisitedData|.
  // -invalidateLayout is necessary because sometimes the flowLayout has the
  // wrong cached size and will throw an internal exception if the
  // -numberOfItems shrinks. -setNeedsLayout is needed in case
  // -numberOfItems increases enough to add a new row and change the height
  // of _mostVisitedView.
  [_mostVisitedView reloadData];
  [[_mostVisitedView collectionViewLayout] invalidateLayout];
  [self.view setNeedsLayout];
}

- (void)willUpdateSnapshot {
  [_overscrollActionsController clear];
}

- (CGFloat)heightForSectionWithOmnibox {
  CGFloat headerHeight =
      CGRectGetMaxY([self searchFieldFrame]) + kNTPSearchFieldBottomPadding;
  if (IsIPadIdiom()) {
    if (self.showingLogo) {
      if (!_notification_promo || !_notification_promo->CanShow()) {
        UIInterfaceOrientation orient =
            [[UIApplication sharedApplication] statusBarOrientation];
        const CGFloat kTopSpacingMaterialPortrait = 56;
        const CGFloat kTopSpacingMaterialLandscape = 32;
        headerHeight += UIInterfaceOrientationIsPortrait(orient)
                            ? kTopSpacingMaterialPortrait
                            : kTopSpacingMaterialLandscape;
      }
    } else {
      headerHeight = kNonGoogleSearchHeaderHeightIPad;
    }
  }
  return headerHeight;
}

#pragma mark - UICollectionView Methods.

- (CGSize)collectionView:(UICollectionView*)collectionView
                             layout:
                                 (UICollectionViewLayout*)collectionViewLayout
    referenceSizeForHeaderInSection:(NSInteger)section {
  CGFloat headerHeight = 0;
  if (section == SectionWithOmnibox) {
    headerHeight = [self heightForSectionWithOmnibox];
    ((UICollectionViewFlowLayout*)collectionViewLayout).headerReferenceSize =
        CGSizeMake(0, headerHeight);
  } else if (section == SectionWithMostVisited) {
    if (_notification_promo && _notification_promo->CanShow()) {
      headerHeight = [self promoHeaderHeight];
    } else {
      headerHeight = kWhatsNewHeaderHiddenHeight;
    }
  }
  return CGSizeMake(0, headerHeight);
}

#pragma mark - UICollectionViewDelegate

- (BOOL)collectionView:(UICollectionView*)collectionView
    shouldSelectItemAtIndexPath:(NSIndexPath*)indexPath {
  return indexPath.row < static_cast<NSInteger>([self numberOfItems]);
}

- (void)collectionView:(UICollectionView*)collectionView
    didSelectItemAtIndexPath:(NSIndexPath*)indexPath {
  MostVisitedCell* cell =
      (MostVisitedCell*)[collectionView cellForItemAtIndexPath:indexPath];

  // Keep the UICollectionView alive for one second while the screen
  // reader does its thing.
  // TODO(jif): This needs a radar, since it is almost certainly a
  // UIKit accessibility bug. crbug.com/529271
  if (UIAccessibilityIsVoiceOverRunning()) {
    UICollectionView* blockView = [_mostVisitedView retain];
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW,
                                 static_cast<int64_t>(1 * NSEC_PER_SEC)),
                   dispatch_get_main_queue(), ^{
                     [blockView release];
                   });
  }

  const NSUInteger visitedIndex = indexPath.row;
  [self blurOmnibox];
  DCHECK(visitedIndex < [self numberOfItems]);
  [self logMostVisitedClick:visitedIndex tileType:cell.tileType];
  [_loader loadURL:[self urlForIndex:visitedIndex]
               referrer:web::Referrer()
             transition:ui::PAGE_TRANSITION_AUTO_BOOKMARK
      rendererInitiated:NO];
}

#pragma mark - UICollectionViewDataSource

- (UICollectionReusableView*)collectionView:(UICollectionView*)collectionView
          viewForSupplementaryElementOfKind:(NSString*)kind
                                atIndexPath:(NSIndexPath*)indexPath {
  DCHECK(kind == UICollectionElementKindSectionHeader);

  if (!_supplementaryViews)
    _supplementaryViews.reset([[NSMutableArray alloc] init]);

  if (indexPath.section == SectionWithOmnibox) {
    if (!_headerView) {
      _headerView.reset([[collectionView
          dequeueReusableSupplementaryViewOfKind:
              UICollectionElementKindSectionHeader
                             withReuseIdentifier:@"header"
                                    forIndexPath:indexPath] retain]);
      [_headerView addSubview:[_doodleController view]];
      [_headerView addSubview:_searchTapTarget];
      [_headerView addViewsToSearchField:_searchTapTarget];

      if (!IsIPadIdiom()) {
        ReadingListModel* readingListModel = nullptr;
        if (reading_list::switches::IsReadingListEnabled()) {
          readingListModel =
              ReadingListModelFactory::GetForBrowserState(_browserState);
        }
        // iPhone header also contains a toolbar since the normal toolbar is
        // hidden.
        [_headerView addToolbarWithDelegate:_webToolbarDelegate
                                    focuser:_focuser
                                   tabModel:_tabModel
                           readingListModel:readingListModel];
      }
      [_supplementaryViews addObject:_headerView];
    }
    return _headerView;
  }

  if (indexPath.section == SectionWithMostVisited) {
    if (!_promoHeaderView) {
      _promoHeaderView.reset([[collectionView
          dequeueReusableSupplementaryViewOfKind:
              UICollectionElementKindSectionHeader
                             withReuseIdentifier:@"whatsNew"
                                    forIndexPath:indexPath] retain]);
      [_promoHeaderView setSideMargin:[self leftMargin]];
      [_promoHeaderView setDelegate:self];
      if (_notification_promo && _notification_promo->CanShow()) {
        [_promoHeaderView
            setText:base::SysUTF8ToNSString(_notification_promo->promo_text())];
        [_promoHeaderView setIcon:_notification_promo->icon()];
        _notification_promo->HandleViewed();
      }
      [_supplementaryViews addObject:_promoHeaderView];
    }
    return _promoHeaderView;
  }

  NOTREACHED();
  return nil;
}

- (NSInteger)numberOfSectionsInCollectionView:
    (UICollectionView*)collectionView {
  return NumberOfCollectionViewSections;
}

- (NSInteger)collectionView:(UICollectionView*)collectionView
     numberOfItemsInSection:(NSInteger)section {
  // The first section only contains a header view and no items.
  if (section == SectionWithOmnibox)
    return 0;

  // Phone always contains the maximum number of cells. Cells in excess of the
  // number of thumbnails are used solely for layout/sizing.
  if (!IsIPadIdiom())
    return _maxNumMostVisited;

  return [self numberOfNonEmptyTilesShown];
}

- (UICollectionViewCell*)collectionView:(UICollectionView*)collectionView
                 cellForItemAtIndexPath:(NSIndexPath*)indexPath {
  MostVisitedCell* cell = (MostVisitedCell*)[collectionView
      dequeueReusableCellWithReuseIdentifier:@"classCell"
                                forIndexPath:indexPath];
  BOOL isPlaceholder = indexPath.row >= (int)[self numberOfItems];
  if (isPlaceholder) {
    [cell showPlaceholder];
    for (UIGestureRecognizer* ges in cell.gestureRecognizers) {
      [cell removeGestureRecognizer:ges];
    }

    // When -numberOfItems is 0, always remove the placeholder.
    if (indexPath.row >= [self numberOfColumns] || [self numberOfItems] == 0) {
      // This cell is completely empty and only exists for layout/sizing
      // purposes.
      [cell removePlaceholderImage];
    }
    return cell;
  }

  const ntp_tiles::NTPTile& ntpTile = _mostVisitedData[indexPath.row];
  NSString* title = base::SysUTF16ToNSString(ntpTile.title);

  [cell setupWithURL:ntpTile.url title:title browserState:_browserState];

  base::scoped_nsobject<UILongPressGestureRecognizer> longPress(
      [[UILongPressGestureRecognizer alloc]
          initWithTarget:self
                  action:@selector(handleMostVisitedLongPress:)]);
  [cell addGestureRecognizer:longPress];

  return cell;
}

#pragma mark - Context Menu

// Called when a user does a long press on a most visited item.
- (void)handleMostVisitedLongPress:(UILongPressGestureRecognizer*)sender {
  if (sender.state == UIGestureRecognizerStateBegan) {
    // Only one long press at a time.
    if ([_contextMenuCoordinator isVisible]) {
      return;
    }

    NSIndexPath* indexPath = [_mostVisitedView
        indexPathForCell:static_cast<UICollectionViewCell*>(sender.view)];
    const NSUInteger index = indexPath.row;

    // A long press occured on one of the most visited button. Popup a context
    // menu.
    DCHECK(index < [self numberOfItems]);

    web::ContextMenuParams params;
    // Get view coordinates in local space.
    params.location = [sender locationInView:self.view];
    params.view.reset([self.view retain]);

    // Present sheet/popover using controller that is added to view hierarchy.
    UIViewController* topController = [params.view window].rootViewController;
    while (topController.presentedViewController)
      topController = topController.presentedViewController;

    _contextMenuCoordinator.reset([[ContextMenuCoordinator alloc]
        initWithBaseViewController:topController
                            params:params]);

    ProceduralBlock action;

    // Open In New Tab.
    GURL url = [self urlForIndex:index];
    base::WeakNSObject<GoogleLandingController> weakSelf(self);
    action = ^{
      base::scoped_nsobject<GoogleLandingController> strongSelf(
          [weakSelf retain]);
      if (!strongSelf)
        return;
      MostVisitedCell* cell = (MostVisitedCell*)sender.view;
      [strongSelf logMostVisitedClick:index tileType:cell.tileType];
      [[strongSelf loader] webPageOrderedOpen:url
                                     referrer:web::Referrer()
                                 inBackground:YES
                                     appendTo:kCurrentTab];
    };
    [_contextMenuCoordinator
        addItemWithTitle:l10n_util::GetNSStringWithFixup(
                             IDS_IOS_CONTENT_CONTEXT_OPENLINKNEWTAB)
                  action:action];

    if (!_browserState->IsOffTheRecord()) {
      // Open in Incognito Tab.
      action = ^{
        base::scoped_nsobject<GoogleLandingController> strongSelf(
            [weakSelf retain]);
        if (!strongSelf)
          return;
        MostVisitedCell* cell = (MostVisitedCell*)sender.view;
        [strongSelf logMostVisitedClick:index tileType:cell.tileType];
        [[strongSelf loader] webPageOrderedOpen:url
                                       referrer:web::Referrer()
                                    inIncognito:YES
                                   inBackground:NO
                                       appendTo:kCurrentTab];
      };
      [_contextMenuCoordinator
          addItemWithTitle:l10n_util::GetNSStringWithFixup(
                               IDS_IOS_CONTENT_CONTEXT_OPENLINKNEWINCOGNITOTAB)
                    action:action];
    }

    // Remove the most visited url.
    NSString* title =
        l10n_util::GetNSStringWithFixup(IDS_BOOKMARK_BUBBLE_REMOVE_BOOKMARK);
    action = ^{
      base::scoped_nsobject<GoogleLandingController> strongSelf(
          [weakSelf retain]);
      // Early return if the controller has been deallocated.
      if (!strongSelf)
        return;
      base::RecordAction(UserMetricsAction("MostVisited_UrlBlacklisted"));
      [strongSelf addBlacklistedURL:url];
      [strongSelf showMostVisitedUndoForURL:net::NSURLWithGURL(url)];
    };
    [_contextMenuCoordinator addItemWithTitle:title action:action];

    [_contextMenuCoordinator start];

    if (IsIPadIdiom())
      [self blurOmnibox];
  }
}

- (void)showMostVisitedUndoForURL:(NSURL*)url {
  _deletedUrl.reset([url retain]);

  MDCSnackbarMessageAction* action =
      [[[MDCSnackbarMessageAction alloc] init] autorelease];
  base::WeakNSObject<GoogleLandingController> weakSelf(self);
  action.handler = ^{
    base::scoped_nsobject<GoogleLandingController> strongSelf(
        [weakSelf retain]);
    if (!strongSelf)
      return;
    [strongSelf removeBlacklistedURL:net::GURLWithNSURL(_deletedUrl)];
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

- (void)onPromoLabelTapped {
  [_focuser cancelOmniboxEdit];
  _notification_promo->HandleClosed();
  [_promoHeaderView setHidden:YES];
  [self.view setNeedsLayout];

  if (_notification_promo->IsURLPromo()) {
    [_loader webPageOrderedOpen:_notification_promo->url()
                       referrer:web::Referrer()
                   inBackground:NO
                       appendTo:kCurrentTab];
    _notification_promo.reset();
    return;
  }

  if (_notification_promo->IsChromeCommand()) {
    base::scoped_nsobject<GenericChromeCommand> command(
        [[GenericChromeCommand alloc]
            initWithTag:_notification_promo->command_id()]);
    [self.view chromeExecuteCommand:command];
    _notification_promo.reset();
    return;
  }

  NOTREACHED();
}

// Returns the Y value to use for the scroll view's contentOffset when scrolling
// the omnibox to the top of the screen.
- (CGFloat)pinnedOffsetY {
  CGFloat headerHeight = [_headerView frame].size.height;
  CGFloat offsetY =
      headerHeight - ntp_header::kScrolledToTopOmniboxBottomMargin;
  if (!IsIPadIdiom())
    offsetY -= ntp_header::kToolbarHeight;

  return offsetY;
}

#pragma mark - NewTabPagePanelProtocol

- (void)reload {
  // Fetch the doodle after the view finishes laying out. Otherwise, tablet
  // may fetch the wrong sized doodle.
  if (_viewLoaded)
    [_doodleController fetchDoodle];
  [self updateLogoAndFakeboxDisplay];
  [self hideWhatsNewIfNecessary];
}

- (void)wasShown {
  _isShowing = YES;
  [_headerView hideToolbarViewsForNewTabPage];
}

- (void)wasHidden {
  _isShowing = NO;
}

- (void)dismissModals {
  [_contextMenuCoordinator stop];
}

- (void)dismissKeyboard {
}

- (void)setScrollsToTop:(BOOL)enable {
}

- (CGFloat)alphaForBottomShadow {
  // Get the frame of the bottommost cell in |_view|'s coordinate system.
  NSInteger section = SectionWithMostVisited;
  // Account for the fact that the tableview may not yet contain
  // |numberOfNonEmptyTilesShown| tiles because it hasn't been updated yet.
  NSUInteger lastItemIndex =
      std::min([_mostVisitedView numberOfItemsInSection:SectionWithMostVisited],
               [self numberOfNonEmptyTilesShown]) -
      1;
  DCHECK(lastItemIndex >= 0);
  NSIndexPath* lastCellIndexPath =
      [NSIndexPath indexPathForItem:lastItemIndex inSection:section];
  UICollectionViewLayoutAttributes* attributes =
      [_mostVisitedView layoutAttributesForItemAtIndexPath:lastCellIndexPath];
  CGRect lastCellFrame = attributes.frame;
  CGRect cellFrameInSuperview =
      [_mostVisitedView convertRect:lastCellFrame toView:self.view];

  // Calculate when the bottom of the cell passes through the bottom of |_view|.
  CGFloat maxY = CGRectGetMaxY(cellFrameInSuperview);
  CGFloat viewHeight = CGRectGetHeight(self.view.frame);

  CGFloat pixelsBelowFrame = maxY - viewHeight;
  CGFloat alpha = pixelsBelowFrame / kNewTabPageDistanceToFadeShadow;
  alpha = MIN(MAX(alpha, 0), 1);
  return alpha;
}

- (UIView*)view {
  return _view;
}

#pragma mark - LogoAnimationControllerOwnerOwner

- (id<LogoAnimationControllerOwner>)logoAnimationControllerOwner {
  return [_doodleController logoAnimationControllerOwner];
}

#pragma mark - UIScrollViewDelegate Methods.

- (void)scrollViewDidScroll:(UIScrollView*)scrollView {
  [self.delegate updateNtpBarShadowForPanelController:self];
  [_overscrollActionsController scrollViewDidScroll:scrollView];

  // Blur the omnibox when the scroll view is scrolled below the pinned offset.
  CGFloat pinnedOffsetY = [self pinnedOffsetY];
  if (_omniboxFocused && scrollView.dragging &&
      scrollView.contentOffset.y < pinnedOffsetY) {
    [_focuser cancelOmniboxEdit];
  }

  if (IsIPadIdiom()) {
    return;
  }

  if (_animateHeader) {
    [self updateSearchField];
  }
}

- (void)scrollViewWillBeginDragging:(UIScrollView*)scrollView {
  [_overscrollActionsController scrollViewWillBeginDragging:scrollView];
}

- (void)scrollViewDidEndDragging:(UIScrollView*)scrollView
                  willDecelerate:(BOOL)decelerate {
  [_overscrollActionsController scrollViewDidEndDragging:scrollView
                                          willDecelerate:decelerate];
}

- (void)scrollViewWillEndDragging:(UIScrollView*)scrollView
                     withVelocity:(CGPoint)velocity
              targetContentOffset:(inout CGPoint*)targetContentOffset {
  [_overscrollActionsController scrollViewWillEndDragging:scrollView
                                             withVelocity:velocity
                                      targetContentOffset:targetContentOffset];

  if (IsIPadIdiom() || _omniboxFocused)
    return;

  CGFloat pinnedOffsetY = [self pinnedOffsetY];
  CGFloat offsetY = scrollView.contentOffset.y;
  CGFloat targetY = targetContentOffset->y;
  if (offsetY > 0 && offsetY < pinnedOffsetY) {
    // Omnibox is currently between middle and top of screen.
    if (velocity.y > 0) {  // scrolling upwards
      if (targetY < pinnedOffsetY) {
        // Scroll the omnibox up to |pinnedOffsetY| if velocity is upwards but
        // scrolling will stop before reaching |pinnedOffsetY|.
        targetContentOffset->y = offsetY;
        [_mostVisitedView setContentOffset:CGPointMake(0, pinnedOffsetY)
                                  animated:YES];
      }
      _scrolledToTop = YES;
    } else {  // scrolling downwards
      if (targetY > 0) {
        // Scroll the omnibox down to zero if velocity is downwards or 0 but
        // scrolling will stop before reaching 0.
        targetContentOffset->y = offsetY;
        [_mostVisitedView setContentOffset:CGPointZero animated:YES];
      }
      _scrolledToTop = NO;
    }
  } else if (offsetY > pinnedOffsetY &&
             targetContentOffset->y < pinnedOffsetY) {
    // Most visited cells are currently scrolled up past the omnibox but will
    // end the scroll below the omnibox. Stop the scroll at just below the
    // omnibox.
    targetContentOffset->y = offsetY;
    [_mostVisitedView setContentOffset:CGPointMake(0, pinnedOffsetY)
                              animated:YES];
    _scrolledToTop = YES;
  } else if (offsetY >= pinnedOffsetY) {
    _scrolledToTop = YES;
  } else if (offsetY <= 0) {
    _scrolledToTop = NO;
  }
}

#pragma mark - Most visited / Suggestions service wrapper methods.

- (suggestions::SuggestionsService*)suggestionsService {
  return suggestions::SuggestionsServiceFactory::GetForBrowserState(
      _browserState);
}

- (NSUInteger)numberOfItems {
  NSUInteger numItems = _mostVisitedData.size();
  NSUInteger maxItems = [self numberOfColumns] * kMaxNumMostVisitedFaviconRows;
  return MIN(maxItems, numItems);
}

- (NSInteger)numberOfNonEmptyTilesShown {
  NSInteger numCells = MIN([self numberOfItems], _maxNumMostVisited);
  return MAX(numCells, [self numberOfColumns]);
}

- (GURL)urlForIndex:(NSUInteger)index {
  return _mostVisitedData[index].url;
}

- (void)addBlacklistedURL:(const GURL&)url {
  _most_visited_sites->AddOrRemoveBlacklistedUrl(url, true);
}

- (void)removeBlacklistedURL:(const GURL&)url {
  _most_visited_sites->AddOrRemoveBlacklistedUrl(url, false);
}

#pragma mark - GoogleLandingController (ExposedForTesting) methods.

- (BOOL)scrolledToTop {
  return _scrolledToTop;
}

- (BOOL)animateHeader {
  return _animateHeader;
}

#pragma mark - OverscrollActionsControllerDelegate

- (void)overscrollActionsController:(OverscrollActionsController*)controller
                   didTriggerAction:(OverscrollAction)action {
  switch (action) {
    case OverscrollAction::NEW_TAB: {
      base::scoped_nsobject<GenericChromeCommand> command(
          [[GenericChromeCommand alloc] initWithTag:IDC_NEW_TAB]);
      [[self view] chromeExecuteCommand:command];
    } break;
    case OverscrollAction::CLOSE_TAB: {
      base::scoped_nsobject<GenericChromeCommand> command(
          [[GenericChromeCommand alloc] initWithTag:IDC_CLOSE_TAB]);
      [[self view] chromeExecuteCommand:command];
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
  return [[_headerView toolBarView] snapshotViewAfterScreenUpdates:NO];
}

- (UIView*)headerView {
  return self.view;
}

- (CGFloat)overscrollActionsControllerHeaderInset:
    (OverscrollActionsController*)controller {
  return 0;
}

- (CGFloat)overscrollHeaderHeight {
  return [_headerView toolBarView].bounds.size.height;
}

#pragma mark - UIGestureRecognizerDelegate

- (BOOL)gestureRecognizer:(UIGestureRecognizer*)gestureRecognizer
       shouldReceiveTouch:(UITouch*)touch {
  return [self nearestAncestorOfView:touch.view
                           withClass:[MostVisitedCell class]] == nil;
}

- (UIView*)nearestAncestorOfView:(UIView*)view withClass:(Class)aClass {
  if (!view) {
    return nil;
  }
  if ([view isKindOfClass:aClass]) {
    return view;
  }
  return [self nearestAncestorOfView:[view superview] withClass:aClass];
}

@end
