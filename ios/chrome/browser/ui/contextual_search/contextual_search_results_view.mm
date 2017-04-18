// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/contextual_search/contextual_search_results_view.h"

#include <memory>

#import "base/ios/weak_nsobject.h"
#include "base/mac/scoped_nsobject.h"
#include "base/memory/ptr_util.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/tabs/legacy_tab_helper.h"
#import "ios/chrome/browser/tabs/tab.h"
#import "ios/chrome/browser/tabs/tab_helper_util.h"
#import "ios/chrome/browser/tabs/tab_private.h"
#import "ios/chrome/browser/ui/contextual_search/contextual_search_metrics.h"
#import "ios/chrome/browser/ui/contextual_search/contextual_search_panel_view.h"
#import "ios/chrome/browser/ui/contextual_search/contextual_search_web_state_observer.h"
#import "ios/chrome/browser/web_state_list/web_state_opener.h"
#import "ios/chrome/common/material_timing.h"
#include "ios/web/public/load_committed_details.h"
#import "ios/web/public/serializable_user_data_manager.h"
#import "ios/web/public/web_state/ui/crw_native_content_provider.h"
#import "ios/web/public/web_state/ui/crw_web_view_proxy.h"
#import "ios/web/public/web_state/ui/crw_web_view_scroll_view_proxy.h"
#import "ios/web/public/web_state/web_state.h"
#import "ios/web/web_state/ui/crw_web_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
enum SearchResultsViewVisibility { OFFSCREEN, PRELOAD, VISIBLE };
}

@interface ContextualSearchResultsView ()<ContextualSearchWebStateDelegate,
                                          CRWNativeContentProvider>

// Can the search results be scrolled currently?
@property(nonatomic, assign, getter=isScrollEnabled) BOOL scrollEnabled;
@end

@implementation ContextualSearchResultsView {
  std::unique_ptr<ContextualSearchWebStateObserver> _webStateObserver;

  // WebState that loads the search results.
  std::unique_ptr<web::WebState> _webState;
  std::unique_ptr<WebStateOpener> _webStateOpener;

  // Access to the search tab's web view proxy.
  id<CRWWebViewProxy> _webViewProxy;

  BOOL _loaded;
  BOOL _displayed;
  BOOL _loadingError;
  BOOL _waitingForInitialSearchTabLoad;
  BOOL _loadInProgress;
  BOOL _shouldScroll;

  BOOL _preloadEnabled;

  SearchResultsViewVisibility _visibility;

  // Time of starting a search results page load.
  base::Time _loadStartTime;
}

@synthesize active = _active;
@synthesize opener = _opener;
@synthesize promoter = _promoter;
@synthesize preloadChecker = _preloadChecker;

- (instancetype)initWithFrame:(CGRect)frame {
  if ((self = [super initWithFrame:frame])) {
    self.translatesAutoresizingMaskIntoConstraints = NO;
    self.backgroundColor = [UIColor whiteColor];
    self.accessibilityIdentifier = @"contextualSearchResultsView";
    self.clipsToBounds = YES;
    _webStateObserver.reset(new ContextualSearchWebStateObserver(self));
    _visibility = OFFSCREEN;
  }
  return self;
}

#pragma mark - private properties

- (BOOL)isScrollEnabled {
  return [[_webViewProxy scrollViewProxy] isScrollEnabled];
}

- (void)setScrollEnabled:(BOOL)scrollEnabled {
  [[_webViewProxy scrollViewProxy] setScrollEnabled:scrollEnabled];
  [[_webViewProxy scrollViewProxy] setBounces:NO];
}

#pragma mark - public properties

- (id<ContextualSearchTabPromoter>)promoter {
  return _promoter;
}

- (void)setPromoter:(id<ContextualSearchTabPromoter>)promoter {
  _promoter = promoter;
}

- (id<ContextualSearchPreloadChecker>)preloadChecker {
  return _preloadChecker;
}

- (void)setPreloadChecker:(id<ContextualSearchPreloadChecker>)preloadChecker {
  _preloadChecker = preloadChecker;
}

- (BOOL)contentVisible {
  return _loaded && _visibility == VISIBLE;
}

- (void)setActive:(BOOL)active {
  if (active) {
    if (_webState) {
      Tab* tab = LegacyTabHelper::GetTabForWebState(_webState.get());
      // Start watching the embedded Tab's web activity.
      _webStateObserver->ObserveWebState(_webState.get());
      _webState->SetShouldSuppressDialogs(false);

      _webViewProxy = [[tab webController] webViewProxy];
      [[_webViewProxy scrollViewProxy] setBounces:NO];
    }
  } else {
    // Stop watching the embedded Tab's web activity.
    _webStateObserver->ObserveWebState(nullptr);
    _webViewProxy = nil;
  }

  _active = active;
}

#pragma mark - public methods

- (void)scrollToTopAnimated:(BOOL)animated {
  [[_webViewProxy scrollViewProxy] setBounces:YES];
  // Scroll the search tab's view to the top.
  [[_webViewProxy scrollViewProxy] setContentOffset:CGPointZero
                                           animated:animated];
}

- (void)createTabForSearch:(GURL)url preloadEnabled:(BOOL)preloadEnabled {
  DCHECK(self.opener);
  if (_webState)
    [self cancelLoad];

  _webStateOpener = base::MakeUnique<WebStateOpener>(self.opener.webState);
  web::WebState::CreateParams createParams(self.opener.browserState);
  _webState = web::WebState::Create(createParams);

  AttachTabHelpers(_webState.get());
  Tab* tab = LegacyTabHelper::GetTabForWebState(_webState.get());
  DCHECK(tab);

  [[tab webController] setNativeProvider:self];
  _webState->SetWebUsageEnabled(true);
  [tab setIsLinkLoadingPrerenderTab:YES];

  web::NavigationManager::WebLoadParams loadParams(url);
  loadParams.transition_type = ui::PAGE_TRANSITION_FROM_ADDRESS_BAR;
  _webState->GetNavigationManager()->LoadURLWithParams(loadParams);

  // Don't actually start the page load yet -- that happens in -loadTab

  _preloadEnabled = preloadEnabled;
  [self loadPendingSearchIfPossible];
}

- (std::unique_ptr<web::WebState>)releaseWebState {
  [self disconnectTab];
  // Allow the search tab to be sized by autoresizing mask again.
  if (_webState) {
    Tab* tab = LegacyTabHelper::GetTabForWebState(_webState.get());
    [[tab view] setTranslatesAutoresizingMaskIntoConstraints:YES];
  }
  return std::move(_webState);
}

- (WebStateOpener)webStateOpener {
  DCHECK(_webStateOpener);
  return *_webStateOpener;
}

- (void)recordFinishedSearchChained:(BOOL)chained {
  base::TimeDelta duration = base::Time::Now() - _loadStartTime;
  ContextualSearch::RecordDuration(self.contentVisible, chained, duration);
}

#pragma mark - private methods

- (void)disconnectTab {
  if (_webState) {
    Tab* tab = LegacyTabHelper::GetTabForWebState(_webState.get());
    [[tab view] removeFromSuperview];
    [[tab webController] setNativeProvider:nil];
  }
  self.active = NO;
  _webViewProxy = nil;
}

- (void)cancelLoad {
  [self disconnectTab];
  _loadInProgress = NO;
  _loaded = NO;
  _webState.reset();
}

- (void)loadPendingSearchIfPossible {
  // If the search tab hasn't been created, or if it's already loaded, no-op.
  if (!_webState || _loadInProgress || self.active || _visibility == OFFSCREEN)
    return;

  // If this view is in a position where loading would be "preloading", check
  // if that's allowed.
  if (_visibility == PRELOAD &&
      !([_preloadChecker canPreloadSearchResults] && _preloadEnabled)) {
    return;
  }
  [self loadTab];
}

- (void)loadTab {
  DCHECK(_webState);
  // Start observing the search tab.
  self.active = YES;
  // TODO(crbug.com/546223): See if |_waitingForInitialSearchTabLoad| and
  // |_loadInProgress| can be consolidated.
  _waitingForInitialSearchTabLoad = YES;
  // Mark the start of the time for the search.
  _loadStartTime = base::Time::Now();
  // Start the load by asking for the tab's view, but making it hidden.
  Tab* tab = LegacyTabHelper::GetTabForWebState(_webState.get());
  [tab view].hidden = YES;
  [self addSubview:[tab view]];
  _loadInProgress = YES;
}

- (void)displayTab {
  DCHECK(_webState);
  Tab* tab = LegacyTabHelper::GetTabForWebState(_webState.get());
  [tab view].frame = self.bounds;
  _loadInProgress = NO;

  void (^insertTab)(void) = ^{
    [tab view].hidden = NO;
    self.scrollEnabled = _shouldScroll;
  };

  if (_visibility == VISIBLE) {
    // Fade it in quickly.
    UIViewAnimationOptions options =
        UIViewAnimationOptionTransitionCrossDissolve;
    [UIView cr_transitionWithView:self
                         duration:ios::material::kDuration2
                            curve:ios::material::CurveEaseInOut
                          options:options
                       animations:insertTab
                       completion:nil];
  } else {
    // Swap it in.
    insertTab();
  }
}

#pragma mark - ContextualSearchScrollSynchronizer

- (UIGestureRecognizer*)scrollRecognizer {
  if (_webViewProxy) {
    UIGestureRecognizer* recognizer =
        [[_webViewProxy scrollViewProxy] panGestureRecognizer];
    if ([recognizer isEnabled])
      return recognizer;
  }
  return nil;
}

- (BOOL)scrolled {
  if (!_webViewProxy)
    return NO;
  CGPoint offset = [[_webViewProxy scrollViewProxy] contentOffset];
  return !CGPointEqualToPoint(offset, CGPointZero);
}

#pragma mark - ContextualSearchWebStateDelegate methods

- (void)webState:(web::WebState*)webState
    navigatedWithDetails:(const web::LoadCommittedDetails&)details {
  if (_loaded) {
    if (self.active && !details.is_in_page && !_loadingError) {
      // Use async dispatch so the rest of the OnNavigationItemCommitted()
      // handlers can complete before the tab changes owners.
      dispatch_async(dispatch_get_main_queue(), ^{
        [_promoter promoteTabHeaderPressed:NO];
      });
    }
    _loadingError = NO;
  }
}

- (void)webState:(web::WebState*)webState
    pageLoadedWithStatus:(web::PageLoadCompletionStatus)loadStatus {
  if (!_loaded) {
    _loaded = YES;
    if (_waitingForInitialSearchTabLoad) {
      _waitingForInitialSearchTabLoad = NO;
      [self displayTab];
    }
  }
}

#pragma mark - ContextualSearchPanelMotionObserver

- (void)panel:(ContextualSearchPanelView*)panel
    didChangeToState:(ContextualSearch::PanelState)toState
           fromState:(ContextualSearch::PanelState)fromState {
  // Update visibility.
  switch (toState) {
    case ContextualSearch::DISMISSED:
      _visibility = OFFSCREEN;
      break;
    case ContextualSearch::PEEKING:
      _visibility = PRELOAD;
      break;
    default:
      _visibility = VISIBLE;
      break;
  }

  // Load any pending search if now visible.
  if (_visibility != OFFSCREEN) {
    [self loadPendingSearchIfPossible];
  }

  // If the drag takes the panel down from covering, and the search tab's
  // scrolling is enabled, then disable it and reset any scrolling.
  if (toState <= ContextualSearch::PREVIEWING &&
      fromState == ContextualSearch::COVERING && self.isScrollEnabled) {
    self.scrollEnabled = NO;
    [self scrollToTopAnimated:YES];
    _shouldScroll = NO;
  } else if (toState == ContextualSearch::COVERING) {
    self.scrollEnabled = YES;
    _shouldScroll = YES;
  }
}

- (void)panelWillPromote:(ContextualSearchPanelView*)panel {
  [panel removeMotionObserver:self];
}

#pragma mark - CRWNativeContentProvider methods

// Native pages should never be loaded this way.
- (BOOL)hasControllerForURL:(const GURL&)url {
  NOTREACHED();
  [self cancelLoad];
  return NO;
}

// Native pages should never be loaded this way.
- (id<CRWNativeContent>)controllerForURL:(const GURL&)url
                                webState:(web::WebState*)webState {
  NOTREACHED();
  [self cancelLoad];
  return nil;
}

- (id<CRWNativeContent>)controllerForURL:(const GURL&)url
                               withError:(NSError*)error
                                  isPost:(BOOL)isPost {
  // Display the error page in the contextual search tab.
  _loadingError = YES;
  [self displayTab];

  [[_webViewProxy scrollViewProxy] setScrollEnabled:NO];
  id errorController =
      [self.opener.webController.nativeProvider controllerForURL:url
                                                       withError:error
                                                          isPost:isPost];

  if ([errorController respondsToSelector:@selector(setScrollEnabled:)]) {
    [errorController setScrollEnabled:NO];
  }
  return errorController;
}

@end
