// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/ntp/new_tab_page_controller.h"

#import <QuartzCore/QuartzCore.h>

#include "base/logging.h"

#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/template_url_service.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync_sessions/synced_session.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/content_suggestions/content_suggestions_coordinator.h"
#import "ios/chrome/browser/content_suggestions/content_suggestions_header_view_controller.h"
#include "ios/chrome/browser/experimental_flags.h"
#include "ios/chrome/browser/pref_names.h"
#include "ios/chrome/browser/search_engines/template_url_service_factory.h"
#include "ios/chrome/browser/sync/sync_setup_service.h"
#include "ios/chrome/browser/sync/sync_setup_service_factory.h"
#import "ios/chrome/browser/tabs/tab_model.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_controller_factory.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_home_tablet_ntp_controller.h"
#import "ios/chrome/browser/ui/commands/browser_commands.h"
#import "ios/chrome/browser/ui/content_suggestions/ntp_home_constant.h"
#import "ios/chrome/browser/ui/ntp/google_landing_mediator.h"
#import "ios/chrome/browser/ui/ntp/google_landing_view_controller.h"
#import "ios/chrome/browser/ui/ntp/incognito_view_controller.h"
#import "ios/chrome/browser/ui/ntp/modal_ntp.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_bar_item.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_view.h"
#import "ios/chrome/browser/ui/ntp/recent_tabs/recent_tabs_table_coordinator.h"
#import "ios/chrome/browser/ui/rtl_geometry.h"
#include "ios/chrome/browser/ui/toolbar/toolbar_model_ios.h"
#include "ios/chrome/browser/ui/ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/web/web_state/ui/crw_swipe_recognizer_provider.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::UserMetricsAction;

namespace {
const char* kMostVisitedFragment = "most_visited";
const char* kBookmarksFragment = "bookmarks";
const char* kOpenTabsFragment = "open_tabs";
const char* kIncognitoFragment = "incognito";
const CGFloat kToolbarHeight = 56;
}

namespace NewTabPage {

// Converts from a URL #fragment string to an identifier.
// Defaults to ntp_home::NONE if the fragment is nil or not recognized.
// The strings checked by this function matches the set of fragments
// supported by chrome://newtab/# on Android.
// See chrome/browser/resources/mobile_ntp/mobile_ntp.js
ntp_home::PanelIdentifier IdentifierFromFragment(const std::string& fragment) {
  if (fragment == kMostVisitedFragment)
    return ntp_home::HOME_PANEL;
  else if (fragment == kBookmarksFragment)
    return ntp_home::BOOKMARKS_PANEL;
  else if (fragment == kOpenTabsFragment)
    return ntp_home::RECENT_TABS_PANEL;
  else if (fragment == kIncognitoFragment)
    return ntp_home::INCOGNITO_PANEL;
  else
    return ntp_home::NONE;
}

// Converts from a ntp_home::PanelIdentifier to a URL #fragment string.
// Defaults to nil if the fragment is NONE or not recognized.
std::string FragmentFromIdentifier(ntp_home::PanelIdentifier panel) {
  switch (panel) {
    case ntp_home::NONE:
      return "";
    case ntp_home::HOME_PANEL:
      return kMostVisitedFragment;
    case ntp_home::BOOKMARKS_PANEL:
      return kBookmarksFragment;
    case ntp_home::RECENT_TABS_PANEL:
      return kOpenTabsFragment;
    case ntp_home::INCOGNITO_PANEL:
      return kIncognitoFragment;
    default:
      NOTREACHED();
      return "";
  }
}

}  // namespace NewTabPage

namespace {

// TODO(pkl): These are private constants and enums from
// ui/webui/ntp/new_tab_page_handler.h. At some point these should be
// refactored out so they can be included instead of redefined here.
const int kPageIdOffset = 10;
enum {
  INDEX_MASK = (1 << kPageIdOffset) - 1,
  MOST_VISITED_PAGE_ID = 1 << kPageIdOffset,
  BOOKMARKS_PAGE_ID = 3 << kPageIdOffset,
  OPEN_TABS_PAGE_ID = 4 << kPageIdOffset,
};

}  // anonymous namespace

@interface NewTabPageController () {
  ios::ChromeBrowserState* _browserState;  // weak.
  __weak id<UrlLoader> _loader;
  __weak id<NewTabPageControllerObserver> _newTabPageObserver;
  BookmarkHomeTabletNTPController* _bookmarkController;
  GoogleLandingViewController* _googleLandingController;
  IncognitoViewController* _incognitoController;
  // The currently visible controller, one of the above.
  __weak id<NewTabPagePanelProtocol> _currentController;

  GoogleLandingMediator* _googleLandingMediator;

  RecentTabsTableCoordinator* _openTabsCoordinator;
  // Has the scrollView been initialized.
  BOOL _scrollInitialized;

  // Dominant color cache. Key: (NSString*)url, val: (UIColor*)dominantColor.
  __weak NSMutableDictionary* _dominantColorCache;  // Owned by bvc.

  // Delegate to focus and blur the omnibox.
  __weak id<OmniboxFocuser> _focuser;

  // Delegate to fetch the ToolbarModel and current web state from.
  __weak id<IncognitoViewControllerDelegate> _toolbarDelegate;

  TabModel* _tabModel;
}

// Load and bring panel into view.
- (void)showPanel:(NewTabPageBarItem*)item;
// Load panel on demand.
- (BOOL)loadPanel:(NewTabPageBarItem*)item;
// After a panel changes, update metrics and prefs information.
- (void)panelChanged:(NewTabPageBarItem*)item;
// Update current controller and tab bar index.  Used to call reload.
- (void)updateCurrentController:(NewTabPageBarItem*)item
                          index:(NSUInteger)index;
// Bring panel into scroll view.
- (void)scrollToPanel:(NewTabPageBarItem*)item animate:(BOOL)animate;
// Returns index of item in tab bar.
- (NSUInteger)tabBarItemIndex:(NewTabPageBarItem*)item;
// Call loadPanel by item index.
- (void)loadControllerWithIndex:(NSUInteger)index;
// Initialize scroll view.
- (void)setUpScrollView;
// Update overlay scroll view value.
- (void)updateOverlayScrollPosition;
// Disable the horizontal scroll view.
- (void)disableScroll;
// Enable the horizontal scroll view.
- (void)enableScroll;
// Returns the ID for the currently selected panel.
- (ntp_home::PanelIdentifier)selectedPanelID;

@property(nonatomic, strong) NewTabPageView* view;

// To ease modernizing the NTP only the internal panels are being converted
// to UIViewControllers.  This means all the plumbing between the
// BrowserViewController and the internal NTP panels (WebController, NTP)
// hierarchy is skipped.  While normally the logic to push and pop a view
// controller would be owned by a coordinator, in this case the old NTP
// controller adds and removes child view controllers itself when a load
// is initiated, and when WebController calls -willBeDismissed.
@property(nonatomic, weak) UIViewController* parentViewController;

// The command dispatcher.
@property(nonatomic, weak) id<ApplicationCommands,
                              BrowserCommands,
                              OmniboxFocuser,
                              UrlLoader>
    dispatcher;

// Panel displaying the "Home" view, with the logo and the fake omnibox.
@property(nonatomic, strong) id<NewTabPagePanelProtocol> homePanel;

// Coordinator for the ContentSuggestions.
@property(nonatomic, strong)
    ContentSuggestionsCoordinator* contentSuggestionsCoordinator;

// Controller for the header of the Home panel.
@property(nonatomic, strong) id<LogoAnimationControllerOwnerOwner, ToolbarOwner>
    headerController;

@end

@implementation NewTabPageController

@synthesize view = _view;
@synthesize swipeRecognizerProvider = _swipeRecognizerProvider;
@synthesize parentViewController = _parentViewController;
@synthesize dispatcher = _dispatcher;
@synthesize homePanel = _homePanel;
@synthesize contentSuggestionsCoordinator = _contentSuggestionsCoordinator;
@synthesize headerController = _headerController;

- (id)initWithUrl:(const GURL&)url
                  loader:(id<UrlLoader>)loader
                 focuser:(id<OmniboxFocuser>)focuser
             ntpObserver:(id<NewTabPageControllerObserver>)ntpObserver
            browserState:(ios::ChromeBrowserState*)browserState
              colorCache:(NSMutableDictionary*)colorCache
         toolbarDelegate:(id<IncognitoViewControllerDelegate>)toolbarDelegate
                tabModel:(TabModel*)tabModel
    parentViewController:(UIViewController*)parentViewController
              dispatcher:(id<ApplicationCommands,
                             BrowserCommands,
                             OmniboxFocuser,
                             UrlLoader>)dispatcher {
  self = [super initWithNibName:nil url:url];
  if (self) {
    DCHECK(browserState);
    _browserState = browserState;
    _loader = loader;
    _newTabPageObserver = ntpObserver;
    _parentViewController = parentViewController;
    _dispatcher = dispatcher;
    _focuser = focuser;
    _toolbarDelegate = toolbarDelegate;
    _tabModel = tabModel;
    _dominantColorCache = colorCache;
    self.title = l10n_util::GetNSString(IDS_NEW_TAB_TITLE);
    _scrollInitialized = NO;

    UIScrollView* scrollView =
        [[UIScrollView alloc] initWithFrame:CGRectMake(0, 0, 320, 412)];
    [scrollView setAutoresizingMask:(UIViewAutoresizingFlexibleWidth |
                                     UIViewAutoresizingFlexibleHeight)];
    NewTabPageBar* tabBar =
        [[NewTabPageBar alloc] initWithFrame:CGRectMake(0, 412, 320, 48)];
    _view = [[NewTabPageView alloc] initWithFrame:CGRectMake(0, 0, 320, 460)
                                    andScrollView:scrollView
                                        andTabBar:tabBar];
    [tabBar setDelegate:self];

    bool isIncognito = _browserState->IsOffTheRecord();

    NSString* incognito = l10n_util::GetNSString(IDS_IOS_NEW_TAB_INCOGNITO);
    NSString* home = experimental_flags::IsSuggestionsUIEnabled()
                         ? l10n_util::GetNSString(IDS_IOS_NEW_TAB_HOME)
                         : l10n_util::GetNSString(IDS_IOS_NEW_TAB_MOST_VISITED);
    NSString* bookmarks =
        l10n_util::GetNSString(IDS_IOS_NEW_TAB_BOOKMARKS_PAGE_TITLE_MOBILE);
    NSString* openTabs = l10n_util::GetNSString(IDS_IOS_NEW_TAB_RECENT_TABS);

    NSMutableArray* tabBarItems = [NSMutableArray array];
    NewTabPageBarItem* itemToDisplay = nil;
    if (isIncognito) {
      NewTabPageBarItem* incognitoItem = [NewTabPageBarItem
          newTabPageBarItemWithTitle:incognito
                          identifier:ntp_home::INCOGNITO_PANEL
                               image:[UIImage imageNamed:@"ntp_incognito"]];
      if (!PresentNTPPanelModally()) {
        // Only add the bookmarks tab item for Incognito.
        NewTabPageBarItem* bookmarksItem = [NewTabPageBarItem
            newTabPageBarItemWithTitle:bookmarks
                            identifier:ntp_home::BOOKMARKS_PANEL
                                 image:[UIImage imageNamed:@"ntp_bookmarks"]];
        [tabBarItems addObject:bookmarksItem];
        [tabBarItems addObject:incognitoItem];
        self.view.tabBar.items = tabBarItems;
      }
      itemToDisplay = incognitoItem;
    } else {
      NewTabPageBarItem* homeItem = [NewTabPageBarItem
          newTabPageBarItemWithTitle:home
                          identifier:ntp_home::HOME_PANEL
                               image:[UIImage imageNamed:@"ntp_mv_search"]];
      NewTabPageBarItem* bookmarksItem = [NewTabPageBarItem
          newTabPageBarItemWithTitle:bookmarks
                          identifier:ntp_home::BOOKMARKS_PANEL
                               image:[UIImage imageNamed:@"ntp_bookmarks"]];
      [tabBarItems addObject:bookmarksItem];
      if (!PresentNTPPanelModally()) {
        [tabBarItems addObject:homeItem];
      }

      NewTabPageBarItem* openTabsItem = [NewTabPageBarItem
          newTabPageBarItemWithTitle:openTabs
                          identifier:ntp_home::RECENT_TABS_PANEL
                               image:[UIImage imageNamed:@"ntp_opentabs"]];
      [tabBarItems addObject:openTabsItem];
      self.view.tabBar.items = tabBarItems;

      if (PresentNTPPanelModally()) {
        itemToDisplay = homeItem;
      } else {
        PrefService* prefs = _browserState->GetPrefs();
        int shownPage = prefs->GetInteger(prefs::kNtpShownPage);
        shownPage = shownPage & ~INDEX_MASK;

        if (shownPage == BOOKMARKS_PAGE_ID) {
          itemToDisplay = bookmarksItem;
        } else if (shownPage == OPEN_TABS_PAGE_ID) {
          itemToDisplay = openTabsItem;
        } else {
          itemToDisplay = homeItem;
        }
      }
    }
    DCHECK(itemToDisplay);
    [self setUpScrollView];
    [self showPanel:itemToDisplay];
    [self updateOverlayScrollPosition];
  }
  return self;
}

- (void)dealloc {
  // Animations can last past the life of the NTP controller, nil out the
  // delegate.
  self.view.scrollView.delegate = nil;

  [_googleLandingMediator shutdown];

  // This is not an ideal place to put view controller contaimnent, rather a
  // //web -wasDismissed method on CRWNativeContent would be more accurate. If
  // CRWNativeContent leaks, this will not be called.
  [_googleLandingController removeFromParentViewController];
  [_bookmarkController removeFromParentViewController];
  [_incognitoController removeFromParentViewController];
  [[self.contentSuggestionsCoordinator viewController]
      removeFromParentViewController];
  [[_openTabsCoordinator viewController] removeFromParentViewController];

  [self.contentSuggestionsCoordinator stop];
  [_openTabsCoordinator stop];

  [self.homePanel setDelegate:nil];
  [_bookmarkController setDelegate:nil];
  [_openTabsCoordinator setDelegate:nil];
  [[NSNotificationCenter defaultCenter] removeObserver:self];
}

#pragma mark - CRWNativeContent

- (void)willBeDismissed {
  // This methods is called by //web immediately before |self|'s view is removed
  // from the view hierarchy, making it an ideal spot to intiate view controller
  // containment methods.
  [_googleLandingController willMoveToParentViewController:nil];
  [_bookmarkController willMoveToParentViewController:nil];
  [[_openTabsCoordinator viewController] willMoveToParentViewController:nil];
  [[self.contentSuggestionsCoordinator viewController]
      willMoveToParentViewController:nil];
  [_incognitoController willMoveToParentViewController:nil];
}

- (void)reload {
  [_currentController reload];
  [super reload];
}

- (void)wasShown {
  [_currentController wasShown];
  if (_currentController != self.homePanel) {
    // Ensure that the NTP has the latest data when it is shown, except for
    // Home.
    [self reload];
  }
  [self.view.tabBar updateColorsForScrollView:self.view.scrollView];
  [self.view.tabBar setShadowAlpha:[_currentController alphaForBottomShadow]];
}

- (void)wasHidden {
  [_currentController wasHidden];
}

- (BOOL)wantsKeyboardShield {
  return [self selectedPanelID] != ntp_home::HOME_PANEL;
}

- (BOOL)wantsLocationBarHintText {
  // Always show hint text on iPhone.
  if (!IsIPadIdiom())
    return YES;
  // Always show the location bar hint text if the search engine is not Google.
  TemplateURLService* service =
      ios::TemplateURLServiceFactory::GetForBrowserState(_browserState);
  if (service) {
    const TemplateURL* defaultURL = service->GetDefaultSearchProvider();
    if (defaultURL &&
        defaultURL->GetEngineType(service->search_terms_data()) !=
            SEARCH_ENGINE_GOOGLE) {
      return YES;
    }
  }

  // Always return true when incognito.
  if (_browserState->IsOffTheRecord())
    return YES;

  return [self selectedPanelID] != ntp_home::HOME_PANEL;
}

- (void)dismissKeyboard {
  [_currentController dismissKeyboard];
}

- (void)dismissModals {
  [_currentController dismissModals];
}

- (void)willUpdateSnapshot {
  if ([_currentController respondsToSelector:@selector(willUpdateSnapshot)]) {
    [_currentController willUpdateSnapshot];
  }
}

- (CGPoint)scrollOffset {
  if (_currentController == self.homePanel &&
      [self.homePanel respondsToSelector:@selector(scrollOffset)]) {
    return [self.homePanel scrollOffset];
  }
  return CGPointZero;
}

#pragma mark -

- (void)setSwipeRecognizerProvider:(id<CRWSwipeRecognizerProvider>)provider {
  _swipeRecognizerProvider = provider;
  NSSet* recognizers = [_swipeRecognizerProvider swipeRecognizers];
  for (UISwipeGestureRecognizer* swipeRecognizer in recognizers) {
    [self.view.scrollView.panGestureRecognizer
        requireGestureRecognizerToFail:swipeRecognizer];
  }
}

- (void)setUpScrollView {
  NSNotificationCenter* defaultCenter = [NSNotificationCenter defaultCenter];
  [defaultCenter addObserver:self
                    selector:@selector(disableScroll)
                        name:UIKeyboardWillShowNotification
                      object:nil];
  [defaultCenter addObserver:self
                    selector:@selector(enableScroll)
                        name:UIKeyboardWillHideNotification
                      object:nil];

  UIScrollView* scrollView = self.view.scrollView;
  scrollView.pagingEnabled = YES;
  scrollView.showsHorizontalScrollIndicator = NO;
  scrollView.showsVerticalScrollIndicator = NO;
  scrollView.contentMode = UIViewContentModeScaleAspectFit;
  scrollView.bounces = YES;
  scrollView.delegate = self;
  scrollView.scrollsToTop = NO;

  [self.view updateScrollViewContentSize];
  [self.view.tabBar updateColorsForScrollView:scrollView];

  _scrollInitialized = YES;
}

- (void)disableScroll {
  [self.view.scrollView setScrollEnabled:NO];
}

- (void)enableScroll {
  [self.view.scrollView setScrollEnabled:YES];
}

// Update selectedIndex and scroll position as the scroll view moves.
- (void)scrollViewDidScroll:(UIScrollView*)scrollView {
  if (!_scrollInitialized)
    return;

  // Position is used to track the exact X position of the scroll view, whereas
  // index is rounded to the panel that is most visible.
  CGFloat panelWidth =
      scrollView.contentSize.width / self.view.tabBar.items.count;
  LayoutOffset position =
      LeadingContentOffsetForScrollView(scrollView) / panelWidth;
  NSUInteger index = round(position);

  // |scrollView| can be out of range when the frame changes.
  if (index >= self.view.tabBar.items.count)
    return;

  // Only create views when they need to be visible.  This will create a slight
  // jank on first creation, but it doesn't seem very noticeable.  The trade off
  // is loading the adjacent panels, and a longer initial NTP startup.
  if (position - index > 0)
    [self loadControllerWithIndex:index + 1];
  [self loadControllerWithIndex:index];
  if (position - index < 0)
    [self loadControllerWithIndex:index - 1];

  // If index changed, follow same path as if a tab bar item was pressed.  When
  // |index| == |position|, the panel is completely in view.
  if (index == position && self.view.tabBar.selectedIndex != index) {
    NewTabPageBarItem* item = [self.view.tabBar.items objectAtIndex:index];
    DCHECK(item);
    self.view.tabBar.selectedIndex = index;
    [self updateCurrentController:item index:index];
    [self newTabBarItemDidChange:item changePanel:NO];
  }

  [self.view.tabBar updateColorsForScrollView:scrollView];
  [self updateOverlayScrollPosition];
}

- (void)scrollViewDidEndScrollingAnimation:(UIScrollView*)scrollView {
  NSUInteger index = self.view.tabBar.selectedIndex;
  NewTabPageBarItem* item = [self.view.tabBar.items objectAtIndex:index];
  DCHECK(item);
  [self updateCurrentController:item index:index];
}

// Called when the user presses a segment that's not currently selected.
// Pressing a segment that's already selected does not trigger this action.
- (void)newTabBarItemDidChange:(NewTabPageBarItem*)selectedItem
                   changePanel:(BOOL)changePanel {
  [self panelChanged:selectedItem];
  if (changePanel) {
    [self scrollToPanel:selectedItem animate:YES];
  }

  [_newTabPageObserver selectedPanelDidChange];
}

- (void)selectPanel:(ntp_home::PanelIdentifier)panelType {
  for (NewTabPageBarItem* item in self.view.tabBar.items) {
    if (item.identifier == panelType) {
      [self showPanel:item];
      return;  // Early return after finding the first match.
    }
  }
}

- (void)showPanel:(NewTabPageBarItem*)item {
  if ([self loadPanel:item]) {
    // Intentionally omitting a metric for the Incognito panel.
    if (item.identifier == ntp_home::BOOKMARKS_PANEL)
      base::RecordAction(UserMetricsAction("MobileNTPShowBookmarks"));
    else if (item.identifier == ntp_home::HOME_PANEL)
      base::RecordAction(UserMetricsAction("MobileNTPShowMostVisited"));
    else if (item.identifier == ntp_home::RECENT_TABS_PANEL)
      base::RecordAction(UserMetricsAction("MobileNTPShowOpenTabs"));
  }
  [self scrollToPanel:item animate:NO];
}

- (void)loadControllerWithIndex:(NSUInteger)index {
  if (index >= self.view.tabBar.items.count)
    return;

  NewTabPageBarItem* item = [self.view.tabBar.items objectAtIndex:index];
  [self loadPanel:item];
}

- (BOOL)loadPanel:(NewTabPageBarItem*)item {
  DCHECK(self.parentViewController);
  UIViewController* panelController = nil;
  // Only load the controllers once.
  if (item.identifier == ntp_home::BOOKMARKS_PANEL) {
    if (!_bookmarkController) {
      BookmarkControllerFactory* factory =
          [[BookmarkControllerFactory alloc] init];
      _bookmarkController =
          [factory bookmarkPanelControllerForBrowserState:_browserState
                                                   loader:_loader
                                               dispatcher:self.dispatcher];
    }
    panelController = _bookmarkController;
    [_bookmarkController setDelegate:self];
  } else if (item.identifier == ntp_home::HOME_PANEL) {
    if (experimental_flags::IsSuggestionsUIEnabled()) {
      if (!self.contentSuggestionsCoordinator) {
        self.contentSuggestionsCoordinator =
            [[ContentSuggestionsCoordinator alloc]
                initWithBaseViewController:nil];
        self.contentSuggestionsCoordinator.URLLoader = _loader;
        self.contentSuggestionsCoordinator.browserState = _browserState;
        self.contentSuggestionsCoordinator.dispatcher = self.dispatcher;
        self.contentSuggestionsCoordinator.webStateList =
            [_tabModel webStateList];
        [self.contentSuggestionsCoordinator start];
        self.headerController =
            self.contentSuggestionsCoordinator.headerController;
      }
      panelController = [self.contentSuggestionsCoordinator viewController];
      self.homePanel = self.contentSuggestionsCoordinator;
    } else {
      if (!_googleLandingController) {
        _googleLandingController = [[GoogleLandingViewController alloc] init];
        [_googleLandingController setDispatcher:self.dispatcher];
        _googleLandingMediator = [[GoogleLandingMediator alloc]
            initWithBrowserState:_browserState
                    webStateList:[_tabModel webStateList]];
        _googleLandingMediator.consumer = _googleLandingController;
        _googleLandingMediator.dispatcher = self.dispatcher;
        [_googleLandingMediator setUp];

        [_googleLandingController setDataSource:_googleLandingMediator];
        self.headerController = _googleLandingController;
      }
      panelController = _googleLandingController;
      self.homePanel = _googleLandingController;
    }
    [self.homePanel setDelegate:self];
  } else if (item.identifier == ntp_home::RECENT_TABS_PANEL) {
    if (!_openTabsCoordinator) {
      _openTabsCoordinator =
          [[RecentTabsTableCoordinator alloc] initWithLoader:_loader
                                                browserState:_browserState
                                                  dispatcher:self.dispatcher];
      [_openTabsCoordinator start];
    }
    panelController = [_openTabsCoordinator viewController];
    [_openTabsCoordinator setDelegate:self];
  } else if (item.identifier == ntp_home::INCOGNITO_PANEL) {
    if (!_incognitoController)
      _incognitoController =
          [[IncognitoViewController alloc] initWithLoader:_loader
                                          toolbarDelegate:_toolbarDelegate];
    panelController = _incognitoController;
  } else {
    NOTREACHED();
    return NO;
  }

  UIView* view = panelController.view;
  if (item.identifier == ntp_home::HOME_PANEL) {
    // Update the shadow for the toolbar after the view creation.
    [self.view.tabBar setShadowAlpha:[self.homePanel alphaForBottomShadow]];
  }

  // Add the panel views to the scroll view in the proper location.
  NSUInteger index = [self tabBarItemIndex:item];
  BOOL created = NO;
  if (view.superview == nil) {
    created = YES;
    view.frame = [self.view panelFrameForItemAtIndex:index];
    item.view = view;

    // To ease modernizing the NTP only the internal panels are being converted
    // to UIViewControllers.  This means all the plumbing between the
    // BrowserViewController and the internal NTP panels (WebController, NTP)
    // hierarchy is skipped.  While normally the logic to push and pop a view
    // controller would be owned by a coordinator, in this case the old NTP
    // controller adds and removes child view controllers itself when a load
    // is initiated, and when WebController calls -willBeDismissed.
    DCHECK(panelController);
    [self.parentViewController addChildViewController:panelController];
    [self.view.scrollView addSubview:view];
    [panelController didMoveToParentViewController:self.parentViewController];
  }
  return created;
}

- (void)scrollToPanel:(NewTabPageBarItem*)item animate:(BOOL)animate {
  NSUInteger index = [self tabBarItemIndex:item];
  if (!PresentNTPPanelModally()) {
    CGRect itemFrame = [self.view panelFrameForItemAtIndex:index];
    CGPoint point = CGPointMake(CGRectGetMinX(itemFrame), 0);
    [self.view.scrollView setContentOffset:point animated:animate];
  } else {
    if (item.identifier == ntp_home::BOOKMARKS_PANEL) {
      [self.dispatcher showBookmarksManager];
    } else if (item.identifier == ntp_home::RECENT_TABS_PANEL) {
      [self.dispatcher showRecentTabs];
    }
  }

  if (_currentController == nil) {
    [self updateCurrentController:item index:index];
  }
}

// Return the index of the tab item.  For iPhone always return 0 since the
// returned index is used to update the visible controller and scroll the NTP
// scroll view. None of this is applicable for iPhone.
- (NSUInteger)tabBarItemIndex:(NewTabPageBarItem*)item {
  NSUInteger index = 0;
  if (!PresentNTPPanelModally()) {
    index = [self.view.tabBar.items indexOfObject:item];
    DCHECK(index != NSNotFound);
  }
  return index;
}

- (ntp_home::PanelIdentifier)selectedPanelID {
  if (!PresentNTPPanelModally()) {
    // |selectedIndex| isn't meaningful here with modal buttons on iPhone.
    NSUInteger index = self.view.tabBar.selectedIndex;
    DCHECK(index != NSNotFound);
    NewTabPageBarItem* item = self.view.tabBar.items[index];
    return item.identifier;
  }
  return ntp_home::HOME_PANEL;
}

- (void)updateCurrentController:(NewTabPageBarItem*)item
                          index:(NSUInteger)index {
  if (PresentNTPPanelModally() &&
      (item.identifier == ntp_home::BOOKMARKS_PANEL ||
       item.identifier == ntp_home::RECENT_TABS_PANEL)) {
    // Don't update |_currentController| for iPhone since Bookmarks and Recent
    // Tabs are presented in a modal view controller.
    return;
  }

  id<NewTabPagePanelProtocol> oldController = _currentController;
  self.view.tabBar.selectedIndex = index;
  if (item.identifier == ntp_home::BOOKMARKS_PANEL)
    _currentController = _bookmarkController;
  else if (item.identifier == ntp_home::HOME_PANEL)
    _currentController = self.homePanel;
  else if (item.identifier == ntp_home::RECENT_TABS_PANEL)
    _currentController = _openTabsCoordinator;
  else if (item.identifier == ntp_home::INCOGNITO_PANEL)
    _currentController = _incognitoController;

  [_bookmarkController
      setScrollsToTop:(_currentController == _bookmarkController)];
  [self.homePanel setScrollsToTop:(_currentController == self.homePanel)];
  [_openTabsCoordinator
      setScrollsToTop:(_currentController == _openTabsCoordinator)];
  if (oldController) {
    [self.view.tabBar setShadowAlpha:[_currentController alphaForBottomShadow]];
  }

  if (oldController != _currentController) {
    [_currentController wasShown];
    [oldController wasHidden];
  }
}

- (void)panelChanged:(NewTabPageBarItem*)item {
  if (_browserState->IsOffTheRecord())
    return;

  // Save state and update metrics. Intentionally omitting a metric for the
  // Incognito panel.
  PrefService* prefs = _browserState->GetPrefs();
  if (item.identifier == ntp_home::BOOKMARKS_PANEL) {
    base::RecordAction(UserMetricsAction("MobileNTPSwitchToBookmarks"));
    prefs->SetInteger(prefs::kNtpShownPage, BOOKMARKS_PAGE_ID);
  } else if (item.identifier == ntp_home::HOME_PANEL) {
    base::RecordAction(UserMetricsAction("MobileNTPSwitchToMostVisited"));
    prefs->SetInteger(prefs::kNtpShownPage, MOST_VISITED_PAGE_ID);
  } else if (item.identifier == ntp_home::RECENT_TABS_PANEL) {
    base::RecordAction(UserMetricsAction("MobileNTPSwitchToOpenTabs"));
    prefs->SetInteger(prefs::kNtpShownPage, OPEN_TABS_PAGE_ID);
  }
}

- (void)updateOverlayScrollPosition {
  // Update overlay position. This moves the overlay animation on the tab bar.
  UIScrollView* scrollView = self.view.scrollView;
  if (!scrollView || scrollView.contentSize.width == 0.0)
    return;
  self.view.tabBar.overlayPercentage =
      scrollView.contentOffset.x / scrollView.contentSize.width;
}

#pragma mark - LogoAnimationControllerOwnerOwner

- (id<LogoAnimationControllerOwner>)logoAnimationControllerOwner {
  return [self.headerController logoAnimationControllerOwner];
}

#pragma mark -
#pragma mark ToolbarOwner

- (ToolbarController*)relinquishedToolbarController {
  return [self.headerController relinquishedToolbarController];
}

- (void)reparentToolbarController {
  [self.headerController reparentToolbarController];
}

- (CGFloat)toolbarHeight {
  // If the google landing controller is nil, there is no toolbar visible in the
  // native content view, finally there is no toolbar on iPad.
  return self.headerController && !IsIPadIdiom() ? kToolbarHeight : 0.0;
}

#pragma mark - NewTabPagePanelControllerDelegate

- (void)updateNtpBarShadowForPanelController:
    (id<NewTabPagePanelProtocol>)ntpPanelController {
  if (_currentController != ntpPanelController)
    return;
  [self.view.tabBar setShadowAlpha:[ntpPanelController alphaForBottomShadow]];
}

@end

@implementation NewTabPageController (TestSupport)

- (id<NewTabPagePanelProtocol>)currentController {
  return _currentController;
}

- (BookmarkHomeTabletNTPController*)bookmarkController {
  return _bookmarkController;
}

- (GoogleLandingViewController*)googleLandingController {
  return _googleLandingController;
}

- (id<NewTabPagePanelProtocol>)incognitoController {
  return _incognitoController;
}

@end
