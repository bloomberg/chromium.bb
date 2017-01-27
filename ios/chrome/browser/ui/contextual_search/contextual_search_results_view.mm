// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/contextual_search/contextual_search_results_view.h"

#include <memory>

#import "base/ios/weak_nsobject.h"
#include "base/mac/scoped_nsobject.h"
#import "ios/chrome/browser/tabs/tab.h"
#import "ios/chrome/browser/ui/contextual_search/contextual_search_metrics.h"
#import "ios/chrome/browser/ui/contextual_search/contextual_search_panel_view.h"
#import "ios/chrome/browser/ui/contextual_search/contextual_search_web_state_observer.h"
#import "ios/chrome/common/material_timing.h"
#include "ios/web/public/load_committed_details.h"
#import "ios/web/public/web_state/crw_web_view_proxy.h"
#import "ios/web/public/web_state/crw_web_view_scroll_view_proxy.h"
#import "ios/web/public/web_state/ui/crw_native_content_provider.h"
#import "ios/web/web_state/ui/crw_web_controller.h"

namespace {
enum SearchResultsViewVisibility { OFFSCREEN, PRELOAD, VISIBLE };
}

@interface ContextualSearchResultsView ()<ContextualSearchWebStateDelegate,
                                          CRWNativeContentProvider>

// Can the search results be scrolled currently?
@property(nonatomic, assign, getter=isScrollEnabled) BOOL scrollEnabled;
@end

@implementation ContextualSearchResultsView {
  base::WeakNSProtocol<id<ContextualSearchTabPromoter>> _promoter;
  base::WeakNSProtocol<id<ContextualSearchPreloadChecker>> _preloadChecker;
  std::unique_ptr<ContextualSearchWebStateObserver> _webStateObserver;

  // Tab that loads the search results.
  base::scoped_nsobject<Tab> _tab;

  // Access to the search tab's web view proxy.
  base::scoped_nsprotocol<id<CRWWebViewProxy>> _webViewProxy;

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
  _promoter.reset(promoter);
}

- (id<ContextualSearchPreloadChecker>)preloadChecker {
  return _preloadChecker;
}

- (void)setPreloadChecker:(id<ContextualSearchPreloadChecker>)preloadChecker {
  _preloadChecker.reset(preloadChecker);
}

- (BOOL)contentVisible {
  return _loaded && _visibility == VISIBLE;
}

- (void)setActive:(BOOL)active {
  if (active) {
    // Start watching the embedded Tab's web activity.
    _webStateObserver->ObserveWebState([_tab webState]);
    [[_tab webController] setShouldSuppressDialogs:NO];
    _webViewProxy.reset([[[_tab webController] webViewProxy] retain]);
    [[_webViewProxy scrollViewProxy] setBounces:NO];
  } else {
    // Stop watching the embedded Tab's web activity.
    _webStateObserver->ObserveWebState(nullptr);
    _webViewProxy.reset(nil);
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
  if (_tab)
    [self cancelLoad];

  void (^searchTabConfiguration)(Tab*) = ^(Tab* tab) {
    [tab setIsLinkLoadingPrerenderTab:YES];
    [[tab webController] setDelegate:tab];
  };

  ui::PageTransition transition = ui::PAGE_TRANSITION_FROM_ADDRESS_BAR;
  Tab* tab = [Tab newPreloadingTabWithBrowserState:self.opener.browserState
                                               url:url
                                          referrer:web::Referrer()
                                        transition:transition
                                          provider:self
                                            opener:self.opener
                                  desktopUserAgent:false
                                     configuration:searchTabConfiguration];
  _tab.reset([tab retain]);
  // Don't actually start the page load yet -- that happens in -loadTab

  _preloadEnabled = preloadEnabled;
  [self loadPendingSearchIfPossible];
}

- (Tab*)releaseTab {
  [self disconnectTab];
  // Allow the search tab to be sized by autoresizing mask again.
  [[_tab view] setTranslatesAutoresizingMaskIntoConstraints:YES];
  return [_tab.release() autorelease];
}

- (void)recordFinishedSearchChained:(BOOL)chained {
  base::TimeDelta duration = base::Time::Now() - _loadStartTime;
  ContextualSearch::RecordDuration(self.contentVisible, chained, duration);
}

#pragma mark - private methods

- (void)disconnectTab {
  [[_tab view] removeFromSuperview];
  [[_tab webController] setNativeProvider:nil];
  self.active = NO;
  _webViewProxy.reset();
}

- (void)cancelLoad {
  [self disconnectTab];
  _loadInProgress = NO;
  _loaded = NO;
  [_tab close];
  _tab.reset();
}

- (void)loadPendingSearchIfPossible {
  // If the search tab hasn't been created, or if it's already loaded, no-op.
  if (!_tab.get() || _loadInProgress || self.active || _visibility == OFFSCREEN)
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
  DCHECK(_tab.get());
  // Start observing the search tab.
  self.active = YES;
  // TODO(crbug.com/546223): See if |_waitingForInitialSearchTabLoad| and
  // |_loadInProgress| can be consolidated.
  _waitingForInitialSearchTabLoad = YES;
  // Mark the start of the time for the search.
  _loadStartTime = base::Time::Now();
  // Start the load by asking for the tab's view, but making it hidden.
  [_tab view].hidden = YES;
  [self addSubview:[_tab view]];
  _loadInProgress = YES;
}

- (void)displayTab {
  [_tab view].frame = self.bounds;
  _loadInProgress = NO;

  void (^insertTab)(void) = ^{
    [_tab view].hidden = NO;
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
