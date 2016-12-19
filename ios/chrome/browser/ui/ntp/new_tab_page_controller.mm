// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/ntp/new_tab_page_controller.h"

#import <QuartzCore/QuartzCore.h>

#import "base/ios/weak_nsobject.h"
#include "base/logging.h"
#include "base/mac/objc_property_releaser.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/template_url_service.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync_sessions/synced_session.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/pref_names.h"
#include "ios/chrome/browser/search_engines/template_url_service_factory.h"
#include "ios/chrome/browser/sync/sync_setup_service.h"
#include "ios/chrome/browser/sync/sync_setup_service_factory.h"
#import "ios/chrome/browser/tabs/tab_model.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_controller_factory.h"
#import "ios/chrome/browser/ui/commands/UIKit+ChromeExecuteCommand.h"
#import "ios/chrome/browser/ui/commands/generic_chrome_command.h"
#include "ios/chrome/browser/ui/commands/ios_command_ids.h"
#import "ios/chrome/browser/ui/ntp/google_landing_controller.h"
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
    return NewTabPage::kMostVisitedPanel;
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
    case NewTabPage::kMostVisitedPanel:
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
  ios::ChromeBrowserState* browserState_;                   // Weak.
  id<UrlLoader> loader_;                                    // Weak.
  id<CRWSwipeRecognizerProvider> swipeRecognizerProvider_;  // Weak.
  id<NewTabPageControllerObserver> newTabPageObserver_;     // Weak.

  NewTabPageView* newTabPageView_;

  base::scoped_nsobject<RecentTabsPanelController> openTabsController_;
  // Has the scrollView been initialized.
  BOOL scrollInitialized_;

  // Dominant color cache. Key: (NSString*)url, val: (UIColor*)dominantColor.
  NSMutableDictionary* dominantColorCache_;  // Weak, owned by bvc.

  // Delegate to focus and blur the omnibox.
  base::WeakNSProtocol<id<OmniboxFocuser>> focuser_;

  // Delegate to fetch the ToolbarModel and current web state from.
  base::WeakNSProtocol<id<WebToolbarDelegate>> webToolbarDelegate_;

  base::scoped_nsobject<TabModel> tabModel_;

  base::mac::ObjCPropertyReleaser propertyReleaser_NewTabPageController_;
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

@property(nonatomic, retain) NewTabPageView* ntpView;
@end

@implementation NewTabPageController

@synthesize ntpView = newTabPageView_;
@synthesize swipeRecognizerProvider = swipeRecognizerProvider_;

- (id)initWithUrl:(const GURL&)url
                loader:(id<UrlLoader>)loader
               focuser:(id<OmniboxFocuser>)focuser
           ntpObserver:(id<NewTabPageControllerObserver>)ntpObserver
          browserState:(ios::ChromeBrowserState*)browserState
            colorCache:(NSMutableDictionary*)colorCache
    webToolbarDelegate:(id<WebToolbarDelegate>)webToolbarDelegate
              tabModel:(TabModel*)tabModel {
  self = [super initWithNibName:nil url:url];
  if (self) {
    DCHECK(browserState);
    propertyReleaser_NewTabPageController_.Init(self,
                                                [NewTabPageController class]);
    browserState_ = browserState;
    loader_ = loader;
    newTabPageObserver_ = ntpObserver;
    focuser_.reset(focuser);
    webToolbarDelegate_.reset(webToolbarDelegate);
    tabModel_.reset([tabModel retain]);
    dominantColorCache_ = colorCache;
    self.title = l10n_util::GetNSString(IDS_NEW_TAB_TITLE);
    scrollInitialized_ = NO;

    base::scoped_nsobject<UIScrollView> scrollView(
        [[UIScrollView alloc] initWithFrame:CGRectMake(0, 0, 320, 412)]);
    [scrollView setAutoresizingMask:(UIViewAutoresizingFlexibleWidth |
                                     UIViewAutoresizingFlexibleHeight)];
    base::scoped_nsobject<NewTabPageBar> tabBar(
        [[NewTabPageBar alloc] initWithFrame:CGRectMake(0, 412, 320, 48)]);
    newTabPageView_ =
        [[NewTabPageView alloc] initWithFrame:CGRectMake(0, 0, 320, 460)
                                andScrollView:scrollView
                                    andTabBar:tabBar];
    // TODO(crbug.com/607113): Merge view and ntpView.
    self.view = newTabPageView_;
    [tabBar setDelegate:self];

    bool isIncognito = browserState_->IsOffTheRecord();

    NSString* incognito = l10n_util::GetNSString(IDS_IOS_NEW_TAB_INCOGNITO);
    NSString* mostVisited =
        l10n_util::GetNSString(IDS_IOS_NEW_TAB_MOST_VISITED);
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
      NewTabPageBarItem* mostVisitedItem = [NewTabPageBarItem
          newTabPageBarItemWithTitle:mostVisited
                          identifier:NewTabPage::kMostVisitedPanel
                               image:[UIImage imageNamed:@"ntp_mv_search"]];
      NewTabPageBarItem* bookmarksItem = [NewTabPageBarItem
          newTabPageBarItemWithTitle:bookmarks
                          identifier:NewTabPage::kBookmarksPanel
                               image:[UIImage imageNamed:@"ntp_bookmarks"]];
      [tabBarItems addObject:bookmarksItem];
      if (IsIPadIdiom()) {
        [tabBarItems addObject:mostVisitedItem];
      }

      NewTabPageBarItem* openTabsItem = [NewTabPageBarItem
          newTabPageBarItemWithTitle:openTabs
                          identifier:NewTabPage::kOpenTabsPanel
                               image:[UIImage imageNamed:@"ntp_opentabs"]];
      [tabBarItems addObject:openTabsItem];
      self.ntpView.tabBar.items = tabBarItems;

      if (!IsIPadIdiom()) {
        itemToDisplay = mostVisitedItem;
      } else {
        PrefService* prefs = browserState_->GetPrefs();
        int shownPage = prefs->GetInteger(prefs::kNtpShownPage);
        shownPage = shownPage & ~INDEX_MASK;

        if (shownPage == BOOKMARKS_PAGE_ID) {
          itemToDisplay = bookmarksItem;
        } else if (shownPage == OPEN_TABS_PAGE_ID) {
          itemToDisplay = openTabsItem;
        } else {
          itemToDisplay = mostVisitedItem;
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
  [googleLandingController_ setDelegate:nil];
  [bookmarkController_ setDelegate:nil];
  [openTabsController_ setDelegate:nil];
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [super dealloc];
}

#pragma mark - CRWNativeContent

// Note: No point implementing -handleLowMemory because all native content
// views but the selected one are dropped, and the selected view doesn't
// need to do anything.

- (void)reload {
  [currentController_ reload];
  [super reload];
}

- (void)wasShown {
  [currentController_ wasShown];
  // Ensure that the NTP has the latest data when it is shown.
  [self reload];
  [self.ntpView.tabBar updateColorsForScrollView:self.ntpView.scrollView];
  [self.ntpView.tabBar
      setShadowAlpha:[currentController_ alphaForBottomShadow]];
}

- (void)wasHidden {
  [currentController_ wasHidden];
}

- (BOOL)wantsKeyboardShield {
  return [self selectedPanelID] != NewTabPage::kMostVisitedPanel;
}

- (BOOL)wantsLocationBarHintText {
  // Always show hint text on iPhone.
  if (!IsIPadIdiom())
    return YES;
  // Always show the location bar hint text if the search engine is not Google.
  TemplateURLService* service =
      ios::TemplateURLServiceFactory::GetForBrowserState(browserState_);
  if (service) {
    TemplateURL* defaultURL = service->GetDefaultSearchProvider();
    if (defaultURL &&
        defaultURL->GetEngineType(service->search_terms_data()) !=
            SEARCH_ENGINE_GOOGLE) {
      return YES;
    }
  }

  // Always return true when incognito.
  if (browserState_->IsOffTheRecord())
    return YES;

  return [self selectedPanelID] != NewTabPage::kMostVisitedPanel;
}

- (void)dismissKeyboard {
  [currentController_ dismissKeyboard];
}

- (void)dismissModals {
  [currentController_ dismissModals];
}

- (void)willUpdateSnapshot {
  if ([currentController_ respondsToSelector:@selector(willUpdateSnapshot)]) {
    [currentController_ willUpdateSnapshot];
  }
}

#pragma mark -

- (void)setSwipeRecognizerProvider:(id<CRWSwipeRecognizerProvider>)provider {
  swipeRecognizerProvider_ = provider;
  NSSet* recognizers = [swipeRecognizerProvider_ swipeRecognizers];
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

  scrollInitialized_ = YES;
}

- (void)disableScroll {
  [self.ntpView.scrollView setScrollEnabled:NO];
}

- (void)enableScroll {
  [self.ntpView.scrollView setScrollEnabled:YES];
}

// Update selectedIndex and scroll position as the scroll view moves.
- (void)scrollViewDidScroll:(UIScrollView*)scrollView {
  if (!scrollInitialized_)
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
  // jank on first creation, but it doesn't seem very noticable.  The trade off
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

  [newTabPageObserver_ selectedPanelDidChange];
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
    else if (item.identifier == NewTabPage::kMostVisitedPanel)
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
  UIView* view;
  BOOL created = NO;
  // Only load the controllers once.
  if (item.identifier == NewTabPage::kBookmarksPanel) {
    if (!bookmarkController_) {
      base::scoped_nsobject<BookmarkControllerFactory> factory(
          [[BookmarkControllerFactory alloc] init]);
      bookmarkController_.reset([[factory
          bookmarkPanelControllerForBrowserState:browserState_
                                          loader:loader_
                                      colorCache:dominantColorCache_] retain]);
    }
    view = [bookmarkController_ view];
    [bookmarkController_ setDelegate:self];
  } else if (item.identifier == NewTabPage::kMostVisitedPanel) {
    if (!googleLandingController_) {
      googleLandingController_.reset([[GoogleLandingController alloc]
              initWithLoader:loader_
                browserState:browserState_
                     focuser:focuser_
          webToolbarDelegate:webToolbarDelegate_
                    tabModel:tabModel_]);
    }
    view = [googleLandingController_ view];
    [googleLandingController_ setDelegate:self];
  } else if (item.identifier == NewTabPage::kOpenTabsPanel) {
    if (!openTabsController_)
      openTabsController_.reset([[RecentTabsPanelController alloc]
          initWithLoader:loader_
            browserState:browserState_]);
    view = [openTabsController_ view];
    [openTabsController_ setDelegate:self];
  } else if (item.identifier == NewTabPage::kIncognitoPanel) {
    if (!incognitoController_)
      incognitoController_.reset([[IncognitoPanelController alloc]
              initWithLoader:loader_
                browserState:browserState_
          webToolbarDelegate:webToolbarDelegate_]);
    view = [incognitoController_ view];
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
    [self.ntpView.scrollView addSubview:view];
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
      base::scoped_nsobject<GenericChromeCommand> command(
          [[GenericChromeCommand alloc] initWithTag:IDC_SHOW_BOOKMARK_MANAGER]);
      [self.ntpView chromeExecuteCommand:command];
    } else if (item.identifier == NewTabPage::kOpenTabsPanel) {
      base::scoped_nsobject<GenericChromeCommand> command(
          [[GenericChromeCommand alloc] initWithTag:IDC_SHOW_OTHER_DEVICES]);
      [self.ntpView chromeExecuteCommand:command];
    }
  }

  if (currentController_ == nil) {
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
  return NewTabPage::kMostVisitedPanel;
}

- (void)updateCurrentController:(NewTabPageBarItem*)item
                          index:(NSUInteger)index {
  if (!IsIPadIdiom() && (item.identifier == NewTabPage::kBookmarksPanel ||
                         item.identifier == NewTabPage::kOpenTabsPanel)) {
    // Don't update |currentController_| for iPhone since Bookmarks and Recent
    // Tabs are presented in a modal view controller.
    return;
  }

  id<NewTabPagePanelProtocol> oldController = currentController_;
  self.ntpView.tabBar.selectedIndex = index;
  if (item.identifier == NewTabPage::kBookmarksPanel)
    currentController_ = bookmarkController_.get();
  else if (item.identifier == NewTabPage::kMostVisitedPanel)
    currentController_ = googleLandingController_.get();
  else if (item.identifier == NewTabPage::kOpenTabsPanel)
    currentController_ = openTabsController_.get();
  else if (item.identifier == NewTabPage::kIncognitoPanel)
    currentController_ = incognitoController_.get();

  [bookmarkController_
      setScrollsToTop:(currentController_ == bookmarkController_.get())];
  [googleLandingController_
      setScrollsToTop:(currentController_ == googleLandingController_.get())];
  [openTabsController_
      setScrollsToTop:(currentController_ == openTabsController_.get())];
  [self.ntpView.tabBar
      setShadowAlpha:[currentController_ alphaForBottomShadow]];

  if (oldController != currentController_) {
    [currentController_ wasShown];
    [oldController wasHidden];
  }
}

- (void)panelChanged:(NewTabPageBarItem*)item {
  if (browserState_->IsOffTheRecord())
    return;

  // Save state and update metrics. Intentionally omitting a metric for the
  // Incognito panel.
  PrefService* prefs = browserState_->GetPrefs();
  if (item.identifier == NewTabPage::kBookmarksPanel) {
    base::RecordAction(UserMetricsAction("MobileNTPSwitchToBookmarks"));
    prefs->SetInteger(prefs::kNtpShownPage, BOOKMARKS_PAGE_ID);
  } else if (item.identifier == NewTabPage::kMostVisitedPanel) {
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
  return [googleLandingController_ logoAnimationControllerOwner];
}

#pragma mark -
#pragma mark ToolbarOwner

- (ToolbarController*)relinquishedToolbarController {
  return [googleLandingController_ relinquishedToolbarController];
}

- (void)reparentToolbarController {
  [googleLandingController_ reparentToolbarController];
}

- (CGFloat)toolbarHeight {
  // If the google landing controller is nil, there is no toolbar visible in the
  // native content view, finally there is no toolbar on iPad.
  return googleLandingController_ && !IsIPadIdiom() ? kToolbarHeight : 0.0;
}

#pragma mark - NewTabPagePanelControllerDelegate

- (void)updateNtpBarShadowForPanelController:
    (id<NewTabPagePanelProtocol>)ntpPanelController {
  if (currentController_ != ntpPanelController)
    return;
  [self.ntpView.tabBar
      setShadowAlpha:[ntpPanelController alphaForBottomShadow]];
}

@end
