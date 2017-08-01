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
#import "ios/chrome/browser/ui/commands/UIKit+ChromeExecuteCommand.h"
#import "ios/chrome/browser/ui/commands/generic_chrome_command.h"
#include "ios/chrome/browser/ui/commands/ios_command_ids.h"
#import "ios/chrome/browser/ui/ntp/google_landing_mediator.h"
#import "ios/chrome/browser/ui/ntp/google_landing_view_controller.h"
#import "ios/chrome/browser/ui/ntp/incognito_panel_controller.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_bar_item.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_view.h"
#import "ios/chrome/browser/ui/ntp/recent_tabs/recent_tabs_panel_controller.h"
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
// Defaults to NewTabPage::kNone if the fragment is nil or not recognized.
// The strings checked by this function matches the set of fragments
// supported by chrome://newtab/# on Android.
// See chrome/browser/resources/mobile_ntp/mobile_ntp.js
PanelIdentifier IdentifierFromFragment(const std::string& fragment) {
  if (fragment == kMostVisitedFragment)
    return NewTabPage::kHomePanel;
  else if (fragment == kBookmarksFragment)
    return NewTabPage::kBookmarksPanel;
  else if (fragment == kOpenTabsFragment)
    return NewTabPage::kOpenTabsPanel;
  else if (fragment == kIncognitoFragment)
    return NewTabPage::kIncognitoPanel;
  else
    return NewTabPage::kNone;
}

// Converts from a NewTabPage::PanelIdentifier to a URL #fragment string.
// Defaults to nil if the fragment is kNone or not recognized.
std::string FragmentFromIdentifier(PanelIdentifier panel) {
  switch (panel) {
    case NewTabPage::kNone:
      return "";
    case NewTabPage::kHomePanel:
      return kMostVisitedFragment;
    case NewTabPage::kBookmarksPanel:
      return kBookmarksFragment;
    case NewTabPage::kOpenTabsPanel:
      return kOpenTabsFragment;
    case NewTabPage::kIncognitoPanel:
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
  id<NewTabPagePanelProtocol> _incognitoController;
  // The currently visible controller, one of the above.
  __weak id<NewTabPagePanelProtocol> _currentController;

  GoogleLandingMediator* _googleLandingMediator;

  RecentTabsPanelController* _openTabsController;
  // Has the scrollView been initialized.
  BOOL _scrollInitialized;

  // Dominant color cache. Key: (NSString*)url, val: (UIColor*)dominantColor.
  __weak NSMutableDictionary* _dominantColorCache;  // Owned by bvc.

  // Delegate to focus and blur the omnibox.
  __weak id<OmniboxFocuser> _focuser;

  // Delegate to fetch the ToolbarModel and current web state from.
  __weak id<WebToolbarDelegate> _webToolbarDelegate;

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
- (NewTabPage::PanelIdentifier)selectedPanelID;

@property(nonatomic, strong) NewTabPageView* ntpView;

// To ease modernizing the NTP only the internal panels are being converted
// to UIViewControllers.  This means all the plumbing between the
// BrowserViewController and the internal NTP panels (WebController, NTP)
// hierarchy is skipped.  While normally the logic to push and pop a view
// controller would be owned by a coordinator, in this case the old NTP
// controller adds and removes child view controllers itself when a load
// is initiated, and when WebController calls -willBeDismissed.
@property(nonatomic, weak) UIViewController* parentViewController;

// To ease modernizing the NTP a non-descript CommandDispatcher is passed
// through to be used by the reuabled NTP panels.
@property(nonatomic, weak) id dispatcher;

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

@synthesize ntpView = _ntpView;
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
      webToolbarDelegate:(id<WebToolbarDelegate>)webToolbarDelegate
                tabModel:(TabModel*)tabModel
    parentViewController:(UIViewController*)parentViewController
              dispatcher:(id)dispatcher {
  self = [super initWithNibName:nil url:url];
  if (self) {
    DCHECK(browserState);
    _browserState = browserState;
    _loader = loader;
    _newTabPageObserver = ntpObserver;
    _parentViewController = parentViewController;
    _dispatcher = dispatcher;
    _focuser = focuser;
    _webToolbarDelegate = webToolbarDelegate;
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
    _ntpView = [[NewTabPageView alloc] initWithFrame:CGRectMake(0, 0, 320, 460)
                                       andScrollView:scrollView
                                           andTabBar:tabBar];
    // TODO(crbug.com/607113): Merge view and ntpView.
    self.view = _ntpView;
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
                          identifier:NewTabPage::kIncognitoPanel
                               image:[UIImage imageNamed:@"ntp_incognito"]];
      if (IsIPadIdiom()) {
        // Only add the bookmarks tab item for Incognito.
        NewTabPageBarItem* bookmarksItem = [NewTabPageBarItem
            newTabPageBarItemWithTitle:bookmarks
                            identifier:NewTabPage::kBookmarksPanel
                                 image:[UIImage imageNamed:@"ntp_bookmarks"]];
        [tabBarItems addObject:bookmarksItem];
        [tabBarItems addObject:incognitoItem];
        self.ntpView.tabBar.items = tabBarItems;
      }
      itemToDisplay = incognitoItem;
    } else {
      NewTabPageBarItem* homeItem = [NewTabPageBarItem
          newTabPageBarItemWithTitle:home
                          identifier:NewTabPage::kHomePanel
                               image:[UIImage imageNamed:@"ntp_mv_search"]];
      NewTabPageBarItem* bookmarksItem = [NewTabPageBarItem
          newTabPageBarItemWithTitle:bookmarks
                          identifier:NewTabPage::kBookmarksPanel
                               image:[UIImage imageNamed:@"ntp_bookmarks"]];
      [tabBarItems addObject:bookmarksItem];
      if (IsIPadIdiom()) {
        [tabBarItems addObject:homeItem];
      }

      NewTabPageBarItem* openTabsItem = [NewTabPageBarItem
          newTabPageBarItemWithTitle:openTabs
                          identifier:NewTabPage::kOpenTabsPanel
                               image:[UIImage imageNamed:@"ntp_opentabs"]];
      [tabBarItems addObject:openTabsItem];
      self.ntpView.tabBar.items = tabBarItems;

      if (!IsIPadIdiom()) {
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
  self.ntpView.scrollView.delegate = nil;

  [_googleLandingMediator shutdown];

  // This is not an ideal place to put view controller contaimnent, rather a
  // //web -wasDismissed method on CRWNativeContent would be more accurate. If
  // CRWNativeContent leaks, this will not be called.
  // TODO(crbug.com/708319): Also call -removeFromParentViewController for
  // open tabs and incognito here.
  [_googleLandingController removeFromParentViewController];
  [_bookmarkController removeFromParentViewController];
  [[self.contentSuggestionsCoordinator viewController]
      removeFromParentViewController];

  [self.contentSuggestionsCoordinator stop];

  [self.homePanel setDelegate:nil];
  [_bookmarkController setDelegate:nil];
  [_openTabsController setDelegate:nil];
  [[NSNotificationCenter defaultCenter] removeObserver:self];
}

#pragma mark - CRWNativeContent

- (void)willBeDismissed {
  // This methods is called by //web immediately before |self|'s view is removed
  // from the view hierarchy, making it an ideal spot to intiate view controller
  // containment methods.
  // TODO(crbug.com/708319): Also call -willMoveToParentViewController:nil for
  // open tabs and incognito here.
  [_googleLandingController willMoveToParentViewController:nil];
  [_bookmarkController willMoveToParentViewController:nil];
  [[self.contentSuggestionsCoordinator viewController]
      willMoveToParentViewController:nil];
}

- (void)reload {
  [_currentController reload];
  [super reload];
}

- (void)wasShown {
  [_currentController wasShown];
  // Ensure that the NTP has the latest data when it is shown.
  [self reload];
  [self.ntpView.tabBar updateColorsForScrollView:self.ntpView.scrollView];
  [self.ntpView.tabBar
      setShadowAlpha:[_currentController alphaForBottomShadow]];
}

- (void)wasHidden {
  [_currentController wasHidden];
}

- (BOOL)wantsKeyboardShield {
  return [self selectedPanelID] != NewTabPage::kHomePanel;
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

  return [self selectedPanelID] != NewTabPage::kHomePanel;
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

#pragma mark -

- (void)setSwipeRecognizerProvider:(id<CRWSwipeRecognizerProvider>)provider {
  _swipeRecognizerProvider = provider;
  NSSet* recognizers = [_swipeRecognizerProvider swipeRecognizers];
  for (UISwipeGestureRecognizer* swipeRecognizer in recognizers) {
    [self.ntpView.scrollView.panGestureRecognizer
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

  UIScrollView* scrollView = self.ntpView.scrollView;
  scrollView.pagingEnabled = YES;
  scrollView.showsHorizontalScrollIndicator = NO;
  scrollView.showsVerticalScrollIndicator = NO;
  scrollView.contentMode = UIViewContentModeScaleAspectFit;
  scrollView.bounces = YES;
  scrollView.delegate = self;
  scrollView.scrollsToTop = NO;

  [self.ntpView updateScrollViewContentSize];
  [self.ntpView.tabBar updateColorsForScrollView:scrollView];

  _scrollInitialized = YES;
}

- (void)disableScroll {
  [self.ntpView.scrollView setScrollEnabled:NO];
}

- (void)enableScroll {
  [self.ntpView.scrollView setScrollEnabled:YES];
}

// Update selectedIndex and scroll position as the scroll view moves.
- (void)scrollViewDidScroll:(UIScrollView*)scrollView {
  if (!_scrollInitialized)
    return;

  // Position is used to track the exact X position of the scroll view, whereas
  // index is rounded to the panel that is most visible.
  CGFloat panelWidth =
      scrollView.contentSize.width / self.ntpView.tabBar.items.count;
  LayoutOffset position =
      LeadingContentOffsetForScrollView(scrollView) / panelWidth;
  NSUInteger index = round(position);

  // |scrollView| can be out of range when the frame changes.
  if (index >= self.ntpView.tabBar.items.count)
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
  if (index == position && self.ntpView.tabBar.selectedIndex != index) {
    NewTabPageBarItem* item = [self.ntpView.tabBar.items objectAtIndex:index];
    DCHECK(item);
    self.ntpView.tabBar.selectedIndex = index;
    [self updateCurrentController:item index:index];
    [self newTabBarItemDidChange:item changePanel:NO];
  }

  [self.ntpView.tabBar updateColorsForScrollView:scrollView];
  [self updateOverlayScrollPosition];
}

- (void)scrollViewDidEndScrollingAnimation:(UIScrollView*)scrollView {
  NSUInteger index = self.ntpView.tabBar.selectedIndex;
  NewTabPageBarItem* item = [self.ntpView.tabBar.items objectAtIndex:index];
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

- (void)selectPanel:(NewTabPage::PanelIdentifier)panelType {
  for (NewTabPageBarItem* item in self.ntpView.tabBar.items) {
    if (item.identifier == panelType) {
      [self showPanel:item];
      return;  // Early return after finding the first match.
    }
  }
}

- (void)showPanel:(NewTabPageBarItem*)item {
  if ([self loadPanel:item]) {
    // Intentionally omitting a metric for the Incognito panel.
    if (item.identifier == NewTabPage::kBookmarksPanel)
      base::RecordAction(UserMetricsAction("MobileNTPShowBookmarks"));
    else if (item.identifier == NewTabPage::kHomePanel)
      base::RecordAction(UserMetricsAction("MobileNTPShowMostVisited"));
    else if (item.identifier == NewTabPage::kOpenTabsPanel)
      base::RecordAction(UserMetricsAction("MobileNTPShowOpenTabs"));
  }
  [self scrollToPanel:item animate:NO];
}

- (void)loadControllerWithIndex:(NSUInteger)index {
  if (index >= self.ntpView.tabBar.items.count)
    return;

  NewTabPageBarItem* item = [self.ntpView.tabBar.items objectAtIndex:index];
  [self loadPanel:item];
}

- (BOOL)loadPanel:(NewTabPageBarItem*)item {
  DCHECK(self.parentViewController);
  UIView* view = nil;
  UIViewController* panelController = nil;
  BOOL created = NO;
  // Only load the controllers once.
  if (item.identifier == NewTabPage::kBookmarksPanel) {
    if (!_bookmarkController) {
      BookmarkControllerFactory* factory =
          [[BookmarkControllerFactory alloc] init];
      _bookmarkController =
          [factory bookmarkPanelControllerForBrowserState:_browserState
                                                   loader:_loader
                                               colorCache:_dominantColorCache];
    }
    panelController = _bookmarkController;
    view = [_bookmarkController view];
    [_bookmarkController setDelegate:self];
  } else if (item.identifier == NewTabPage::kHomePanel) {
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
            initWithConsumer:_googleLandingController
                browserState:_browserState
                  dispatcher:self.dispatcher
                webStateList:[_tabModel webStateList]];
        [_googleLandingController setDataSource:_googleLandingMediator];
        self.headerController = _googleLandingController;
      }
      panelController = _googleLandingController;
      self.homePanel = _googleLandingController;
    }
    view = panelController.view;
    [self.homePanel setDelegate:self];
    [self.ntpView.tabBar setShadowAlpha:[self.homePanel alphaForBottomShadow]];
  } else if (item.identifier == NewTabPage::kOpenTabsPanel) {
    if (!_openTabsController)
      _openTabsController =
          [[RecentTabsPanelController alloc] initWithLoader:_loader
                                               browserState:_browserState];
    // TODO(crbug.com/708319): Also set panelController for opentabs here.
    view = [_openTabsController view];
    [_openTabsController setDelegate:self];
  } else if (item.identifier == NewTabPage::kIncognitoPanel) {
    if (!_incognitoController)
      _incognitoController =
          [[IncognitoPanelController alloc] initWithLoader:_loader
                                              browserState:_browserState
                                        webToolbarDelegate:_webToolbarDelegate];
    // TODO(crbug.com/708319): Also set panelController for incognito here.
    view = [_incognitoController view];
  } else {
    NOTREACHED();
    return NO;
  }

  // Add the panel views to the scroll view in the proper location.
  NSUInteger index = [self tabBarItemIndex:item];
  if (view.superview == nil) {
    created = YES;
    view.frame = [self.ntpView panelFrameForItemAtIndex:index];
    item.view = view;

    // To ease modernizing the NTP only the internal panels are being converted
    // to UIViewControllers.  This means all the plumbing between the
    // BrowserViewController and the internal NTP panels (WebController, NTP)
    // hierarchy is skipped.  While normally the logic to push and pop a view
    // controller would be owned by a coordinator, in this case the old NTP
    // controller adds and removes child view controllers itself when a load
    // is initiated, and when WebController calls -willBeDismissed.
    // TODO(crbug.com/708319):This 'if' can become a DCHECK once all panels move
    // to panelControllers.
    if (panelController)
      [self.parentViewController addChildViewController:panelController];
    [self.ntpView.scrollView addSubview:view];
    if (panelController)
      [panelController didMoveToParentViewController:self.parentViewController];
  }
  return created;
}

- (void)scrollToPanel:(NewTabPageBarItem*)item animate:(BOOL)animate {
  NSUInteger index = [self tabBarItemIndex:item];
  if (IsIPadIdiom()) {
    CGRect itemFrame = [self.ntpView panelFrameForItemAtIndex:index];
    CGPoint point = CGPointMake(CGRectGetMinX(itemFrame), 0);
    [self.ntpView.scrollView setContentOffset:point animated:animate];
  } else {
    if (item.identifier == NewTabPage::kBookmarksPanel) {
      GenericChromeCommand* command =
          [[GenericChromeCommand alloc] initWithTag:IDC_SHOW_BOOKMARK_MANAGER];
      [self.ntpView chromeExecuteCommand:command];
    } else if (item.identifier == NewTabPage::kOpenTabsPanel) {
      GenericChromeCommand* command =
          [[GenericChromeCommand alloc] initWithTag:IDC_SHOW_OTHER_DEVICES];
      [self.ntpView chromeExecuteCommand:command];
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
  if (IsIPadIdiom()) {
    index = [self.ntpView.tabBar.items indexOfObject:item];
    DCHECK(index != NSNotFound);
  }
  return index;
}

- (NewTabPage::PanelIdentifier)selectedPanelID {
  if (IsIPadIdiom()) {
    // |selectedIndex| isn't meaningful here with modal buttons on iPhone.
    NSUInteger index = self.ntpView.tabBar.selectedIndex;
    DCHECK(index != NSNotFound);
    NewTabPageBarItem* item = self.ntpView.tabBar.items[index];
    return static_cast<NewTabPage::PanelIdentifier>(item.identifier);
  }
  return NewTabPage::kHomePanel;
}

- (void)updateCurrentController:(NewTabPageBarItem*)item
                          index:(NSUInteger)index {
  if (!IsIPadIdiom() && (item.identifier == NewTabPage::kBookmarksPanel ||
                         item.identifier == NewTabPage::kOpenTabsPanel)) {
    // Don't update |_currentController| for iPhone since Bookmarks and Recent
    // Tabs are presented in a modal view controller.
    return;
  }

  id<NewTabPagePanelProtocol> oldController = _currentController;
  self.ntpView.tabBar.selectedIndex = index;
  if (item.identifier == NewTabPage::kBookmarksPanel)
    _currentController = _bookmarkController;
  else if (item.identifier == NewTabPage::kHomePanel)
    _currentController = self.homePanel;
  else if (item.identifier == NewTabPage::kOpenTabsPanel)
    _currentController = _openTabsController;
  else if (item.identifier == NewTabPage::kIncognitoPanel)
    _currentController = _incognitoController;

  [_bookmarkController
      setScrollsToTop:(_currentController == _bookmarkController)];
  [self.homePanel setScrollsToTop:(_currentController == self.homePanel)];
  [_openTabsController
      setScrollsToTop:(_currentController == _openTabsController)];
  if (oldController) {
    [self.ntpView.tabBar
        setShadowAlpha:[_currentController alphaForBottomShadow]];
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
  if (item.identifier == NewTabPage::kBookmarksPanel) {
    base::RecordAction(UserMetricsAction("MobileNTPSwitchToBookmarks"));
    prefs->SetInteger(prefs::kNtpShownPage, BOOKMARKS_PAGE_ID);
  } else if (item.identifier == NewTabPage::kHomePanel) {
    base::RecordAction(UserMetricsAction("MobileNTPSwitchToMostVisited"));
    prefs->SetInteger(prefs::kNtpShownPage, MOST_VISITED_PAGE_ID);
  } else if (item.identifier == NewTabPage::kOpenTabsPanel) {
    base::RecordAction(UserMetricsAction("MobileNTPSwitchToOpenTabs"));
    prefs->SetInteger(prefs::kNtpShownPage, OPEN_TABS_PAGE_ID);
  }
}

- (void)updateOverlayScrollPosition {
  // Update overlay position. This moves the overlay animation on the tab bar.
  UIScrollView* scrollView = self.ntpView.scrollView;
  if (!scrollView || scrollView.contentSize.width == 0.0)
    return;
  self.ntpView.tabBar.overlayPercentage =
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
  [self.ntpView.tabBar
      setShadowAlpha:[ntpPanelController alphaForBottomShadow]];
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
